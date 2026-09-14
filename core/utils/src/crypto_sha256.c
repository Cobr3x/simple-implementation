#include "crypto_sha256.h"
#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>

/* Hash a complete buffer while propagating every TinyCrypt failure. */
int crypto_sha256(const uint8_t *data, size_t data_len, uint8_t out[32]) {
    if (!data || !out)
        return 0;

    struct tc_sha256_state_struct s;

    if (tc_sha256_init(&s) != TC_CRYPTO_SUCCESS)
        return 0;

    if (tc_sha256_update(&s, data, data_len) != TC_CRYPTO_SUCCESS)
        return 0;

    if (tc_sha256_final(out, &s) != TC_CRYPTO_SUCCESS)
        return 0;

    return 1;
}
