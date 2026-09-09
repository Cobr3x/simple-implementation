#include "simple.h"
#include <string.h>

int simple_ct_compare(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
        diff |= (a[i] ^ b[i]);
    return diff == 0;
}

// Wire encoding currently depends on native struct layout and byte order.
// TODO: Use a portable wire format without padding.
size_t simple_serialize_msg(const simple_msg_t *m, uint8_t *out, size_t out_max)
{
    if (out_max < sizeof(simple_msg_t))
        return 0;

    memcpy(out, m, sizeof(simple_msg_t));
    return sizeof(simple_msg_t);
}

int simple_parse_msg(const uint8_t *buf, size_t len, simple_msg_t *m)
{
    if (len < sizeof(simple_msg_t))
        return 0;

    memcpy(m, buf, sizeof(simple_msg_t));
    return 1;
}

size_t simple_serialize_report(const simple_report_t *r, uint8_t *out, size_t out_max)
{
    if (out_max < sizeof(simple_report_t))
        return 0;

    memcpy(out, r, sizeof(simple_report_t));
    return sizeof(simple_report_t);
}

int simple_parse_report(const uint8_t *buf, size_t len, simple_report_t *r)
{
    if (len < sizeof(simple_report_t))
        return 0;

    memcpy(r, buf, sizeof(simple_report_t));
    return 1;
}
