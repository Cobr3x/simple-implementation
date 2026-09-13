#ifndef AUTH_JOURNAL_H
#define AUTH_JOURNAL_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *user;
    size_t block_size;
    int (*read)(void *, unsigned block, size_t offset, uint8_t out[32]);
    int (*program)(void *, unsigned block, size_t offset, const uint8_t data[32]);
    int (*erase)(void *, unsigned block);
    uint8_t key[32];
    uint32_t counter;
    unsigned active;
    size_t next;
    int ready;
} auth_journal_t;

int auth_journal_load(auth_journal_t *j, uint32_t initial_counter);
int auth_journal_save(auth_journal_t *j, uint32_t counter);

#endif
