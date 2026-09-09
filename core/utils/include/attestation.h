#ifndef ATTESTATION_H
#define ATTESTATION_H

#include <stdint.h>
#include <stddef.h>

int attestation_compute_valid_state(const uint8_t *K_attest,
                                    uint8_t out[32]);

#endif
