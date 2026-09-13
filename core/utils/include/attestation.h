#ifndef ATTESTATION_H
#define ATTESTATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *start;
    size_t length;
} attestation_range_t;

/* Ranges must be immutable, readable, ordered, and identical to the reference image. */
int attestation_compute_valid_state(const uint8_t key[32], const attestation_range_t *ranges,
                                    size_t count, uint8_t out[32]);

#endif
