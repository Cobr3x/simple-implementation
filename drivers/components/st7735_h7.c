/* Generative AI disclosure: Drafted with Microsoft Copilot;
 * revised with OpenAI Codex (SOL model). */

#include "board.h"
#include <string.h>

static SPI_HandleTypeDef spi;
static int lcd_ready;
static const char *const error_labels[] = {
    "E19 INTERNAL",    "E01 CLOCK",      "E02 UART INIT",   "E03 RNG",         "E04 LCD",
    "E05 PROTECT",     "E06 PROVISION",  "E07 JOURNAL",     "E08 UART RX",     "E09 FRAME",
    "E10 TYPE",        "E11 LENGTH",     "E12 STALE",       "E13 AUTH",        "E14 COUNTER",
    "E15 MEASURE",     "E16 RESPONSE",   "E17 CHALLENGE",   "E18 UART TX",     "E19 INTERNAL",
    "E20 FLASH SIZE",  "E21 BANK MAP",   "E22 RDP",         "E23 CODE WRP",    "E24 KEY WRP",
    "E25 FLASH OPEN",  "E26 FLASH WRP",  "E27 FLASH ALIGN", "E28 FLASH WRITE", "E29 FLASH CHECK",
    "E30 FLASH ERASE", "E31 WRITE SIZE", "E32 FLASH SEQ"};

