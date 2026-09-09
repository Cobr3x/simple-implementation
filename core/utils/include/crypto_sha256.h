#ifndef CRYPTO_SHA256_H
#define CRYPTO_SHA256_H

#include <stdint.h>
#include <stddef.h>

int crypto_sha256(const uint8_t *data,
                  size_t data_len,
                  uint8_t out[32]);

#endif
