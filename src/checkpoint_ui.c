#include "checkpoint_ui.h"
#include "FreeRTOS.h"
#include "task.h"
/*
 * LCD hardware/initialization adapted from RT-Thread's Spark-1 BSP:
 * libraries/Board_Drivers/lcd/drv_lcd.c and
 * projects/03_driver_lcd/board/CubeMX_Config/Src/stm32f4xx_hal_msp.c
 * https://github.com/RT-Thread-Studio/sdk-bsp-stm32f407-spark
 * Copyright (c) 2006-2022, RT-Thread Development Team
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stm32f4xx_hal.h"

/* NE3, 8-bit bus: A18 low selects commands; A18 high selects data. */
#define LCD_COMMAND (*(volatile uint8_t *)0x6803FFFEUL)
#define LCD_DATA    (*(volatile uint8_t *)0x68040000UL)

static void lcd_command(uint8_t command, const uint8_t *data, uint32_t count)
{
    LCD_COMMAND = command;
    for (uint32_t i = 0; i < count; ++i)
    {
        LCD_DATA = data[i];
    }
}

static HAL_StatusTypeDef lcd_bus_init(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE();

    /* Keep backlight off and hold the panel in reset during bus setup. */
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOF, &gpio);
    gpio.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOD, &gpio);

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;
    /* D0=PD14, D1=PD15, D2=PD0, D3=PD1; RD=PD4, WR=PD5, A18=PD13. */
    gpio.Pin = GPIO_PIN_14 | GPIO_PIN_15 | GPIO_PIN_0 | GPIO_PIN_1 |
               GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_13;
    HAL_GPIO_Init(GPIOD, &gpio);
    /* D4=PE7, D5=PE8, D6=PE9, D7=PE10. */
    gpio.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOE, &gpio);
    gpio.Pin = GPIO_PIN_10; /* NE3 */
    HAL_GPIO_Init(GPIOG, &gpio);

    SRAM_HandleTypeDef sram = {0};
    sram.Instance = FSMC_NORSRAM_DEVICE;
    sram.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
    sram.Init.NSBank = FSMC_NORSRAM_BANK3;
    sram.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
    sram.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
    sram.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_8;
    sram.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
    sram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
    sram.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
    sram.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
    sram.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
    sram.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
    sram.Init.ExtendedMode = FSMC_EXTENDED_MODE_ENABLE;
    sram.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
    sram.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
    sram.Init.PageSize = FSMC_PAGE_SIZE_NONE;

    /* BSP timing counts retained at the slower reset-default HSI clock. */
    FSMC_NORSRAM_TimingTypeDef read = {0};
    read.AddressSetupTime = 15;
    read.DataSetupTime = 60;
    read.AccessMode = FSMC_ACCESS_MODE_A;
    FSMC_NORSRAM_TimingTypeDef write = {0};
    write.AddressSetupTime = 9;
    write.DataSetupTime = 8;
    write.AccessMode = FSMC_ACCESS_MODE_A;
    return HAL_SRAM_Init(&sram, &read, &write);
}

static void lcd_init(void)
{
    HAL_Delay(100);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_Delay(200);

    /* Official Spark-1 ST7789V3 panel sequence; no TE interrupt or PWM. */
    lcd_command(0x36, (const uint8_t[]){0x00}, 1);
    lcd_command(0x3A, (const uint8_t[]){0x65}, 1); /* RGB565 */
    lcd_command(0xB2, (const uint8_t[]){0x0C, 0x0C, 0x00, 0x33, 0x33}, 5);
    lcd_command(0xB7, (const uint8_t[]){0x35}, 1);
    lcd_command(0xBB, (const uint8_t[]){0x37}, 1);
    lcd_command(0xC0, (const uint8_t[]){0x2C}, 1);
    lcd_command(0xC2, (const uint8_t[]){0x01}, 1);
    lcd_command(0xC3, (const uint8_t[]){0x12}, 1);
    lcd_command(0xC4, (const uint8_t[]){0x20}, 1);
    lcd_command(0xC6, (const uint8_t[]){0x0F}, 1);
    lcd_command(0xD0, (const uint8_t[]){0xA4, 0xA1}, 2);
    lcd_command(0xE0, (const uint8_t[]){
        0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
        0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}, 14);
    lcd_command(0xE1, (const uint8_t[]){
        0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
        0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}, 14);
    lcd_command(0x21, NULL, 0); /* Inversion on, as required by this panel. */
    lcd_command(0x11, NULL, 0); /* Sleep out. */
    HAL_Delay(120);
    lcd_command(0x29, NULL, 0); /* Display on. */
}

