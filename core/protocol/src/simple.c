#include "simple.h"
#include <string.h>

/* Shared helpers for constant-time comparison, secret clearing, and wire byte order. */
int simple_ct_compare(const uint8_t *a, const uint8_t *b, size_t len) {
    if (!a || !b)
        return 0;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; ++i)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

void simple_zero(void *data, size_t len) {
    volatile uint8_t *p = data;
    while (len--)
        *p++ = 0;
}

void simple_put_u32(uint8_t out[4], uint32_t value) {
    out[0] = (uint8_t)(value >> 24);
    out[1] = (uint8_t)(value >> 16);
    out[2] = (uint8_t)(value >> 8);
    out[3] = (uint8_t)value;
}

uint32_t simple_get_u32(const uint8_t in[4]) {
    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3];
}

/* Encode and decode the fixed authenticated request and response payloads. */
size_t simple_serialize_msg(const simple_msg_t *m, uint8_t *out, size_t out_max) {
    if (!m || !out || out_max < SIMPLE_REQUEST_LEN)
        return 0;
    simple_put_u32(out, m->counter);
    memcpy(out + 4, m->vs, 32);
    memcpy(out + 36, m->nonce, 16);
    memcpy(out + 52, m->hmac, 32);
    return SIMPLE_REQUEST_LEN;
}

int simple_parse_msg(const uint8_t *buf, size_t len, simple_msg_t *m) {
    if (!buf || !m || len != SIMPLE_REQUEST_LEN)
        return 0;
    m->counter = simple_get_u32(buf);
    memcpy(m->vs, buf + 4, 32);
    memcpy(m->nonce, buf + 36, 16);
    memcpy(m->hmac, buf + 52, 32);
    return 1;
}

size_t simple_serialize_report(const simple_report_t *r, uint8_t *out, size_t out_max) {
    if (!r || !out || out_max < SIMPLE_REPORT_LEN || r->result > 1)
        return 0;
    out[0] = r->result;
    memcpy(out + 1, r->hmac, 32);
    return SIMPLE_REPORT_LEN;
}

int simple_parse_report(const uint8_t *buf, size_t len, simple_report_t *r) {
    if (!buf || !r || len != SIMPLE_REPORT_LEN || buf[0] > 1)
        return 0;
    r->result = buf[0];
    memcpy(r->hmac, buf + 1, 32);
    return 1;
}

/* Build the response authentication input shared by both peers. */
int simple_report_auth(uint8_t result, uint32_t counter, const uint8_t nonce[16],
                       uint8_t out[SIMPLE_REPORT_AUTH_LEN]) {
    if (result > 1 || !nonce || !out)
        return 0;
    out[0] = result;
    simple_put_u32(out + 1, counter);
    memcpy(out + 5, nonce, 16);
    return 1;
}
