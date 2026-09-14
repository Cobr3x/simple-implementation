/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#include "board.h"
#include "auth_transport.h"
#include <limits.h>

#ifndef AUTH_CLOCK_170MHZ_HSE
#define AUTH_CLOCK_170MHZ_HSE 0
#endif
#ifndef AUTH_FLASH_LATENCY
#define AUTH_FLASH_LATENCY FLASH_LATENCY_2
#endif
#ifndef AUTH_CODE_MPU_SUBREGIONS
#define AUTH_CODE_MPU_SUBREGIONS 0u
#endif

static UART_HandleTypeDef uart;
static RNG_HandleTypeDef rng;
static int timer_ready;
static int uart_ready;
volatile auth_phase_t board_phase = AUTH_UI_READY;
volatile auth_error_t board_error_code = AUTH_ERROR_NONE;
volatile uint32_t board_last_elapsed_ms;
volatile uint32_t board_loaded_counter;

uint32_t HAL_GetTick(void) { return timer_ready ? TIM2->CNT : uwTick; }

void SysTick_Handler(void) { HAL_IncTick(); }

static uint32_t now_ms(void *u) {
    (void)u;
    return HAL_GetTick();
}

static uintptr_t enter_atomic(void *u) {
    (void)u;
    uint32_t saved = __get_PRIMASK();
    __disable_irq();
    __DSB();
    __ISB();
    return saved;
}

static void leave_atomic(void *u, uintptr_t saved) {
    (void)u;
    __DSB();
    __ISB();
    __set_PRIMASK((uint32_t)saved);
}

void board_flash_access(int writable) {
    /* Keep code and provisioning read-only while opening the counter journals. */
    MPU_Region_InitTypeDef r = {0};
    HAL_MPU_Disable();
    r.Enable = MPU_REGION_ENABLE;
    r.Number = MPU_REGION_NUMBER3;
    r.BaseAddress = AUTH_JOURNAL_A;
    r.Size = AUTH_JOURNAL_MPU_SIZE;
    r.AccessPermission = writable ? MPU_REGION_PRIV_RW : MPU_REGION_PRIV_RO;
    r.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    r.TypeExtField = MPU_TEX_LEVEL1;
    r.IsShareable = MPU_ACCESS_SHAREABLE;
    r.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    r.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    HAL_MPU_ConfigRegion(&r);
#if AUTH_JOURNAL_SECOND_REGION
    r.Number = MPU_REGION_NUMBER4;
    r.BaseAddress = AUTH_JOURNAL_B;
    HAL_MPU_ConfigRegion(&r);
#endif
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void memory_protection(void) {
    MPU_Region_InitTypeDef r = {0};
    HAL_MPU_Disable();
    r.Enable = MPU_REGION_ENABLE;
    r.Number = MPU_REGION_NUMBER0;
    r.TypeExtField = MPU_TEX_LEVEL1;
    r.BaseAddress = 0x20000000u;
    r.Size = MPU_REGION_SIZE_512MB;
    r.AccessPermission = MPU_REGION_PRIV_RW;
    r.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    r.IsShareable = MPU_ACCESS_SHAREABLE;
    HAL_MPU_ConfigRegion(&r);
    r.Number = MPU_REGION_NUMBER1;
    r.BaseAddress = 0x08000000u;
    r.Size = MPU_REGION_SIZE_16MB;
    r.AccessPermission = MPU_REGION_PRIV_RO;
    r.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    HAL_MPU_ConfigRegion(&r);
    r.Number = MPU_REGION_NUMBER2;
    r.Size = AUTH_CODE_MPU_SIZE;
    r.SubRegionDisable = AUTH_CODE_MPU_SUBREGIONS;
    r.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&r);
    board_flash_access(0);
}

static int clock_init(void) {
    RCC_OscInitTypeDef o = {0};
    RCC_ClkInitTypeDef c = {0};
#if AUTH_CLOCK_170MHZ_HSE
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
        return 0;
    o.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    o.HSEState = RCC_HSE_ON;
    o.HSI48State = RCC_HSI48_ON;
    o.PLL.PLLState = RCC_PLL_ON;
    o.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    o.PLL.PLLM = RCC_PLLM_DIV2;
    o.PLL.PLLN = 85;
    o.PLL.PLLP = RCC_PLLP_DIV2;
    o.PLL.PLLQ = RCC_PLLQ_DIV2;
    o.PLL.PLLR = RCC_PLLR_DIV2;
#else
    o.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    o.HSIState = RCC_HSI_ON;
    o.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    o.HSI48State = RCC_HSI48_ON;
    o.PLL.PLLState = RCC_PLL_NONE;
#endif
    if (HAL_RCC_OscConfig(&o) != HAL_OK)
        return 0;
    c.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
#if AUTH_CLOCK_170MHZ_HSE
    c.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
#else
    c.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
#endif
    c.AHBCLKDivider = RCC_SYSCLK_DIV1;
    c.APB1CLKDivider = RCC_HCLK_DIV1;
    c.APB2CLKDivider = RCC_HCLK_DIV1;
#if AUTH_STM32_H7
    c.ClockType |= RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_D3PCLK1;
    c.SYSCLKDivider = RCC_SYSCLK_DIV1;
    c.APB3CLKDivider = RCC_HCLK_DIV1;
    c.APB4CLKDivider = RCC_HCLK_DIV1;
#endif
    if (HAL_RCC_ClockConfig(&c, AUTH_FLASH_LATENCY) != HAL_OK)
        return 0;
    RCC_PeriphCLKInitTypeDef p = {0};
    p.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    p.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
    return HAL_RCCEx_PeriphCLKConfig(&p) == HAL_OK;
}