static void lcd_clear(uint16_t color)
{
    /* BSP uses x/y 0..239, with no panel offset. */
    const uint8_t range[] = {0x00, 0x00, 0x00, 0xEF};
    lcd_command(0x2A, range, sizeof(range));
    lcd_command(0x2B, range, sizeof(range));
    lcd_command(0x2C, NULL, 0);
    for (uint32_t pixel = 0; pixel < 240U * 240U; ++pixel)
    {
        /* Two byte bus cycles, high byte first, for each RGB565 pixel. */
        LCD_DATA = (uint8_t)(color >> 8);
        LCD_DATA = (uint8_t)color;
    }
}


/*
 * Matrix hardware reference: RT-Thread-Studio/sdk-bsp-stm32f407-spark:
 * libraries/Board_Drivers/led_matrix/drv_matrix_led.c and .h;
 * projects/03_driver_led_matrix/board/CubeMX_Config/Src/stm32f4xx_hal_msp.c
 * Copyright (c) 2006-2022, RT-Thread Development Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * The verified LCD checkpoint is preserved in
 * reference/lcd_checkpoint_main.c.txt; this test does not initialize the LCD.
 */
#include "stm32f4xx_hal.h"
#include <stdio.h>

#define MATRIX_PIXELS 19U
#define MATRIX_RESET_SLOTS 256U
#define MATRIX_BUFFER_SIZE (MATRIX_RESET_SLOTS + MATRIX_PIXELS * 24U + MATRIX_RESET_SLOTS)

static TIM_HandleTypeDef matrix_tim;
static DMA_HandleTypeDef matrix_dma;
/* SRAM-resident, halfword DMA transfers, with leading/trailing LOW latch time. */
static uint16_t matrix_pwm[MATRIX_BUFFER_SIZE] __attribute__((aligned(4)));
static volatile uint8_t matrix_transfer;
static uint16_t matrix_zero, matrix_one;
static uint32_t matrix_timer_hz, matrix_period_ticks;

void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&matrix_dma);
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *tim)
{
    if (tim->Instance == TIM3 && tim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        __HAL_TIM_DISABLE_DMA(tim, TIM_DMA_CC2);
        __HAL_TIM_SET_COMPARE(tim, TIM_CHANNEL_2, 0);
        matrix_transfer = 1;
    }
}

void HAL_TIM_ErrorCallback(TIM_HandleTypeDef *tim)
{
    if (tim->Instance == TIM3)
    {
        __HAL_TIM_DISABLE_DMA(tim, TIM_DMA_CC2);
        __HAL_TIM_SET_COMPARE(tim, TIM_CHANNEL_2, 0);
        matrix_transfer = 2;
    }
}

