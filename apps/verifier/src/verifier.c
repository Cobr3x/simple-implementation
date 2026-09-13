#include "verifier.h"
#include "crypto_hmac.h"
#include <string.h>

int init_verifier(verifier_t *v, const auth_platform_t *platform, uint32_t timeout_ms) {
    if (!v || !platform || !platform->load || !platform->save_counter || !platform->random ||
        !platform->now_ms || !timeout_ms || timeout_ms > INT32_MAX)
        return 0;
    memset(v, 0, sizeof(*v));
    v->platform = *platform;
    v->timeout_ms = timeout_ms;
    v->ready = platform->load(platform->user, &v->material);
    if (!v->ready)
        simple_zero(&v->material, sizeof(v->material));
    if (v->ready)
        auth_ui(platform, AUTH_UI_READY);
    else if (!platform->error)
        auth_ui(platform, AUTH_UI_ERROR);
    return v->ready;
}

void verifier_cancel(verifier_t *v) {
    if (!v)
        return;
    v->outstanding = 0;
    simple_zero(v->nonce, sizeof(v->nonce));
}

size_t verifier_challenge(verifier_t *v, uint8_t *out, size_t capacity) {
    if (!v || !v->ready || v->outstanding || !out || capacity < SIMPLE_REQUEST_LEN)
        return 0;
    if (v->material.counter == UINT32_MAX) {
        auth_error(&v->platform, AUTH_ERROR_CHALLENGE);
        return 0;
    }
    simple_msg_t msg = {0};
    if (!v->platform.random(v->platform.user, msg.nonce, sizeof(msg.nonce))) {
        auth_error(&v->platform, AUTH_ERROR_RNG);
        return 0;
    }
    msg.counter = v->material.counter + 1;
    memcpy(msg.vs, v->material.reference_vs, 32);
    if (!simple_serialize_msg(&msg, out, capacity) ||
        !crypto_hmac_compute(v->material.k_auth, 32, out, SIMPLE_MSG_LEN, msg.hmac)) {
        auth_error(&v->platform, AUTH_ERROR_CHALLENGE);
        return 0;
    }
    /* Commit before transmission. A reset may skip a counter but never reuse one. */
    int saved = v->platform.save_counter(v->platform.user, msg.counter);
    if (saved <= 0) {
        v->ready = 0;
        simple_zero(out, SIMPLE_REQUEST_LEN);
        auth_error_t error = saved < 0 ? (auth_error_t)-saved : AUTH_ERROR_COUNTER_SAVE;
        auth_error(&v->platform, error);
        return 0;
    }
    v->material.counter = msg.counter;
    memcpy(v->nonce, msg.nonce, sizeof(v->nonce));
    v->started_ms = v->platform.now_ms(v->platform.user);
    v->outstanding = 1;
    auth_ui(&v->platform, AUTH_UI_REQUEST);
    return simple_serialize_msg(&msg, out, capacity);
}

verify_result_t verifier_poll(verifier_t *v) {
    if (!v || !v->ready || !v->outstanding)
        return VERIFY_IDLE;
    if (!auth_expired(v->platform.now_ms(v->platform.user), v->started_ms, v->timeout_ms))
        return VERIFY_IDLE;
    verifier_cancel(v);
    auth_ui(&v->platform, AUTH_UI_TIMEOUT);
    return VERIFY_TIMEOUT;
}

verify_result_t verify(verifier_t *v, const uint8_t *response, size_t len) {
    if (!v || !v->ready || !v->outstanding)
        return VERIFY_INVALID;
    if (verifier_poll(v) == VERIFY_TIMEOUT)
        return VERIFY_TIMEOUT;
    simple_report_t report;
    uint8_t input[SIMPLE_REPORT_AUTH_LEN];
    if (!simple_parse_report(response, len, &report) ||
        !simple_report_auth(report.result, v->material.counter, v->nonce, input) ||
        !crypto_hmac_verify(v->material.k_auth, 32, input, sizeof(input), report.hmac))
        return VERIFY_INVALID;
    if (verifier_poll(v) == VERIFY_TIMEOUT)
        return VERIFY_TIMEOUT;
    verifier_cancel(v);
    auth_ui(&v->platform, report.result ? AUTH_UI_PASS : AUTH_UI_MISMATCH);
    return report.result ? VERIFY_PASS : VERIFY_MISMATCH;
}
