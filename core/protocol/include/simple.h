#ifndef SIMPLE_H
#define SIMPLE_H

#include <stddef.h>
#include <stdint.h>

#define SIMPLE_NONCE_LEN 16u
#define SIMPLE_HMAC_LEN 32u
#define SIMPLE_VS_LEN 32u
#define SIMPLE_MSG_LEN 52u
#define SIMPLE_REQUEST_LEN 84u
#define SIMPLE_REPORT_LEN 33u
#define SIMPLE_REPORT_AUTH_LEN 21u

typedef struct {
    uint32_t counter;
    uint8_t vs[SIMPLE_VS_LEN];
    uint8_t nonce[SIMPLE_NONCE_LEN];
    uint8_t hmac[SIMPLE_HMAC_LEN];
} simple_msg_t;

typedef struct {
    uint8_t result;
    uint8_t hmac[SIMPLE_HMAC_LEN];
} simple_report_t;

int simple_ct_compare(const uint8_t *a, const uint8_t *b, size_t len);
void simple_zero(void *data, size_t len);
void simple_put_u32(uint8_t out[4], uint32_t value);
uint32_t simple_get_u32(const uint8_t in[4]);
size_t simple_serialize_msg(const simple_msg_t *m, uint8_t *out, size_t out_max);
int simple_parse_msg(const uint8_t *buf, size_t len, simple_msg_t *m);
size_t simple_serialize_report(const simple_report_t *r, uint8_t *out, size_t out_max);
int simple_parse_report(const uint8_t *buf, size_t len, simple_report_t *r);
int simple_report_auth(uint8_t result, uint32_t counter, const uint8_t nonce[16],
                       uint8_t out[SIMPLE_REPORT_AUTH_LEN]);

#endif