/* HannStar 160x80 panel initialization; SPI4 uses the APB2 clock domain. */
static int transfer(int data, const uint8_t *bytes, size_t len) {
    HAL_GPIO_WritePin(AUTH_LCD_PORT, AUTH_LCD_DC, data ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(AUTH_LCD_PORT, AUTH_LCD_CS, GPIO_PIN_RESET);
    int ok = HAL_SPI_Transmit(&spi, (uint8_t *)bytes, (uint16_t)len, 100) == HAL_OK;
    HAL_GPIO_WritePin(AUTH_LCD_PORT, AUTH_LCD_CS, GPIO_PIN_SET);
    return ok;
}

static int command(uint8_t cmd, const uint8_t *bytes, size_t len) {
    return transfer(0, &cmd, 1) && (!len || transfer(1, bytes, len));
}

static int window(unsigned x, unsigned y, unsigned w, unsigned h) {
    x += AUTH_LCD_X_OFFSET;
    y += AUTH_LCD_Y_OFFSET;
    uint8_t columns[] = {0, (uint8_t)x, 0, (uint8_t)(x + w - 1)};
    uint8_t rows[] = {0, (uint8_t)y, 0, (uint8_t)(y + h - 1)};
    return command(0x2a, columns, 4) && command(0x2b, rows, 4) && command(0x2c, NULL, 0);
}

static int fill(uint16_t color) {
    uint8_t row[320];
    for (unsigned i = 0; i < 160; ++i) {
        row[2 * i] = (uint8_t)(color >> 8);
        row[2 * i + 1] = (uint8_t)color;
    }
    if (!window(0, 0, 160, 80))
        return 0;
    for (unsigned i = 0; i < 80; ++i)
        if (!transfer(1, row, sizeof(row)))
            return 0;
    return 1;
}

static const uint8_t letters[26][7] = {
    {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30}, {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30}, {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 23, 17, 17, 15}, {17, 17, 17, 31, 17, 17, 17}, {14, 4, 4, 4, 4, 4, 14},
    {7, 2, 2, 2, 18, 18, 12},     {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17}, {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16}, {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},       {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};
static const uint8_t digits[10][7] = {{14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
                                      {14, 17, 1, 2, 4, 8, 31},     {30, 1, 1, 14, 1, 1, 30},
                                      {2, 6, 10, 18, 31, 2, 2},     {31, 16, 16, 30, 1, 1, 30},
                                      {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
                                      {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14}};

static int text(unsigned y, const char *s, uint16_t color) {
    unsigned x = 4;
    while (*s && x + 12 <= 160) {
        uint8_t pixels[12 * 14 * 2];
        char c = *s++;
        for (unsigned row = 0; row < 14; ++row)
            for (unsigned col = 0; col < 12; ++col) {
                uint8_t bits = c >= 'A' && c <= 'Z'   ? letters[c - 'A'][row / 2]
                               : c >= '0' && c <= '9' ? digits[c - '0'][row / 2]
                                                      : 0;
                int on = col < 10 && (bits & (1u << (4 - col / 2)));
                uint16_t pixel = on ? color : 0;
                unsigned i = (row * 12 + col) * 2;
                pixels[i] = (uint8_t)(pixel >> 8);
                pixels[i + 1] = (uint8_t)pixel;
            }
        if (!window(x, y, 12, 14) || !transfer(1, pixels, sizeof(pixels)))
            return 0;
        x += 12;
    }
    return 1;
}

int board_lcd_init(void) {
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SPI4_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = AUTH_LCD_BACKLIGHT | AUTH_LCD_CS | AUTH_LCD_DC;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(AUTH_LCD_PORT, &g);
    HAL_GPIO_WritePin(AUTH_LCD_PORT, AUTH_LCD_CS, GPIO_PIN_SET);
    HAL_GPIO_WritePin(AUTH_LCD_PORT, AUTH_LCD_BACKLIGHT, GPIO_PIN_SET);
    g.Pin = AUTH_LCD_SCK | AUTH_LCD_MOSI;
    g.Mode = GPIO_MODE_AF_PP;
    g.Alternate = AUTH_LCD_AF;
    HAL_GPIO_Init(AUTH_LCD_PORT, &g);
    RCC_PeriphCLKInitTypeDef clock = {0};
    clock.PeriphClockSelection = RCC_PERIPHCLK_SPI4;
    clock.Spi45ClockSelection = RCC_SPI45CLKSOURCE_D2PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&clock) != HAL_OK)
        return 0;
    spi.Instance = SPI4;
    spi.Init.Mode = SPI_MODE_MASTER;
    spi.Init.Direction = SPI_DIRECTION_1LINE;
    spi.Init.DataSize = SPI_DATASIZE_8BIT;
    spi.Init.CLKPolarity = SPI_POLARITY_LOW;
    spi.Init.CLKPhase = SPI_PHASE_1EDGE;
    spi.Init.NSS = SPI_NSS_SOFT;
    spi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    spi.Init.FirstBit = SPI_FIRSTBIT_MSB;
    spi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    spi.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
    if (HAL_SPI_Init(&spi) != HAL_OK)
        return 0;
    if (!command(0x01, NULL, 0))
        return 0;
    HAL_Delay(150);
    if (!command(0x11, NULL, 0))
        return 0;
    HAL_Delay(120);
    const uint8_t init[][7] = {{0xb1, 3, 1, 0x2c, 0x2d},
                               {0xb2, 3, 1, 0x2c, 0x2d},
                               {0xb4, 1, 7},
                               {0xc0, 3, 0xa2, 2, 0x84},
                               {0xc1, 1, 0xc5},
                               {0xc2, 2, 0x0a, 0},
                               {0xc3, 2, 0x8a, 0x2a},
                               {0xc4, 2, 0x8a, 0xee},
                               {0xc5, 1, 0x0e},
                               {0x3a, 1, 5},
                               {0x36, 1, 0xa8}};
    for (unsigned i = 0; i < sizeof(init) / sizeof(init[0]); ++i)
        if (!command(init[i][0], init[i] + 2, init[i][1]))
            return 0;
    const uint8_t partial[] = {1, 0x2c, 0x2d, 1, 0x2c, 0x2d};
    const uint8_t positive[] = {2,    0x1c, 7,    0x12, 0x37, 0x32, 0x29, 0x2d,
                                0x29, 0x25, 0x2b, 0x39, 0,    1,    3,    0x10};
    const uint8_t negative[] = {3,    0x1d, 7,    6,    0x2e, 0x2c, 0x29, 0x2d,
                                0x2e, 0x2e, 0x37, 0x3f, 0,    0,    2,    0x10};
    if (!command(0xb3, partial, sizeof(partial)) || !command(0xe0, positive, sizeof(positive)) ||
        !command(0xe1, negative, sizeof(negative)) || !command(0x21, NULL, 0) ||
        !command(0x13, NULL, 0) || !command(0x29, NULL, 0) || !fill(0))
        return 0;
    HAL_Delay(20);
    /* The P-channel backlight MOSFET is active-low. */
    HAL_GPIO_WritePin(AUTH_LCD_PORT, AUTH_LCD_BACKLIGHT, GPIO_PIN_RESET);
    lcd_ready = 1;
    return 1;
}

void board_lcd_phase(auth_phase_t phase) {
    static const char *const labels[] = {"READY",    "REQUEST",  "MEASURING", "PASS",
                                         "MISMATCH", "REJECTED", "TIMEOUT",   "ERROR"};
    if (!lcd_ready || (unsigned)phase >= sizeof(labels) / sizeof(labels[0]))
        return;
    uint16_t color = phase == AUTH_UI_PASS ? 0x07e0 : phase >= AUTH_UI_MISMATCH ? 0xf800 : 0xffe0;
    int ok = fill(0) && text(4, "SIMPLE PROVER", 0xffff) && text(34, labels[phase], color);
    if (ok && phase == AUTH_UI_READY) {
        char counter[10] = "C00000000";
        uint32_t value = board_loaded_counter;
        for (unsigned i = 0; i < 8; ++i) {
            unsigned nibble = (value >> (28u - 4u * i)) & 15u;
            counter[i + 1] = (char)(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
        }
        ok = text(56, counter, 0x07ff);
    }
    if (!ok)
        lcd_ready = 0;
}

void board_lcd_error(auth_error_t error) {
    unsigned index = (unsigned)error;
    if (!lcd_ready)
        return;
    if (index >= sizeof(error_labels) / sizeof(error_labels[0]))
        index = AUTH_ERROR_INTERNAL;
    if (!fill(0) || !text(4, "SIMPLE PROVER", 0xffff) || !text(34, error_labels[index], 0xf800))
        lcd_ready = 0;
}

void board_lcd_peer_error(auth_error_t error) {
    unsigned index = (unsigned)error;
    if (!lcd_ready)
        return;
    if (index >= sizeof(error_labels) / sizeof(error_labels[0]))
        index = AUTH_ERROR_INTERNAL;
    if (!fill(0) || !text(4, "VERIFIER", 0xffff) || !text(34, error_labels[index], 0xf800))
        lcd_ready = 0;
}
