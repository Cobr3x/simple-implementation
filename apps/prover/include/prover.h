#ifndef PROVER_H
#define PROVER_H

#include <stdint.h>
#include <stddef.h>

void prover_init(void);

size_t prover_handle_request(const uint8_t *req,
                             size_t req_len,
                             uint8_t *resp,
                             size_t resp_max);

#endif
