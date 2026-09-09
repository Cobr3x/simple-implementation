#ifndef CRYPTO_HMAC_H
#define CRYPTO_HMAC_H

#include <stdint.h>
#include <stddef.h>

int crypto_hmac_compute(const uint8_t *key,
                        size_t key_len,
                        const uint8_t *msg,
                        size_t msg_len,
                        uint8_t out[32]);

// Returns 1 if match, 0 otherwise.
int crypto_hmac_verify(const uint8_t *key,
                       size_t key_len,
                       const uint8_t *msg,
                       size_t msg_len,
                       const uint8_t expected[32]);

#endif