static HAL_StatusTypeDef matrix_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* HW1.1: PF2 controls the active-LOW OE of the 74LVC1G125 buffer. */
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_2, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_2;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &gpio);

    /* Official matrix example: PA7, AF2, TIM3 channel 2. */
    gpio.Pin = GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF2_TIM3;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* Preserve current clocks; APB1 timers double PCLK1 when APB1 is divided. */
    RCC_ClkInitTypeDef clocks = {0};
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    matrix_timer_hz = HAL_RCC_GetPCLK1Freq();
    if (clocks.APB1CLKDivider != RCC_HCLK_DIV1)
    {
        matrix_timer_hz *= 2U;
    }
    matrix_period_ticks = (matrix_timer_hz + 400000U) / 800000U;
    matrix_zero = (uint16_t)(((uint64_t)matrix_timer_hz * 350U + 500000000U) / 1000000000U);
    matrix_one = (uint16_t)(((uint64_t)matrix_timer_hz * 700U + 500000000U) / 1000000000U);
    if (matrix_period_ticks < 3U || matrix_period_ticks > 65536U ||
        matrix_zero == 0U || matrix_one <= matrix_zero ||
        matrix_one >= matrix_period_ticks)
    {
        return HAL_ERROR;
    }

    matrix_tim.Instance = TIM3;
    matrix_tim.Init.Prescaler = 0;
    matrix_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    matrix_tim.Init.Period = matrix_period_ticks - 1U;
    matrix_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    matrix_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&matrix_tim) != HAL_OK)
    {
        return HAL_ERROR;
    }
    TIM_OC_InitTypeDef pwm = {0};
    pwm.OCMode = TIM_OCMODE_PWM1;
    pwm.Pulse = 0;
    pwm.OCPolarity = TIM_OCPOLARITY_HIGH;
    pwm.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&matrix_tim, &pwm, TIM_CHANNEL_2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Exact onboard stream/channel from drv_matrix_led.c. */
    matrix_dma.Instance = DMA1_Stream5;
    matrix_dma.Init.Channel = DMA_CHANNEL_5;
    matrix_dma.Init.Direction = DMA_MEMORY_TO_PERIPH;
    matrix_dma.Init.PeriphInc = DMA_PINC_DISABLE;
    matrix_dma.Init.MemInc = DMA_MINC_ENABLE;
    matrix_dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    matrix_dma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    matrix_dma.Init.Mode = DMA_NORMAL;
    matrix_dma.Init.Priority = DMA_PRIORITY_HIGH;
    matrix_dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&matrix_dma) != HAL_OK)
    {
        return HAL_ERROR;
    }
    __HAL_LINKDMA(&matrix_tim, hdma[TIM_DMA_ID_CC2], matrix_dma);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

    /* Start at zero duty before enabling the board's data buffer. */
    if (HAL_TIM_PWM_Start(&matrix_tim, TIM_CHANNEL_2) != HAL_OK)
    {
        return HAL_ERROR;
    }
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_Delay(1);
    return HAL_OK;
}

static HAL_StatusTypeDef matrix_set_all_and_refresh(uint8_t red, uint8_t green, uint8_t blue)
{
    /* Leading/trailing zero slots stay zero; GRB, MSB first, as in the BSP. */
    const uint8_t grb[] = {green, red, blue};
    uint32_t slot = MATRIX_RESET_SLOTS;
    for (uint32_t pixel = 0; pixel < MATRIX_PIXELS; ++pixel)
    {
        for (uint32_t component = 0; component < 3U; ++component)
        {
            for (int bit = 7; bit >= 0; --bit)
            {
                matrix_pwm[slot++] = (grb[component] & (1U << bit))
                                         ? matrix_one : matrix_zero;
            }
        }
    }

    HAL_TIM_PWM_Stop(&matrix_tim, TIM_CHANNEL_2);
    __HAL_TIM_SET_COMPARE(&matrix_tim, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COUNTER(&matrix_tim, 0);
    /* Load the zero compare preload before enabling PWM and DMA. */
    if (HAL_TIM_GenerateEvent(&matrix_tim, TIM_EVENTSOURCE_UPDATE) != HAL_OK)
    {
        return HAL_ERROR;
    }
    matrix_transfer = 0;
    if (HAL_TIM_PWM_Start_DMA(&matrix_tim, TIM_CHANNEL_2,
                             (uint32_t *)matrix_pwm, MATRIX_BUFFER_SIZE) != HAL_OK)
    {
        return HAL_ERROR;
    }
    uint32_t start = HAL_GetTick();
    while (matrix_transfer == 0U)
    {
        if ((uint32_t)(HAL_GetTick() - start) >= 20U)
        {
            HAL_TIM_PWM_Stop_DMA(&matrix_tim, TIM_CHANNEL_2);
            return HAL_TIMEOUT;
        }
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
            vTaskDelay(pdMS_TO_TICKS(1));
    }
    return matrix_transfer == 1U ? HAL_OK : HAL_ERROR;
}


static const uint8_t font[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00},
    {0x02,0x01,0x51,0x09,0x06},
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x41,0x51,0x32},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},
    {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},
    {0x00,0x44,0x7D,0x40,0x00},
    {0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},
    {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},
    {0x7C,0x08,0x04,0x04,0x08},
    {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},
    {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
    {0x02,0x01,0x51,0x09,0x06},
};

