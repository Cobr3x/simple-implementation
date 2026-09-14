#include "auth_journal.h"
#include "crypto_hmac.h"
#include "simple.h"
#include <string.h>

/* Journal record helpers keep the on-flash counter format authenticated and endian-safe. */
static int erased(const uint8_t record[32]) {
    for (unsigned i = 0; i < 32; ++i)
        if (record[i] != 255)
            return 0;
    return 1;
}

static int record_mac(const auth_journal_t *j, uint32_t counter, uint8_t out[32]) {
    uint8_t input[8] = {'C', 'T', 'R', '1'};
    simple_put_u32(input + 4, counter);
    return crypto_hmac_compute(j->key, 32, input, sizeof(input), out);
}

/* Recover the newest valid counter while rejecting gaps or malformed records. */
int auth_journal_load(auth_journal_t *j, uint32_t initial_counter) {
    if (!j || !j->read || !j->program || !j->erase || j->block_size < 32 || j->block_size % 32)
        return 0;
    j->ready = 0;
    j->counter = initial_counter;
    j->active = 0;
    j->next = 0;
    uint8_t record[32];
    uint8_t mac[32];
    int found = 0;
    for (unsigned block = 0; block < 2; ++block) {
        int gap = 0;
        uint32_t last = 0;
        for (size_t off = 0; off < j->block_size; off += 32) {
            if (!j->read(j->user, block, off, record))
                return 0;
            if (erased(record)) {
                gap = 1;
                continue;
            }
            uint32_t c = simple_get_u32(record);
            if (gap || !c || c <= last || simple_get_u32(record + 4) != ~c ||
                !record_mac(j, c, mac) || !simple_ct_compare(record + 8, mac, 24)) {
                simple_zero(mac, 32);
                return 0;
            }
            last = c;
            if (c >= j->counter) {
                j->counter = c;
                j->active = block;
                j->next = off + 32;
                found = 1;
            }
        }
    }
    /* Provisioning seeds both journals, so two erased blocks indicate invalid state. */
    simple_zero(mac, 32);
    if (!found)
        return 0;
    j->ready = 1;
    return 1;
}

/* Append a durable counter record and roll over to the alternate erase block when needed. */
int auth_journal_save(auth_journal_t *j, uint32_t counter) {
    if (!j || !j->ready || counter <= j->counter)
        return 0;
    unsigned block = j->active;
    size_t off = j->next;
    j->ready = 0;
    if (off == j->block_size) {
        block ^= 1;
        off = 0;
        if (!j->erase(j->user, block))
            return 0;
    }
    uint8_t record[32];
    uint8_t mac[32];
    uint8_t check[32];
    simple_put_u32(record, counter);
    simple_put_u32(record + 4, ~counter);
    if (!record_mac(j, counter, mac))
        return 0;
    memcpy(record + 8, mac, 24);
    simple_zero(mac, 32);
    if (!j->program(j->user, block, off, record) || !j->read(j->user, block, off, check) ||
        !simple_ct_compare(record, check, 32))
        return 0;
    j->active = block;
    j->next = off + 32;
    j->counter = counter;
    j->ready = 1;
    return 1;
}
