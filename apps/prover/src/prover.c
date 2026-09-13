#include "prover.h"
#include "crypto_hmac.h"
#include "simple.h"
#include <string.h>

int prover_init(prover_t *p, const auth_platform_t *platform, const attestation_range_t *ranges,
                size_t count) {
    if (!p || !platform || !platform->load || !platform->save_counter || !platform->enter_atomic ||
        !platform->leave_atomic || !ranges || !count)
        return 0;
    memset(p, 0, sizeof(*p));
    p->platform = *platform;
    p->ranges = ranges;
    p->range_count = count;
    p->ready = platform->load(platform->user, &p->material);
    if (!p->ready)
        simple_zero(&p->material, sizeof(p->material));
    if (p->ready)
        auth_ui(platform, AUTH_UI_READY);
    else if (!platform->error)
        auth_ui(platform, AUTH_UI_ERROR);
    return p->ready;
}

size_t prover_handle_request(prover_t *p, const uint8_t *req, size_t req_len, uint8_t *resp,
                             size_t resp_max) {
    if (!p || !p->ready || !req || !resp || resp_max < SIMPLE_REPORT_LEN)
        return 0;
    if (req_len != SIMPLE_REQUEST_LEN) {
        auth_error(&p->platform, AUTH_ERROR_REQUEST_LENGTH);
        return 0;
    }
    auth_ui(&p->platform, AUTH_UI_REQUEST);
    uintptr_t previous = p->platform.enter_atomic(p->platform.user);
    simple_msg_t msg = {0};
    simple_report_t report = {0};
    uint8_t vs[32] = {0};
    uint8_t input[SIMPLE_REPORT_AUTH_LEN] = {0};
    auth_phase_t phase = AUTH_UI_REJECTED;
    auth_error_t error = AUTH_ERROR_NONE;
    size_t len = 0;
    if (!simple_parse_msg(req, req_len, &msg)) {
        error = AUTH_ERROR_FRAME;
        goto done;
    }
    if (msg.counter <= p->material.counter) {
        error = AUTH_ERROR_STALE_COUNTER;
        goto done;
    }
    if (!crypto_hmac_verify(p->material.k_auth, 32, req, SIMPLE_MSG_LEN, msg.hmac)) {
        error = AUTH_ERROR_REQUEST_AUTH;
        goto done;
    }
    int saved = p->platform.save_counter(p->platform.user, msg.counter);
    if (saved <= 0) {
        p->ready = 0;
        error = saved < 0 ? (auth_error_t)-saved : AUTH_ERROR_COUNTER_SAVE;
        goto done;
    }
    p->material.counter = msg.counter;
    auth_ui(&p->platform, AUTH_UI_MEASURING);
    if (!attestation_compute_valid_state(p->material.k_attest, p->ranges, p->range_count, vs)) {
        error = AUTH_ERROR_MEASUREMENT;
        goto done;
    }
    report.result = (uint8_t)simple_ct_compare(vs, msg.vs, 32);
    if (!simple_report_auth(report.result, msg.counter, msg.nonce, input) ||
        !crypto_hmac_compute(p->material.k_auth, 32, input, sizeof(input), report.hmac)) {
        error = AUTH_ERROR_RESPONSE;
        goto done;
    }
    len = simple_serialize_report(&report, resp, resp_max);
    if (!len) {
        error = AUTH_ERROR_RESPONSE;
        goto done;
    }
    phase = report.result ? AUTH_UI_PASS : AUTH_UI_MISMATCH;
done:
    simple_zero(vs, sizeof(vs));
    simple_zero(input, sizeof(input));
    simple_zero(&msg, sizeof(msg));
    simple_zero(&report, sizeof(report));
    p->platform.leave_atomic(p->platform.user, previous);
    if (error)
        auth_error(&p->platform, error);
    else
        auth_ui(&p->platform, phase);
    return len;
}