static void lcd_rect(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    const uint8_t cols[] = {0, (uint8_t)x, 0, (uint8_t)(x+w-1)};
    const uint8_t rows[] = {0, (uint8_t)y, 0, (uint8_t)(y+h-1)};
    lcd_command(0x2A, cols, 4);
    lcd_command(0x2B, rows, 4);
    lcd_command(0x2C, NULL, 0);
    for (unsigned i=0; i<w*h; ++i) {
        LCD_DATA = (uint8_t)(color >> 8);
        LCD_DATA = (uint8_t)color;
    }
}

static void lcd_text(unsigned x, unsigned y, const char *text, uint16_t color)
{
    // Scale 2: 10x14 glyph, 12-pixel advance; supplied names fit 240 pixels.
    for (; *text && x+10<=240 && y+14<=240; ++text, x+=12) {
        unsigned ch = (unsigned char)*text;
        if (ch < 32 || ch > 126) ch = '?';
        for (unsigned col=0; col<5; ++col)
            for (unsigned row=0; row<7; ++row)
                if (font[ch-32][col] & (1U<<row))
                    lcd_rect(x+2*col, y+2*row, 2, 2, color);
    }
}

HAL_StatusTypeDef checkpoint_lcd_init(void)
{
    if (lcd_bus_init() != HAL_OK) return HAL_ERROR;
    lcd_init();
    lcd_clear(0);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);
    return HAL_OK;
}

HAL_StatusTypeDef checkpoint_matrix_init(void)
{
    return matrix_init(); /* Startup OFF frame is explicitly sent by main. */
}

HAL_StatusTypeDef checkpoint_matrix_selection(int mode)
{
    return matrix_set_all_and_refresh(mode==4 ? 20 : 0, mode==1 ? 20 : 0, mode==3 ? 20 : 0);
}

void checkpoint_lcd_render(unsigned mode, unsigned index, const char *name1, const char *name2)
{
    lcd_clear(0);
    lcd_text(12, 12, "BCA182 MP3 PLAYER", 0xFFFF);
    lcd_text(12, 46, mode==1 ? "CONFIRM SONG?" : mode==2 ? "SELECTION TIMEOUT" : mode==3 ? "PLAYING" : mode==4 ? "PAUSED" : "Current Song", 0xFFE0);
    char number[] = "Song index: 0";
    number[12] = (char)('0'+index);
    lcd_text(12, 80, number, 0xFFFF);
    lcd_text(12, 112, name1, 0xFFFF);
    lcd_text(12, 138, name2, 0xFFFF);
    if (mode==1) {
        lcd_text(12, 178, "Press UP again", 0x07E0);
        lcd_text(12, 206, "5 sec timeout", 0xFFFF);
    }
}

void checkpoint_lcd_volume(unsigned percent)
{
    // Called only by the LCD task with lcd_mutex held, outside selection screens.
    char message[24];
    snprintf(message,sizeof(message),"VOLUME: %u%%",percent);
    lcd_text(12,178,message,0xFFE0);
}

/* Premium presentation layer only; verified hardware/legacy primitives above unchanged. */
#include "player_ui_assets.h"
#define P_BG    0x1083
#define P_PANEL 0x2125
#define P_TEXT  0xFFFF
#define P_MUTED 0xA535
#define P_TRACK 0x31A7

