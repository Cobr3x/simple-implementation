/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#include "auth_transport.h"
#include "crypto_hmac.h"
#include "prover.h"
#include "verifier.h"
#include "wire_vectors.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    auth_material_t m;
    uint32_t now;
    int fail_save;
    int fail_rng;
    int locked;
    uint8_t seed;
    auth_error_t error;
} mock_t;

static int load(void *u, auth_material_t *m) {
    *m = ((mock_t *)u)->m;
    return 1;
}

static int save(void *u, uint32_t c) {
    mock_t *m = u;
    if (m->fail_save)
        return 0;
    m->m.counter = c;
    return 1;
}

static int random_bytes(void *u, uint8_t *b, size_t n) {
    mock_t *m = u;
    if (m->fail_rng)
        return 0;
    while (n--)
        *b++ = ++m->seed;
    return 1;
}

static uint32_t now(void *u) { return ((mock_t *)u)->now; }

static uintptr_t enter(void *u) {
    ((mock_t *)u)->locked++;
    return 7;
}

static void leave(void *u, uintptr_t state) {
    assert(state == 7);
    ((mock_t *)u)->locked--;
}

static void report_error(void *u, auth_error_t error) { ((mock_t *)u)->error = error; }

static auth_platform_t platform(mock_t *m) {
    return (auth_platform_t){m, load, save, random_bytes, now, enter, leave, NULL, report_error};
}

static void setup(mock_t *m) {
    memset(m, 0, sizeof(*m));
    memset(m->m.k_auth, 0x0b, 32);
    memset(m->m.k_attest, 0x42, 32);
}

static void vectors(void) {
    const uint8_t expected[32] = {0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf,
                                  0xce, 0xaf, 0x0b, 0xf1, 0x2b, 0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83,
                                  0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7};
    uint8_t key[20];
    uint8_t out[32];
    memset(key, 0x0b, 20);
    assert(crypto_hmac_compute(key, 20, (const uint8_t *)"Hi There", 8, out));
    assert(memcmp(out, expected, 32) == 0);
    assert(!crypto_hmac_compute(NULL, 20, NULL, 0, out));
    assert(!crypto_hmac_verify(key, 20, NULL, 1, expected));
    assert(!crypto_hmac_verify(key, 20, NULL, 0, NULL));
    simple_msg_t msg = {.counter = 0x01020304};
    uint8_t wire[84];
    assert(simple_serialize_msg(&msg, wire, sizeof(wire)) == 84);
    assert(memcmp(wire, "\x01\x02\x03\x04", 4) == 0);
    for (size_t n = 0; n < 84; ++n)
        assert(!simple_parse_msg(wire, n, &msg));
    assert(!simple_parse_msg(wire, 85, &msg));
    assert(!simple_parse_msg(NULL, 84, &msg));
    assert(!simple_serialize_msg(NULL, wire, 84));
}

