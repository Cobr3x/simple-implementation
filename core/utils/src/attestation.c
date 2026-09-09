#include "attestation.h"
#include "crypto_sha256.h"
#include <string.h>


// TODO: Hash firmware instead of the key and fixed tag.
int attestation_compute_valid_state(const uint8_t *K_attest,
                                    uint8_t out[32])
{
    if (!out)
        return 0;

    uint8_t buf[32 + 8];

    if (K_attest)
        memcpy(buf, K_attest, 32);
    else
        memset(buf, 0, 32);

    const uint8_t tag[8] = { 'S','I','M','P','L','E','A','T' };
    memcpy(buf + 32, tag, 8);

    return crypto_sha256(buf, sizeof(buf), out);
}
