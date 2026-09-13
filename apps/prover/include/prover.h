#ifndef PROVER_H
#define PROVER_H

#include "attestation.h"
#include "auth_platform.h"

typedef struct {
    auth_platform_t platform;
    auth_material_t material;
    const attestation_range_t *ranges;
    size_t range_count;
    int ready;
} prover_t;

int prover_init(prover_t *p, const auth_platform_t *platform, const attestation_range_t *ranges,
                size_t count);
size_t prover_handle_request(prover_t *p, const uint8_t *req, size_t req_len, uint8_t *resp,
                             size_t resp_max);

#endif
