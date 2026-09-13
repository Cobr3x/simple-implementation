#include "auth_journal.h"
#include "board.h"
#include "crypto_sha256.h"
#include "simple.h"
#include <string.h>

_Static_assert(AUTH_JOURNAL_A % AUTH_JOURNAL_BLOCK_SIZE == 0,
               "Journal A is not erase-unit aligned");
_Static_assert(AUTH_JOURNAL_B % AUTH_JOURNAL_BLOCK_SIZE == 0,
               "Journal B is not erase-unit aligned");
_Static_assert(AUTH_JOURNAL_A + AUTH_JOURNAL_BLOCK_SIZE <= AUTH_JOURNAL_B ||
                   AUTH_JOURNAL_B + AUTH_JOURNAL_BLOCK_SIZE <= AUTH_JOURNAL_A,
               "Journal erase units overlap");
_Static_assert(AUTH_KEY_ADDRESS + 160u <= AUTH_JOURNAL_A ||
                   AUTH_KEY_ADDRESS >= AUTH_JOURNAL_A + AUTH_JOURNAL_BLOCK_SIZE,
               "Provisioning data overlaps journal A");
_Static_assert(AUTH_KEY_ADDRESS + 160u <= AUTH_JOURNAL_B ||
                   AUTH_KEY_ADDRESS >= AUTH_JOURNAL_B + AUTH_JOURNAL_BLOCK_SIZE,
               "Provisioning data overlaps journal B");

static auth_journal_t journal;
static auth_error_t store_error;

static uintptr_t block_address(unsigned block) {
    return block == 0 ? AUTH_JOURNAL_A : AUTH_JOURNAL_B;
}

static int journal_read(void *u, unsigned block, size_t off, uint8_t out[32]) {
    (void)u;
    if (block > 1 || off > AUTH_JOURNAL_BLOCK_SIZE - 32)
        return 0;
    memcpy(out, (const void *)(block_address(block) + off), 32);
    return 1;
}

static int journal_program(void *u, unsigned block, size_t off, const uint8_t bytes[32]) {
    (void)u;
    if (block > 1 || off > AUTH_JOURNAL_BLOCK_SIZE - 32 || off % 32)
        return 0;
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    board_flash_access(1);
    int ok = HAL_FLASH_Unlock() == HAL_OK;
    if (!ok)
        store_error = AUTH_ERROR_FLASH_UNLOCK;
#if !AUTH_STM32_H7
    if (ok)
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS | FLASH_FLAG_EOP);
#endif
    uintptr_t address = block_address(block) + off;
#if AUTH_STM32_H7
    _Alignas(32) uint8_t data[32];
    memcpy(data, bytes, 32);
    if (ok)
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, (uint32_t)address,
                               (uint32_t)(uintptr_t)data) == HAL_OK;
    simple_zero(data, 32);
#else
    for (unsigned i = 0; ok && i < 32; i += 8) {
        uint64_t word;
        memcpy(&word, bytes + i, 8);
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, (uint32_t)address + i, word) == HAL_OK;
    }
#endif
    if (!ok && store_error == AUTH_ERROR_COUNTER_SAVE) {
        uint32_t error = HAL_FLASH_GetError();
        if (error & HAL_FLASH_ERROR_WRP)
            store_error = AUTH_ERROR_FLASH_WRP;
#if AUTH_STM32_H7
        else if (error & (HAL_FLASH_ERROR_PGS | HAL_FLASH_ERROR_STRB | HAL_FLASH_ERROR_INC))
            store_error = AUTH_ERROR_FLASH_ALIGNMENT;
#else
        else if (error & HAL_FLASH_ERROR_PGA)
            store_error = AUTH_ERROR_FLASH_ALIGNMENT;
        else if (error & HAL_FLASH_ERROR_SIZ)
            store_error = AUTH_ERROR_FLASH_SIZE_WRITE;
        else if (error & HAL_FLASH_ERROR_PGS)
            store_error = AUTH_ERROR_FLASH_SEQUENCE;
#endif
        else
            store_error = AUTH_ERROR_FLASH_PROGRAM;
    }
    if (ok && memcmp((const void *)address, bytes, 32) != 0) {
        ok = 0;
        store_error = AUTH_ERROR_FLASH_VERIFY;
    }
    HAL_FLASH_Lock();
    board_flash_access(0);
    __set_PRIMASK(saved);
    return ok;
}

static int journal_erase(void *u, unsigned block) {
    (void)u;
    if (block > 1)
        return 0;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
#if AUTH_STM32_H7
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = AUTH_JOURNAL_BANK;
    erase.Sector = block == 0 ? AUTH_JOURNAL_UNIT_A : AUTH_JOURNAL_UNIT_B;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
#else
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = AUTH_JOURNAL_BANK;
    erase.Page = block == 0 ? AUTH_JOURNAL_UNIT_A : AUTH_JOURNAL_UNIT_B;
    erase.NbPages = 1;
#endif
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    board_flash_access(1);
    int ok = HAL_FLASH_Unlock() == HAL_OK;
    if (!ok)
        store_error = AUTH_ERROR_FLASH_UNLOCK;
#if !AUTH_STM32_H7
    if (ok)
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS | FLASH_FLAG_EOP);
#endif
    if (ok && HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
        ok = 0;
        store_error = (HAL_FLASH_GetError() & HAL_FLASH_ERROR_WRP) ? AUTH_ERROR_FLASH_WRP
                                                                   : AUTH_ERROR_FLASH_ERASE;
    }
    HAL_FLASH_Lock();
    board_flash_access(0);
    __set_PRIMASK(saved);
    return ok;
}

