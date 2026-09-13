#ifndef VERIFIER_H
#define VERIFIER_H

#include "auth_platform.h"
#include "simple.h"

typedef enum {
    VERIFY_INVALID = -1,
    VERIFY_TIMEOUT = -2,
    VERIFY_IDLE = -3,
    VERIFY_MISMATCH = 0,
    VERIFY_PASS = 1
} verify_result_t;

typedef struct {
    auth_platform_t platform;
    auth_material_t material;
    uint8_t nonce[SIMPLE_NONCE_LEN];
    uint32_t started_ms;
    uint32_t timeout_ms;
    int ready;
    int outstanding;
} verifier_t;

int init_verifier(verifier_t *v, const auth_platform_t *platform, uint32_t timeout_ms);
size_t verifier_challenge(verifier_t *v, uint8_t *out, size_t capacity);
verify_result_t verify(verifier_t *v, const uint8_t *response, size_t len);
verify_result_t verifier_poll(verifier_t *v);
void verifier_cancel(verifier_t *v);

#endif