static uint16_t p_mix(uint16_t a, uint16_t b, unsigned n, unsigned d)
{
    unsigned ar=(a>>11)&31, ag=(a>>5)&63, ab=a&31;
    unsigned br=(b>>11)&31, bg=(b>>5)&63, bb=b&31;
    return (uint16_t)((((ar*(d-n)+br*n)/d)<<11) |
                     (((ag*(d-n)+bg*n)/d)<<5) | ((ab*(d-n)+bb*n)/d));
}
static const BcaGlyph *p_glyph(const BcaFont *font, unsigned ch)
{
    if (ch<32 || ch>126) ch='?';
    return &font->glyphs[ch-32];
}
static unsigned p_width(const char *text, const BcaFont *font)
{
    unsigned width=0;
    while (*text) width+=p_glyph(font,(unsigned char)*text++)->advance;
    return width;
}
static void p_text(unsigned x, unsigned y, const char *text, const BcaFont *font,
                   uint16_t color, uint16_t background)
{
    uint16_t shades[4]={background,p_mix(background,color,1,3),p_mix(background,color,2,3),color};
    for (;*text;++text) {
        const BcaGlyph *g=p_glyph(font,(unsigned char)*text);
        if (x+g->width>240 || y+font->height>240) return;
        for (unsigned row=0;row<font->height;++row) {
            unsigned col=0;
            while (col<g->width) {
                unsigned bit=row*g->width+col;
                unsigned coverage=(font->bits[g->offset+bit/4]>>(6-2*(bit%4)))&3;
                unsigned start=col++;
                while (col<g->width) {
                    bit=row*g->width+col;
                    if (((font->bits[g->offset+bit/4]>>(6-2*(bit%4)))&3)!=coverage) break;
                    ++col;
                }
                if (coverage) lcd_rect(x+start,y+row,col-start,1,shades[coverage]);
            }
        }
        x+=g->advance;
    }
}
static void p_center(unsigned y,const char *text,const BcaFont *font,uint16_t color,uint16_t bg)
{
    unsigned width=p_width(text,font);
    p_text(width<240 ? (240-width)/2 : 0,y,text,font,color,bg);
}
static void p_disc(int cx,int cy,int radius,uint16_t color)
{
    for (int dy=-radius;dy<=radius;++dy) {
        int dx=radius;
        while (dx*dx+dy*dy>radius*radius) --dx;
        lcd_rect((unsigned)(cx-dx),(unsigned)(cy+dy),(unsigned)(2*dx+1),1,color);
    }
}
static void p_line(int x,int y,int ex,int ey,uint16_t color)
{
    int dx=ex>x ? ex-x : x-ex, sx=x<ex ? 1 : -1;
    int dy=ey>y ? y-ey : ey-y, sy=y<ey ? 1 : -1;
    int error=dx+dy;
    for (;;) {
        lcd_rect((unsigned)x,(unsigned)y,2,2,color);
        if (x==ex && y==ey) break;
        int e=2*error;
        if (e>=dy) { error+=dy; x+=sx; }
        if (e<=dx) { error+=dx; y+=sy; }
    }
}
static void p_art(unsigned index)
{
    const BcaArt *art=&bca_art[index];
    lcd_rect(59,28,122,122,p_mix(P_BG,art->accent,1,9));
    lcd_rect(62,30,116,116,0x0841);
    /* Subtle three-row color bands form the square artwork background. */
    for (unsigned row=0;row<112;row+=3)
        lcd_rect(64,32+row,112,row+3>112 ? 112-row : 3,p_mix(art->top,art->bottom,row,111));
    for (unsigned i=0;i<art->count;++i) {
        const BcaShape *s=&art->shapes[i];
        if (s->op==0) lcd_rect(64+s->x,32+s->y,s->a,s->b,s->color);
        else if (s->op==1) p_disc(64+s->x,32+s->y,s->a,s->color);
        else p_line(64+s->x,32+s->y,64+s->a,32+s->b,s->color);
    }
}
static void p_header(const CheckpointLcdView *v)
{
    uint16_t bg=p_mix(P_BG,bca_art[v->index].accent,1,14);
    char volume[8];
    lcd_rect(0,0,240,25,bg);
    p_text(12,5,v->mode==1 ? "SELECT SONG" : "BCA182 PLAYER",&bca_label,P_TEXT,bg);
    snprintf(volume,sizeof(volume),"%u%%",v->volume_percent);
    p_text(228-p_width(volume,&bca_label),5,volume,&bca_label,P_MUTED,bg);
}
static void p_hud(const CheckpointLcdView *v)
{
    uint16_t accent=bca_art[v->index].accent;
    lcd_rect(22,61,196,125,P_PANEL);
    lcd_rect(22,61,196,2,accent);
    p_center(74,"VOLUME",&bca_label,P_MUTED,P_PANEL);
    char percent[8];
    snprintf(percent,sizeof(percent),"%u%%",v->volume_percent);
    p_center(97,percent,&bca_number,P_TEXT,P_PANEL);
    for (unsigned i=0;i<5;++i)
        lcd_rect(44+i*32,151,24,9,i<v->volume_percent/20 ? accent : P_TRACK);
}
static void p_equalizer(unsigned phase,uint16_t accent)
{
    /* Decorative animation only: not a measured audio spectrum. */
    static const uint8_t heights[8][5]={
        {3,9,5,12,7},{7,4,11,6,10},{11,6,3,9,5},{5,12,8,4,11},
        {9,5,12,7,3},{4,10,6,11,8},{12,7,4,8,5},{6,3,10,5,12}
    };
    lcd_rect(184,193,36,15,P_BG);
    for (unsigned i=0;i<5;++i) {
        unsigned height=heights[phase][i];
        lcd_rect(186+i*6,207-height,3,height,accent);
    }
}
static void p_clock(unsigned x,unsigned y,uint32_t ms,int right)
{
    char text[12];
    unsigned seconds=ms/1000;
    snprintf(text,sizeof(text),"%u:%02u",seconds/60,seconds%60);
    if (right) x-=p_width(text,&bca_label);
    p_text(x,y,text,&bca_label,P_MUTED,P_BG);
}

