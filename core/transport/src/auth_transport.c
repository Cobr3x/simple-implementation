#include "auth_transport.h"
#include <string.h>

size_t auth_frame_encode(uint8_t type, const uint8_t *payload, size_t len, uint8_t *out,
                         size_t capacity) {
    if (!payload || !out ||
        !((type == AUTH_FRAME_REQUEST && len == SIMPLE_REQUEST_LEN) ||
          (type == AUTH_FRAME_RESPONSE && len == SIMPLE_REPORT_LEN) ||
          (type == AUTH_FRAME_DIAGNOSTIC && len == 1u)) ||
        capacity < len + 4)
        return 0;
    uint8_t raw[SIMPLE_REQUEST_LEN + 2];
    raw[0] = type;
    raw[1] = (uint8_t)len;
    memcpy(raw + 2, payload, len);
    size_t pos = 1;
    size_t code_pos = 0;
    uint8_t code = 1;
    for (size_t i = 0; i < len + 2; ++i) {
        if (raw[i] == 0) {
            out[code_pos] = code;
            code_pos = pos++;
            code = 1;
        } else {
            out[pos++] = raw[i];
            ++code;
        }
    }
    out[code_pos] = code;
    out[pos++] = 0;
    return pos;
}

int auth_frame_feed(auth_receiver_t *r, uint8_t byte, uint32_t now_ms, uint32_t frame_timeout_ms,
                    uint8_t *type, uint8_t *payload, size_t capacity, size_t *length) {
    if (!r || !type || !payload || !length || !frame_timeout_ms)
        return -1;
    *length = 0;
    if (r->used && auth_expired(now_ms, r->started_ms, frame_timeout_ms))
        r->discard = 1;
    if (byte != 0) {
        if (!r->used && !r->discard)
            r->started_ms = now_ms;
        if (r->discard)
            return 0;
        if (r->used >= sizeof(r->encoded)) {
            r->discard = 1;
            return 0;
        }
        r->encoded[r->used++] = byte;
        return 0;
    }
    size_t used = r->used;
    r->used = 0;
    if (r->discard) {
        r->discard = 0;
        return -1;
    }
    if (!used)
        return 0;
    uint8_t raw[SIMPLE_REQUEST_LEN + 2];
    size_t i = 0;
    size_t n = 0;
    while (i < used) {
        uint8_t code = r->encoded[i++];
        if (!code || (size_t)(code - 1) > used - i)
            return -1;
        for (unsigned j = 1; j < code; ++j) {
            if (n >= sizeof(raw))
                return -1;
            raw[n++] = r->encoded[i++];
        }
        if (code != 255 && i < used) {
            if (n >= sizeof(raw))
                return -1;
            raw[n++] = 0;
        }
    }
    if (n < 2 || raw[1] != n - 2 || capacity < n - 2 ||
        !((raw[0] == AUTH_FRAME_REQUEST && raw[1] == SIMPLE_REQUEST_LEN) ||
          (raw[0] == AUTH_FRAME_RESPONSE && raw[1] == SIMPLE_REPORT_LEN) ||
          (raw[0] == AUTH_FRAME_DIAGNOSTIC && raw[1] == 1u)))
        return -1;
    *type = raw[0];
    *length = n - 2;
    memcpy(payload, raw + 2, *length);
    return 1;
}

int auth_frame_send(const auth_link_t *link, uint8_t type, const uint8_t *payload, size_t len,
                    uint32_t timeout_ms) {
    if (!link || !link->write || !timeout_ms)
        return 0;
    uint8_t frame[AUTH_FRAME_MAX + 1];
    /* A leading delimiter recovers a peer left inside a truncated frame. */
    frame[0] = 0;
    size_t n = auth_frame_encode(type, payload, len, frame + 1, sizeof(frame) - 1);
    return n && link->write(link->user, frame, n + 1, timeout_ms);
}
