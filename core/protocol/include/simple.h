#ifndef SIMPLE_H
#define SIMPLE_H

#include <stdint.h>
#include <stddef.h>

#define SIMPLE_NONCE_LEN 16
#define SIMPLE_HMAC_LEN  32
#define SIMPLE_VS_LEN    32

typedef struct {
    uint32_t counter;
    uint8_t  vs[SIMPLE_VS_LEN];
    uint8_t  nonce[SIMPLE_NONCE_LEN];
} simple_msg_t;

typedef struct {
    uint8_t  result;      // 1 = ok, 0 = fail
    uint32_t counter;
    uint8_t  hmac[SIMPLE_HMAC_LEN];
} simple_report_t;

int simple_ct_compare(const uint8_t *a, const uint8_t *b, size_t len);

size_t simple_serialize_msg(const simple_msg_t *m, uint8_t *out, size_t out_max);
int    simple_parse_msg(const uint8_t *buf, size_t len, simple_msg_t *m);

size_t simple_serialize_report(const simple_report_t *r, uint8_t *out, size_t out_max);
int    simple_parse_report(const uint8_t *buf, size_t len, simple_report_t *r);

#endif