static int link_read(void *u, uint8_t *b) {
    (void)u;
    uint32_t status = uart.Instance->ISR;
    if (status & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE)) {
        uart.Instance->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
        __HAL_UART_SEND_REQ(&uart, UART_RXDATA_FLUSH_REQUEST);
        return -1;
    }
    if (!(status & USART_ISR_RXNE_RXFNE))
        return 0;
    *b = (uint8_t)uart.Instance->RDR;
    return 1;
}

static int link_write(void *u, const uint8_t *bytes, size_t len, uint32_t timeout) {
    (void)u;
    return bytes && len <= UINT16_MAX &&
           HAL_UART_Transmit(&uart, (uint8_t *)bytes, (uint16_t)len, timeout) == HAL_OK;
}

static int random_bytes(void *u, uint8_t *out, size_t len) {
    (void)u;
    if (!out)
        return 0;
    while (len) {
        uint32_t word;
        if (HAL_RNG_GenerateRandomNumber(&rng, &word) != HAL_OK)
            return 0;
        for (unsigned i = 0; i < 4 && len; ++i, --len) {
            *out++ = (uint8_t)word;
            word >>= 8;
        }
    }
    return 1;
}

int board_init(void) {
    HAL_Init();
    if (!clock_init()) {
        board_report_error(NULL, AUTH_ERROR_CLOCK);
        return 0;
    }
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->PSC = HAL_RCC_GetPCLK1Freq() / 1000u - 1u;
    TIM2->ARR = UINT32_MAX;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 = TIM_CR1_CEN;
    timer_ready = 1;
    AUTH_GPIO_CLOCK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = AUTH_LED_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(AUTH_LED_PORT, &g);
    HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_OFF);
#if AUTH_HAS_LCD
    if (!board_lcd_init()) {
        board_report_error(NULL, AUTH_ERROR_LCD);
        return 0;
    }
#endif
    AUTH_UART_CLOCK_ENABLE();
    g.Pin = AUTH_UART_TX | AUTH_UART_RX;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = AUTH_UART_AF;
    HAL_GPIO_Init(AUTH_UART_PORT, &g);
    uart.Instance = AUTH_UART;
    uart.Init.BaudRate = AUTH_BAUD;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX;
    uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&uart) != HAL_OK) {
        board_report_error(NULL, AUTH_ERROR_UART_INIT);
        return 0;
    }
    uart_ready = 1;
    __HAL_RCC_RNG_CLK_ENABLE();
    rng.Instance = RNG;
    rng.Init.ClockErrorDetection = RNG_CED_ENABLE;
    if (HAL_RNG_Init(&rng) != HAL_OK) {
        board_report_error(NULL, AUTH_ERROR_RNG);
        return 0;
    }
    memory_protection();
    return 1;
}

void board_ui(void *user, auth_phase_t phase) {
    (void)user;
    board_phase = phase;
    if (phase != AUTH_UI_ERROR)
        board_error_code = AUTH_ERROR_NONE;
    int active = phase == AUTH_UI_PASS;
#if !AUTH_ROLE_PROVER
    active = active || phase == AUTH_UI_REQUEST;
#endif
    HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, active ? AUTH_LED_ON : AUTH_LED_OFF);
#if AUTH_HAS_LCD
    board_lcd_phase(phase);
#endif
}

void board_report_error(void *user, auth_error_t error) {
    (void)user;
    board_error_code = error ? error : AUTH_ERROR_INTERNAL;
    board_phase = AUTH_UI_ERROR;
    HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_OFF);
#if AUTH_HAS_LCD
    board_lcd_error(board_error_code);
#endif
}

void board_peer_error(auth_error_t error) {
#if AUTH_HAS_LCD
    board_phase = AUTH_UI_ERROR;
    board_lcd_peer_error(error);
#else
    (void)error;
#endif
}

auth_platform_t board_platform(void) {
    return (auth_platform_t){NULL,         board_store_load, board_store_save,
                             random_bytes, now_ms,           enter_atomic,
                             leave_atomic, board_ui,         board_report_error};
}

auth_link_t board_link(void) { return (auth_link_t){NULL, link_read, link_write}; }

void board_fatal(auth_error_t error) {
#if !AUTH_HAS_LCD
    AUTH_GPIO_CLOCK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = AUTH_LED_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(AUTH_LED_PORT, &g);
#endif
    board_report_error(NULL, error ? error : board_error_code);
#if AUTH_HAS_LCD
    for (;;) {
        HAL_GPIO_TogglePin(AUTH_LED_PORT, AUTH_LED_PIN);
        HAL_Delay(200);
    }
#else
    unsigned code = (unsigned)board_error_code;
    for (;;) {
        if (uart_ready && code > 0u && code <= UINT8_MAX) {
            uint8_t frame[] = {0u, 4u, AUTH_FRAME_DIAGNOSTIC, 1u, (uint8_t)code, 0u};
            (void)HAL_UART_Transmit(&uart, frame, sizeof(frame), 100u);
        }
        HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_ON);
        HAL_Delay(900);
        HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_OFF);
        HAL_Delay(500);
        for (unsigned i = 0; i < code / 10u; ++i) {
            HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_ON);
            HAL_Delay(400);
            HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_OFF);
            HAL_Delay(200);
        }
        HAL_Delay(500);
        for (unsigned i = 0; i < code % 10u; ++i) {
            HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_ON);
            HAL_Delay(120);
            HAL_GPIO_WritePin(AUTH_LED_PORT, AUTH_LED_PIN, AUTH_LED_OFF);
            HAL_Delay(180);
        }
        HAL_Delay(1500);
    }
#endif
}

void HardFault_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}

void MemManage_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}

void BusFault_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}

void UsageFault_Handler(void) {
    __disable_irq();
    for (;;) {
    }
}

void _init(void) {}

void _fini(void) {}