static void sessions(void) {
    mock_t pm;
    mock_t vm;
    setup(&pm);
    setup(&vm);
    uint8_t memory[128] = {1, 2, 3};
    attestation_range_t range = {memory, sizeof(memory)};
    assert(attestation_compute_valid_state(pm.m.k_attest, &range, 1, vm.m.reference_vs));
    auth_platform_t pp = platform(&pm);
    auth_platform_t vp = platform(&vm);
    prover_t p;
    verifier_t v;
    assert(prover_init(&p, &pp, &range, 1));
    assert(init_verifier(&v, &vp, 100));
    uint8_t req[84];
    uint8_t resp[33];
    uint8_t old[33];
    assert(verifier_challenge(&v, req, 84) == 84);
    assert(!memcmp(req, request_vector, 84));
    assert(verifier_challenge(&v, req, 84) == 0);
    for (size_t n = 0; n < 84; n++)
        assert(!prover_handle_request(&p, req, n, resp, 33));
    assert(pm.m.counter == 0);
    uint8_t changed[84];
    memcpy(changed, req, 84);
    changed[4] ^= 1;
    assert(!prover_handle_request(&p, changed, 84, resp, 33));
    assert(pm.error == AUTH_ERROR_REQUEST_AUTH);
    assert(pm.m.counter == 0);
    assert(prover_handle_request(&p, req, 84, resp, 33) == 33);
    assert(pm.locked == 0 && pm.m.counter == 1);
    assert(!memcmp(resp, response_vector, 33));
    memcpy(old, resp, 33);
    resp[32] ^= 1;
    assert(verify(&v, resp, 33) == VERIFY_INVALID && v.outstanding);
    resp[32] ^= 1;
    assert(verify(&v, resp, 33) == VERIFY_PASS);
    assert(verify(&v, resp, 33) == VERIFY_INVALID);
    assert(!prover_handle_request(&p, req, 84, resp, 33));
    assert(pm.error == AUTH_ERROR_STALE_COUNTER);
    assert(prover_init(&p, &pp, &range, 1));
    assert(!prover_handle_request(&p, req, 84, resp, 33));
    assert(verifier_challenge(&v, req, 84) == 84);
    assert(verify(&v, old, 33) == VERIFY_INVALID);
    memory[1] ^= 1;
    assert(prover_handle_request(&p, req, 84, resp, 33) == 33);
    assert(verify(&v, resp, 33) == VERIFY_MISMATCH);
    assert(verifier_challenge(&v, req, 84) == 84);
    vm.now = 100;
    assert(verifier_poll(&v) == VERIFY_TIMEOUT);
    assert(verify(&v, old, 33) == VERIFY_INVALID);
    vm.now = UINT32_MAX - 50;
    assert(verifier_challenge(&v, req, 84) == 84);
    vm.now = 49;
    assert(verifier_poll(&v) == VERIFY_TIMEOUT);
    vm.fail_rng = 1;
    uint32_t c = vm.m.counter;
    assert(!verifier_challenge(&v, req, 84));
    assert(c == vm.m.counter);
    vm.fail_rng = 0;
    vm.fail_save = 1;
    assert(!verifier_challenge(&v, req, 84));
    assert(!v.ready);
    vm.fail_save = 0;
    assert(init_verifier(&v, &vp, 100));
    assert(verifier_challenge(&v, req, 84));
    pm.fail_save = 1;
    assert(!prover_handle_request(&p, req, 84, resp, 33));
    assert(!p.ready);
    assert(pm.error == AUTH_ERROR_COUNTER_SAVE);
    verifier_cancel(&v);
    vm.m.counter = UINT32_MAX;
    assert(init_verifier(&v, &vp, 100));
    assert(!verifier_challenge(&v, req, 84));
    simple_report_t r;
    memset(resp, 0, 33);
    resp[0] = 2;
    assert(!simple_parse_report(resp, 33, &r));
    assert(!attestation_compute_valid_state(pm.m.k_attest, NULL, 0, resp));
}

static void nonce_binding(void) {
    mock_t pm;
    mock_t vm;
    setup(&pm);
    setup(&vm);
    uint8_t image[1] = {0};
    uint8_t req[84];
    uint8_t resp[33];
    attestation_range_t range = {image, 1};
    assert(attestation_compute_valid_state(pm.m.k_attest, &range, 1, vm.m.reference_vs));
    auth_platform_t pp = platform(&pm);
    auth_platform_t vp = platform(&vm);
    prover_t p;
    verifier_t v;
    assert(prover_init(&p, &pp, &range, 1));
    assert(init_verifier(&v, &vp, 100));
    assert(verifier_challenge(&v, req, 84));
    assert(prover_handle_request(&p, req, 84, resp, 33));
    v.nonce[0] ^= 1;
    assert(verify(&v, resp, 33) == VERIFY_INVALID);
    v.nonce[0] ^= 1;
    v.material.k_auth[0] ^= 1;
    assert(verify(&v, resp, 33) == VERIFY_INVALID);
    v.material.k_auth[0] ^= 1;
    vm.now = 100;
    assert(verify(&v, resp, 33) == VERIFY_TIMEOUT);
}

