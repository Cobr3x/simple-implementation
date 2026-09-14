/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32g4xx_hal.h"

#define AUTH_STM32_H7 0
#define AUTH_CLOCK_170MHZ_HSE 1
#define AUTH_FLASH_LATENCY FLASH_LATENCY_4
#define AUTH_HAS_LCD 0

#define AUTH_ROLE_PROVER 0
#define AUTH_ROLE_ID 2u

#define AUTH_MEASURE_START 0x08000000u
#define AUTH_MEASURE_LENGTH 0x20000u

#define AUTH_KEY_ADDRESS 0x0801F800u
#define AUTH_JOURNAL_A 0x0801C000u
#define AUTH_JOURNAL_B 0x0801C800u
#define AUTH_JOURNAL_BLOCK_SIZE 0x800u

#define AUTH_LED_PORT GPIOC
#define AUTH_LED_PIN GPIO_PIN_6
#define AUTH_LED_ON GPIO_PIN_SET
#define AUTH_LED_OFF GPIO_PIN_RESET

#define AUTH_UART USART2
#define AUTH_UART_AF GPIO_AF7_USART2
#define AUTH_UART_TX GPIO_PIN_2
#define AUTH_UART_RX GPIO_PIN_3
#define AUTH_UART_PORT GPIOA
#define AUTH_BAUD 115200u

#define AUTH_TIMEOUT_MS 5000u
#define AUTH_INTERVAL_MS 60000u
#define AUTH_FRAME_TIMEOUT_MS 250u

#define AUTH_FLASH_KIB 128u
#define AUTH_JOURNAL_BANK FLASH_BANK_1
#define AUTH_JOURNAL_UNIT_A 56u
#define AUTH_JOURNAL_UNIT_B 57u
#define AUTH_CODE_MPU_SIZE MPU_REGION_SIZE_128KB
#define AUTH_CODE_MPU_SUBREGIONS 0x80u

#define AUTH_UART_CLOCK_ENABLE() __HAL_RCC_USART2_CLK_ENABLE()
#define AUTH_GPIO_CLOCK_ENABLE()                                                                   \
    do {                                                                                           \
        __HAL_RCC_GPIOA_CLK_ENABLE();                                                              \
        __HAL_RCC_GPIOC_CLK_ENABLE();                                                              \
    } while (0)
#define AUTH_JOURNAL_MPU_SIZE MPU_REGION_SIZE_2KB
#define AUTH_JOURNAL_SECOND_REGION 1
#define AUTH_FLASH_REQUIRE_DBANK 0
#define AUTH_CODE_WRP_AREA OB_WRPAREA_BANK1_AREAA
#define AUTH_CODE_WRP_START 0u
#define AUTH_CODE_WRP_END 55u

#define AUTH_KEY_WRP_AREA OB_WRPAREA_BANK1_AREAB
#define AUTH_KEY_WRP_START 63u
#define AUTH_KEY_WRP_END 63u

#endif
