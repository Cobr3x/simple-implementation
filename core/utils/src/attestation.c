#include "attestation.h"
#include "simple.h"
#include <limits.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/hmac.h>

int attestation_compute_valid_state(const uint8_t key[32], const attestation_range_t *ranges,
                                    size_t count, uint8_t out[32]) {
    if (!key || !ranges || !count || !out)
        return 0;
    for (size_t i = 0; i < count; ++i)
        if (!ranges[i].start || !ranges[i].length || ranges[i].length > UINT_MAX)
            return 0;
    struct tc_hmac_state_struct h = {0};
    int ok =
        tc_hmac_set_key(&h, key, 32) == TC_CRYPTO_SUCCESS && tc_hmac_init(&h) == TC_CRYPTO_SUCCESS;
    for (size_t i = 0; ok && i < count; ++i)
        ok = tc_hmac_update(&h, ranges[i].start, (unsigned)ranges[i].length) == TC_CRYPTO_SUCCESS;
    if (ok)
        ok = tc_hmac_final(out, 32, &h) == TC_CRYPTO_SUCCESS;
    simple_zero(&h, sizeof(h));
    if (!ok)
        simple_zero(out, 32);
    return ok;
}
