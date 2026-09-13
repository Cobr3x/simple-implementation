#ifndef AUTH_PLATFORM_H
#define AUTH_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    AUTH_UI_READY,
    AUTH_UI_REQUEST,
    AUTH_UI_MEASURING,
    AUTH_UI_PASS,
    AUTH_UI_MISMATCH,
    AUTH_UI_REJECTED,
    AUTH_UI_TIMEOUT,
    AUTH_UI_ERROR
} auth_phase_t;

typedef enum {
    AUTH_ERROR_NONE = 0,
    AUTH_ERROR_CLOCK = 1,
    AUTH_ERROR_UART_INIT = 2,
    AUTH_ERROR_RNG = 3,
    AUTH_ERROR_LCD = 4,
    AUTH_ERROR_PROVISIONING = 6,
    AUTH_ERROR_JOURNAL = 7,
    AUTH_ERROR_UART_RX = 8,
    AUTH_ERROR_FRAME = 9,
    AUTH_ERROR_FRAME_TYPE = 10,
    AUTH_ERROR_REQUEST_LENGTH = 11,
    AUTH_ERROR_STALE_COUNTER = 12,
    AUTH_ERROR_REQUEST_AUTH = 13,
    AUTH_ERROR_COUNTER_SAVE = 14,
    AUTH_ERROR_MEASUREMENT = 15,
    AUTH_ERROR_RESPONSE = 16,
    AUTH_ERROR_CHALLENGE = 17,
    AUTH_ERROR_UART_TX = 18,
    AUTH_ERROR_INTERNAL = 19,
    AUTH_ERROR_FLASH_SIZE = 20,
    AUTH_ERROR_BANK_MAPPING = 21,
    AUTH_ERROR_RDP = 22,
    AUTH_ERROR_CODE_WRP = 23,
    AUTH_ERROR_KEY_WRP = 24,
    AUTH_ERROR_FLASH_UNLOCK = 25,
    AUTH_ERROR_FLASH_WRP = 26,
    AUTH_ERROR_FLASH_ALIGNMENT = 27,
    AUTH_ERROR_FLASH_PROGRAM = 28,
    AUTH_ERROR_FLASH_VERIFY = 29,
    AUTH_ERROR_FLASH_ERASE = 30,
    AUTH_ERROR_FLASH_SIZE_WRITE = 31,
    AUTH_ERROR_FLASH_SEQUENCE = 32
} auth_error_t;

typedef struct {
    uint8_t k_auth[32];
    uint8_t k_attest[32];
    uint8_t reference_vs[32];
    uint32_t counter;
} auth_material_t;

typedef struct {
    void *user;

    /* Instance backends own protected storage, entropy, time, atomicity, and UI. */
    int (*load)(void *user, auth_material_t *material);

    /* Positive means durable success; zero is generic failure; a negative value
     * is an auth_error_t. */
    int (*save_counter)(void *user, uint32_t counter);

    int (*random)(void *user, uint8_t *out, size_t len);
    uint32_t (*now_ms)(void *user);

    uintptr_t (*enter_atomic)(void *user);
    void (*leave_atomic)(void *user, uintptr_t previous);

    void (*ui)(void *user, auth_phase_t phase);
    void (*error)(void *user, auth_error_t error);
} auth_platform_t;

typedef struct {
    void *user;

    /* The selected instance supplies one bounded byte-stream transport. */
    /* read: 1 byte, 0 no data, -1 hardware error; write: complete or failure. */
    int (*read)(void *user, uint8_t *byte);
    int (*write)(void *user, const uint8_t *bytes, size_t len, uint32_t timeout_ms);
} auth_link_t;

static inline int auth_expired(uint32_t now, uint32_t start, uint32_t duration) {
    return (uint32_t)(now - start) >= duration;
}

static inline void auth_ui(const auth_platform_t *p, auth_phase_t phase) {
    if (p->ui)
        p->ui(p->user, phase);
}

static inline void auth_error(const auth_platform_t *p, auth_error_t error) {
    if (p->error)
        p->error(p->user, error);
    else
        auth_ui(p, AUTH_UI_ERROR);
}

#endif
