/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32h7xx_hal.h"

#define AUTH_STM32_H7 1
#define AUTH_HAS_LCD 1

#define AUTH_ROLE_PROVER 1
#define AUTH_ROLE_ID 1u

#define AUTH_MEASURE_START 0x08000000u
#define AUTH_MEASURE_LENGTH 0x20000u

#define AUTH_KEY_ADDRESS 0x081A0000u
#define AUTH_JOURNAL_A 0x081C0000u
#define AUTH_JOURNAL_B 0x081E0000u
#define AUTH_JOURNAL_BLOCK_SIZE 0x20000u

#define AUTH_LED_PORT GPIOE
#define AUTH_LED_PIN GPIO_PIN_3
#define AUTH_LED_ON GPIO_PIN_RESET
#define AUTH_LED_OFF GPIO_PIN_SET

#define AUTH_UART USART2
#define AUTH_UART_AF GPIO_AF7_USART2
#define AUTH_UART_TX GPIO_PIN_2
#define AUTH_UART_RX GPIO_PIN_3
#define AUTH_UART_PORT GPIOA
#define AUTH_BAUD 115200u

#define AUTH_TIMEOUT_MS 5000u
#define AUTH_INTERVAL_MS 60000u
#define AUTH_FRAME_TIMEOUT_MS 250u

#define AUTH_FLASH_KIB 2048u
#define AUTH_JOURNAL_BANK FLASH_BANK_2
#define AUTH_JOURNAL_UNIT_A FLASH_SECTOR_6
#define AUTH_JOURNAL_UNIT_B FLASH_SECTOR_7
#define AUTH_CODE_MPU_SIZE MPU_REGION_SIZE_128KB

#define AUTH_UART_CLOCK_ENABLE() __HAL_RCC_USART2_CLK_ENABLE()
#define AUTH_GPIO_CLOCK_ENABLE()                                                                   \
    do {                                                                                           \
        __HAL_RCC_GPIOA_CLK_ENABLE();                                                              \
        __HAL_RCC_GPIOC_CLK_ENABLE();                                                              \
        __HAL_RCC_GPIOE_CLK_ENABLE();                                                              \
    } while (0)
#define AUTH_JOURNAL_MPU_SIZE MPU_REGION_SIZE_256KB
#define AUTH_JOURNAL_SECOND_REGION 0
#define AUTH_CODE_WRP OB_WRP_SECTOR_0
#define AUTH_KEY_WRP OB_WRP_SECTOR_5

#define AUTH_LCD_PORT GPIOE
#define AUTH_LCD_CS GPIO_PIN_11
#define AUTH_LCD_DC GPIO_PIN_13
#define AUTH_LCD_BACKLIGHT GPIO_PIN_10
#define AUTH_LCD_SCK GPIO_PIN_12
#define AUTH_LCD_MOSI GPIO_PIN_14
#define AUTH_LCD_AF GPIO_AF5_SPI4
#define AUTH_LCD_X_OFFSET 1u
#define AUTH_LCD_Y_OFFSET 26u

#endif
