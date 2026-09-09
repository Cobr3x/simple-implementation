#include "prover.h"
#include "simple.h"
#include "crypto_hmac.h"
#include "attestation.h"
#include <string.h>

static uint8_t K_auth[32];
static uint8_t K_attest[32];
static uint32_t C_P = 0;

void prover_init(void)
{
    // TODO: Replace test keys with device keys.
    memset(K_auth,   0xAA, sizeof(K_auth));
    memset(K_attest, 0xBB, sizeof(K_attest));
    C_P = 0;
}

size_t prover_handle_request(const uint8_t *req,
                             size_t req_len,
                             uint8_t *resp,
                             size_t resp_max)
{
    // TODO: Check buffer pointers and request length, including the HMAC.
    simple_msg_t msg;

    if (!simple_parse_msg(req, req_len, &msg))
        return 0;

    const uint8_t *hmac_msg = req + sizeof(simple_msg_t);

    if (!crypto_hmac_verify(K_auth, sizeof(K_auth),
                            (uint8_t *)&msg, sizeof(msg),
                            hmac_msg))
        return 0;

    if (C_P < msg.counter)
        C_P = msg.counter;

    uint8_t VS_prime[SIMPLE_VS_LEN];
    // TODO: Handle attestation failure.
    attestation_compute_valid_state(K_attest, VS_prime);

    uint8_t result = (memcmp(VS_prime, msg.vs, SIMPLE_VS_LEN) == 0);

    simple_report_t rep;
    rep.result  = result;
    rep.counter = C_P;

    // TODO: Exclude padding from the report HMAC and handle HMAC failure.
    crypto_hmac_compute(K_auth, sizeof(K_auth),
                        (uint8_t *)&rep.result,
                        1 + sizeof(rep.counter),
                        rep.hmac);

    return simple_serialize_report(&rep, resp, resp_max);
}