static int protection_valid(void) {
    FLASH_OBProgramInitTypeDef ob = {0};
#if AUTH_STM32_H7
    if (FLASH_SIZE / 1024u != AUTH_FLASH_KIB) {
        board_report_error(NULL, AUTH_ERROR_FLASH_SIZE);
        return 0;
    }
    ob.Banks = FLASH_BANK_1;
    HAL_FLASHEx_OBGetConfig(&ob);
    if (ob.USERConfig & OB_SWAP_BANK_ENABLE) {
        board_report_error(NULL, AUTH_ERROR_BANK_MAPPING);
        return 0;
    }
#if AUTH_REQUIRE_PROTECTION
    if (ob.RDPLevel != OB_RDP_LEVEL_1) {
        board_report_error(NULL, AUTH_ERROR_RDP);
        return 0;
    }
    if (!(ob.WRPSector & AUTH_CODE_WRP)) {
        board_report_error(NULL, AUTH_ERROR_CODE_WRP);
        return 0;
    }
    ob.Banks = FLASH_BANK_2;
    HAL_FLASHEx_OBGetConfig(&ob);
    if (!(ob.WRPSector & AUTH_KEY_WRP)) {
        board_report_error(NULL, AUTH_ERROR_KEY_WRP);
        return 0;
    }
#endif
#else
    if (FLASH_SIZE / 1024u != AUTH_FLASH_KIB) {
        board_report_error(NULL, AUTH_ERROR_FLASH_SIZE);
        return 0;
    }
#if AUTH_FLASH_REQUIRE_DBANK
    if (!(FLASH->OPTR & FLASH_OPTR_DBANK) || (SYSCFG->MEMRMP & SYSCFG_MEMRMP_FB_MODE)) {
        board_report_error(NULL, AUTH_ERROR_BANK_MAPPING);
        return 0;
    }
#endif
#if AUTH_REQUIRE_PROTECTION
    ob.WRPArea = AUTH_CODE_WRP_AREA;
    HAL_FLASHEx_OBGetConfig(&ob);
    if (ob.RDPLevel != OB_RDP_LEVEL_1) {
        board_report_error(NULL, AUTH_ERROR_RDP);
        return 0;
    }
    if (ob.WRPStartOffset != AUTH_CODE_WRP_START || ob.WRPEndOffset != AUTH_CODE_WRP_END) {
        board_report_error(NULL, AUTH_ERROR_CODE_WRP);
        return 0;
    }
    ob.WRPArea = AUTH_KEY_WRP_AREA;
    HAL_FLASHEx_OBGetConfig(&ob);
    if (ob.WRPStartOffset != AUTH_KEY_WRP_START || ob.WRPEndOffset != AUTH_KEY_WRP_END) {
        board_report_error(NULL, AUTH_ERROR_KEY_WRP);
        return 0;
    }
#else
    (void)ob;
#endif
#endif
    return 1;
}

int board_store_load(void *user, auth_material_t *m) {
    (void)user;
    if (!m) {
        board_report_error(NULL, AUTH_ERROR_INTERNAL);
        return 0;
    }
    if (!protection_valid())
        return 0;
    const uint8_t *p = (const uint8_t *)AUTH_KEY_ADDRESS;
    uint8_t digest[32];
    if (memcmp(p, "SIMPLE01", 8) || simple_get_u32(p + 8) != 1 ||
        simple_get_u32(p + 12) != AUTH_ROLE_ID || simple_get_u32(p + 112) != AUTH_MEASURE_START ||
        simple_get_u32(p + 116) != AUTH_MEASURE_LENGTH || !simple_get_u32(p + 120) ||
        !crypto_sha256(p, 124, digest) || !simple_ct_compare(digest, p + 124, 32)) {
        board_report_error(NULL, AUTH_ERROR_PROVISIONING);
        return 0;
    }
    memcpy(m->k_auth, p + 16, 32);
    memcpy(m->k_attest, p + 48, 32);
    memcpy(m->reference_vs, p + 80, 32);
    journal = (auth_journal_t){.block_size = AUTH_JOURNAL_BLOCK_SIZE,
                               .read = journal_read,
                               .program = journal_program,
                               .erase = journal_erase};
    memcpy(journal.key, m->k_auth, 32);
    if (!auth_journal_load(&journal, simple_get_u32(p + 120))) {
        simple_zero(&journal, sizeof(journal));
        simple_zero(m, sizeof(*m));
        board_report_error(NULL, AUTH_ERROR_JOURNAL);
        return 0;
    }
    m->counter = journal.counter;
    board_loaded_counter = m->counter;
    return 1;
}

int board_store_save(void *user, uint32_t counter) {
    (void)user;
    store_error = AUTH_ERROR_COUNTER_SAVE;
    int ok = auth_journal_save(&journal, counter);
    if (ok)
        board_loaded_counter = counter;
    return ok ? 1 : -(int)store_error;
}
