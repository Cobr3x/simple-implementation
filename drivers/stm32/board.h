#ifndef AUTH_BOARD_H
#define AUTH_BOARD_H

#include "auth_platform.h"
#include "board_config.h"

extern volatile auth_phase_t board_phase;
extern volatile auth_error_t board_error_code;
extern volatile uint32_t board_last_elapsed_ms;
extern volatile uint32_t board_loaded_counter;

int board_init(void);
void board_fatal(auth_error_t error);

auth_platform_t board_platform(void);
auth_link_t board_link(void);

void board_ui(void *user, auth_phase_t phase);
void board_report_error(void *user, auth_error_t error);
void board_peer_error(auth_error_t error);

int board_store_load(void *user, auth_material_t *m);
int board_store_save(void *user, uint32_t counter);
void board_flash_access(int writable);

#if AUTH_HAS_LCD
int board_lcd_init(void);
void board_lcd_phase(auth_phase_t phase);
void board_lcd_error(auth_error_t error);
void board_lcd_peer_error(auth_error_t error);
#endif

#endif
