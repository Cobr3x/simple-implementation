#include "crypto_hmac.h"
#include "simple.h"
#include <limits.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/hmac.h>

int crypto_hmac_compute(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                        uint8_t out[32]) {
    if (!key || !key_len || !out || (!msg && msg_len) || key_len > UINT_MAX || msg_len > UINT_MAX)
        return 0;
    struct tc_hmac_state_struct h = {0};
    int ok = tc_hmac_set_key(&h, key, (unsigned)key_len) == TC_CRYPTO_SUCCESS &&
             tc_hmac_init(&h) == TC_CRYPTO_SUCCESS;
    if (ok && msg_len)
        ok = tc_hmac_update(&h, msg, (unsigned)msg_len) == TC_CRYPTO_SUCCESS;
    if (ok)
        ok = tc_hmac_final(out, 32, &h) == TC_CRYPTO_SUCCESS;
    simple_zero(&h, sizeof(h));
    if (!ok)
        simple_zero(out, 32);
    return ok;
}

int crypto_hmac_verify(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len,
                       const uint8_t expected[32]) {
    if (!expected)
        return 0;
    uint8_t computed[32];
    int ok = crypto_hmac_compute(key, key_len, msg, msg_len, computed) &&
             simple_ct_compare(computed, expected, 32);
    simple_zero(computed, sizeof(computed));
    return ok;
}
