#ifndef AUTH_TRANSPORT_H
#define AUTH_TRANSPORT_H

#include "auth_platform.h"
#include "simple.h"

#define AUTH_FRAME_MAX (SIMPLE_REQUEST_LEN + 4u)
#define AUTH_FRAME_REQUEST 1u
#define AUTH_FRAME_RESPONSE 2u
#define AUTH_FRAME_DIAGNOSTIC 3u

typedef struct {
    uint8_t encoded[AUTH_FRAME_MAX];
    size_t used;
    uint32_t started_ms;
    int discard;
} auth_receiver_t;

size_t auth_frame_encode(uint8_t type, const uint8_t *payload, size_t len, uint8_t *out,
                         size_t capacity);
/* Returns 1 for a complete frame, 0 while incomplete, and -1 when discarded. */
int auth_frame_feed(auth_receiver_t *r, uint8_t byte, uint32_t now_ms, uint32_t frame_timeout_ms,
                    uint8_t *type, uint8_t *payload, size_t capacity, size_t *length);
int auth_frame_send(const auth_link_t *link, uint8_t type, const uint8_t *payload, size_t len,
                    uint32_t timeout_ms);

#endif
