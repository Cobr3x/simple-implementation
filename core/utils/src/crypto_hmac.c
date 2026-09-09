#include "crypto_hmac.h"
#include "simple.h"
#include <tinycrypt/hmac.h>
#include <tinycrypt/constants.h>

int crypto_hmac_compute(const uint8_t *key,
                        size_t key_len,
                        const uint8_t *msg,
                        size_t msg_len,
                        uint8_t out[32])
{
    struct tc_hmac_state_struct h;

    if (tc_hmac_set_key(&h, key, key_len) != TC_CRYPTO_SUCCESS)
        return 0;

    if (tc_hmac_init(&h) != TC_CRYPTO_SUCCESS)
        return 0;

    if (tc_hmac_update(&h, msg, msg_len) != TC_CRYPTO_SUCCESS)
        return 0;

    if (tc_hmac_final(out, &h, 32) != TC_CRYPTO_SUCCESS)
        return 0;

    return 1;
}



int crypto_hmac_verify(const uint8_t *key,
                       size_t key_len,
                       const uint8_t *msg,
                       size_t msg_len,
                       const uint8_t expected[32])
{
    uint8_t computed[32];

    if (!crypto_hmac_compute(key, key_len, msg, msg_len, computed))
        return 0;

    return simple_ct_compare(computed, expected, 32);
}