static void framing(void) {
    uint8_t payload[84];
    uint8_t wire[AUTH_FRAME_MAX];
    uint8_t decoded[84];
    uint8_t type;
    size_t len = 0;
    for (unsigned pattern = 0; pattern < 256; pattern++) {
        for (unsigned i = 0; i < 84; i++)
            payload[i] = (uint8_t)(pattern + i);
        size_t n = auth_frame_encode(AUTH_FRAME_REQUEST, payload, 84, wire, sizeof(wire));
        assert(n && n <= sizeof(wire));
        auth_receiver_t rx = {0};
        for (size_t i = 0; i < n; i++) {
            int r = auth_frame_feed(&rx, wire[i], 0, 100, &type, decoded, 84, &len);
            assert(r == (i == n - 1 ? 1 : 0));
        }
        assert(len == 84 && type == AUTH_FRAME_REQUEST && !memcmp(payload, decoded, 84));
    }
    uint8_t diagnostic = AUTH_ERROR_RDP;
    size_t diagnostic_n =
        auth_frame_encode(AUTH_FRAME_DIAGNOSTIC, &diagnostic, 1, wire, sizeof(wire));
    assert(diagnostic_n);
    auth_receiver_t diagnostic_rx = {0};
    for (size_t i = 0; i < diagnostic_n; i++) {
        int result =
            auth_frame_feed(&diagnostic_rx, wire[i], 0, 100, &type, decoded, sizeof(decoded), &len);
        assert(result == (i == diagnostic_n - 1 ? 1 : 0));
    }
    assert(type == AUTH_FRAME_DIAGNOSTIC && len == 1 && decoded[0] == AUTH_ERROR_RDP);
    auth_receiver_t rx = {0};
    for (unsigned i = 0; i < 200; i++)
        auth_frame_feed(&rx, 1, 0, 100, &type, decoded, 84, &len);
    assert(auth_frame_feed(&rx, 0, 0, 100, &type, decoded, 84, &len) == -1);
    auth_frame_feed(&rx, 4, 0, 100, &type, decoded, 84, &len);
    assert(auth_frame_feed(&rx, 0, 100, 100, &type, decoded, 84, &len) == -1);
    for (unsigned seed = 1; seed < 200; seed++) {
        uint32_t x = seed;
        for (unsigned i = 0; i < 500; i++) {
            x = x * 1664525u + 1013904223u;
            auth_frame_feed(&rx, (uint8_t)(x >> 24), i, 100, &type, decoded, 84, &len);
        }
        auth_frame_feed(&rx, 0, 500, 100, &type, decoded, 84, &len);
    }
}

static void measurement_failure(void) {
    mock_t pm;
    mock_t vm;
    setup(&pm);
    setup(&vm);
    auth_platform_t pp = platform(&pm);
    auth_platform_t vp = platform(&vm);
    attestation_range_t range = {NULL, 1};
    prover_t p;
    verifier_t v;
    uint8_t req[84];
    uint8_t resp[33];
    assert(prover_init(&p, &pp, &range, 1));
    assert(init_verifier(&v, &vp, 100));
    assert(verifier_challenge(&v, req, 84) == 84);
    memset(resp, 0xa5, sizeof(resp));
    assert(prover_handle_request(&p, req, 84, resp, 33) == 0);
    assert(pm.error == AUTH_ERROR_MEASUREMENT);
    assert(pm.locked == 0 && pm.m.counter == 1);
    for (unsigned i = 0; i < 33; ++i)
        assert(resp[i] == 0xa5);
}

int main(void) {
    vectors();
    sessions();
    nonce_binding();
    framing();
    measurement_failure();
    puts("protocol tests passed");
}
