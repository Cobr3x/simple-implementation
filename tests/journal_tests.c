/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#include "auth_journal.h"
#include "crypto_hmac.h"
#include "simple.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BLOCK 96u

typedef struct {
    uint8_t flash[2][BLOCK];
    int fail_program;
    int fail_erase;
} flash_t;

static int read_record(void *u, unsigned b, size_t o, uint8_t out[32]) {
    flash_t *f = u;
    if (b > 1 || o > BLOCK - 32)
        return 0;
    memcpy(out, f->flash[b] + o, 32);
    return 1;
}

static int program(void *u, unsigned b, size_t o, const uint8_t in[32]) {
    flash_t *f = u;
    if (b > 1 || o > BLOCK - 32)
        return 0;
    for (unsigned i = 0; i < 32; i++)
        assert(f->flash[b][o + i] == 255);
    if (f->fail_program) {
        memcpy(f->flash[b] + o, in, 8);
        return 0;
    }
    memcpy(f->flash[b] + o, in, 32);
    return 1;
}

static int erase(void *u, unsigned b) {
    flash_t *f = u;
    if (b > 1 || f->fail_erase)
        return 0;
    memset(f->flash[b], 255, BLOCK);
    return 1;
}

static auth_journal_t journal(flash_t *f) {
    auth_journal_t j = {
        .user = f, .block_size = BLOCK, .read = read_record, .program = program, .erase = erase};
    memset(j.key, 0x34, 32);
    return j;
}

static void seed(flash_t *f) {
    memset(f, 0, sizeof(*f));
    memset(f->flash, 255, sizeof(f->flash));
    uint8_t data[32];
    uint8_t mac[32];
    uint8_t input[8] = {'C', 'T', 'R', '1', 0, 0, 0, 1};
    auth_journal_t j = journal(f);
    assert(crypto_hmac_compute(j.key, 32, input, 8, mac));
    simple_put_u32(data, 1);
    simple_put_u32(data + 4, ~1u);
    memcpy(data + 8, mac, 24);
    assert(program(f, 0, 0, data));
    assert(program(f, 1, 0, data));
}

int main(void) {
    flash_t f;
    seed(&f);
    auth_journal_t j = journal(&f);
    assert(auth_journal_load(&j, 1));
    assert(j.counter == 1);
    assert(!auth_journal_save(&j, 1));
    for (uint32_t c = 2; c < 50; c++) {
        assert(auth_journal_save(&j, c));
        j = journal(&f);
        assert(auth_journal_load(&j, 1));
        assert(j.counter == c);
    }
    f.fail_program = 1;
    assert(!auth_journal_save(&j, 50));
    assert(!j.ready);
    j = journal(&f);
    assert(!auth_journal_load(&j, 1));
    seed(&f);
    j = journal(&f);
    assert(auth_journal_load(&j, 1));
    assert(auth_journal_save(&j, 2));
    assert(auth_journal_save(&j, 3));
    f.fail_erase = 1;
    assert(!auth_journal_save(&j, 4));
    j = journal(&f);
    assert(auth_journal_load(&j, 1));
    assert(j.counter == 3);
    f.fail_erase = 0;
    assert(auth_journal_save(&j, 4));
    f.flash[0][8] ^= 1;
    j = journal(&f);
    assert(!auth_journal_load(&j, 1));
    memset(f.flash, 255, sizeof(f.flash));
    j = journal(&f);
    assert(!auth_journal_load(&j, 1));
    seed(&f);
    j = journal(&f);
    j.key[0] ^= 1;
    assert(!auth_journal_load(&j, 1));
    seed(&f);
    j = journal(&f);
    assert(!auth_journal_load(&j, 2));
    puts("journal tests passed");
}
