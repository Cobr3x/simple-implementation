#ifndef CRYPTO_HMAC_H
#define CRYPTO_HMAC_H

#include <stddef.h>
#include <stdint.h>

int crypto_hmac_compute(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                        uint8_t out[32]);

int crypto_hmac_verify(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                       const uint8_t expected[32]);

#endif