/* Pending selection occupies a small overlay on the cover. Current title,
 * playback status, elapsed/total time and progress remain the current song. */
static void p_pending(const CheckpointLcdView *v)
{
    if ((v->mode!=0 && v->mode!=3 && v->mode!=4) || v->pending_index==v->index)
        return;
    char bits[24];
    snprintf(bits,sizeof(bits),"NEXT %u%u%u - TAP UP",(v->pending_index>>2)&1,
             (v->pending_index>>1)&1,v->pending_index&1);
    lcd_rect(12,30,216,33,P_PANEL);
    p_center(32,bits,&bca_label,P_TEXT,P_PANEL);
    p_center(47,v->pending_name ? v->pending_name : "",&bca_label,P_MUTED,P_PANEL);
}

void checkpoint_lcd_update(const CheckpointLcdView *v,const char *name1,const char *name2)
{
    static unsigned valid,last_mode,last_index,last_completed,last_volume,last_pending;
    static unsigned last_width,last_seconds,last_elapsed,last_total,last_eq;
    static uint32_t last_bar_ms;
    unsigned full=!valid || v->mode!=last_mode || v->index!=last_index ||
                  v->pending_index!=last_pending ||
                  (v->mode==0 && v->completed!=last_completed);
    uint16_t accent=bca_art[v->index].accent;
    if (full) {
        lcd_clear(P_BG);
        p_header(v);
        if (v->mode==5) p_hud(v);
        else {
            p_art(v->index);
            if (v->mode==2) {
                lcd_rect(14,90,212,65,P_PANEL);
                p_center(98,"Selection cancelled",&bca_title,P_TEXT,P_PANEL);
                p_center(130,"Current song retained",&bca_label,P_MUTED,P_PANEL);
            } else {
                p_center(*name2 ? 150 : 160,name1,&bca_title,P_TEXT,P_BG);
                if (*name2) p_center(172,name2,&bca_title,P_TEXT,P_BG);
                if (v->mode==1) {
                    char bits[4]={(char)('0'+((v->index>>2)&1)),(char)('0'+((v->index>>1)&1)),
                                  (char)('0'+(v->index&1)),0};
                    lcd_rect(82,109,76,33,P_PANEL);
                    p_center(109,bits,&bca_number,P_TEXT,P_PANEL);
                    p_center(194,"UP TO CONFIRM",&bca_label,accent,P_BG);
                } else if (v->mode==3) {
                    for (unsigned i=0;i<6;++i) lcd_rect(83+i,197+i,1,11-2*i,accent);
                    p_text(98,194,"PLAYING",&bca_label,P_TEXT,P_BG);
                } else if (v->mode==4) {
                    lcd_rect(79,120,82,19,P_PANEL);
                    lcd_rect(85,126,2,8,accent);lcd_rect(90,126,2,8,accent);
                    p_text(99,122,"PAUSED",&bca_label,P_TEXT,P_PANEL);
                    p_center(194,"HOLD UP TO RESUME",&bca_label,P_MUTED,P_BG);
                } else p_center(194,v->completed ? "PLAY COMPLETE" : "SHORT UP TO SELECT",&bca_label,P_MUTED,P_BG);
                if (v->mode!=1) {
                    char bits[16];
                    snprintf(bits,sizeof(bits),"%u%u%u   %u/8",(v->index>>2)&1,(v->index>>1)&1,
                             v->index&1,v->index+1);
                    p_center(223,bits,&bca_label,P_MUTED,P_BG);
                }
            }
        }
        p_pending(v);
        last_width=last_seconds=last_elapsed=last_total=last_eq=~0U;
    }
    if (!full && last_volume!=v->volume_percent) {
        p_header(v);
        if (v->mode==5) p_hud(v);
    }
    if (v->mode==1 || v->mode==0 || v->mode==3 || v->mode==4) {
        unsigned progress=v->mode==1 ?
            (v->confirmation_remaining_ms>5000 ? 1000 : v->confirmation_remaining_ms/5) :
            (v->progress_permille>1000 ? 1000 : v->progress_permille);
        unsigned width=(progress*208/1000)/2*2;
        if (width!=last_width && (full || v->completed || (uint32_t)(v->animation_ms-last_bar_ms)>=100)) {
            lcd_rect(12,211,216,9,P_BG);
            lcd_rect(16,215,208,2,P_TRACK);
            if (width) lcd_rect(16,215,width,2,accent);
            p_disc(16+width,215,3,accent);
            last_width=width;last_bar_ms=v->animation_ms;
        }
        if (v->mode==1) {
            unsigned seconds=(v->confirmation_remaining_ms+999)/1000;
            if (last_seconds!=seconds) {
                char text[12];
                lcd_rect(87,222,66,16,P_BG);
                snprintf(text,sizeof(text),"%u SEC",seconds);
                p_center(223,text,&bca_label,P_MUTED,P_BG);
                last_seconds=seconds;
            }
        } else {
            if (last_elapsed!=v->elapsed_ms/1000 || last_total!=v->total_ms/1000) {
                lcd_rect(12,222,47,17,P_BG);lcd_rect(181,222,47,17,P_BG);
                p_clock(16,223,v->elapsed_ms,0);p_clock(224,223,v->total_ms,1);
                last_elapsed=v->elapsed_ms/1000;last_total=v->total_ms/1000;
            }
            if (v->mode==3) {
                unsigned phase=(v->animation_ms/160)%8;
                if (phase!=last_eq) { p_equalizer(phase,accent);last_eq=phase; }
            }
        }
    }
    valid=1;last_mode=v->mode;last_index=v->index;
    last_completed=v->completed;last_volume=v->volume_percent;last_pending=v->pending_index;
}
