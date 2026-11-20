#include "board_config.h"
#if APP_DIAGNOSTIC_MODE
#include "board_diagnostics.h"
#include "main.h"
#include "dht20.h"
#include "lcd.h"
#include "i2c_scanner.h"
extern I2C_HandleTypeDef hi2c1;
volatile board_diagnostics_t g_board_diag;
static uint32_t sensor_at, lcd_at, heartbeat_at;

static void release_lights(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = TL1_A_Pin | TL1_B_Pin | TL2_A_Pin | TL2_B_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOB, &gpio);
}

void board_diagnostics_init(void)
{
    const board_diagnostics_t empty = {0};
    g_board_diag = empty;
    release_lights();
    sensor_at = lcd_at = heartbeat_at = HAL_GetTick();
    dht20_init();
    lcd_init();
    i2c_scanner_init(&hi2c1);
}

void board_diagnostics_service(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t command = g_board_diag.command;
    g_board_diag.alive++;
    g_board_diag.tick_ms = now;
    g_board_diag.raw_buttons[0] = HAL_GPIO_ReadPin(BTN_1_GPIO_Port, BTN_1_Pin);
    g_board_diag.raw_buttons[1] = HAL_GPIO_ReadPin(BTN_2_GPIO_Port, BTN_2_Pin);
    g_board_diag.raw_buttons[2] = HAL_GPIO_ReadPin(BTN_3_GPIO_Port, BTN_3_Pin);
    g_board_diag.raw_buttons[3] = HAL_GPIO_ReadPin(BTN_4_GPIO_Port, BTN_4_Pin);

    /* Once a manual test has started, changing road/code in the debugger
     * reapplies it without requiring another command=2. Command=3 stops it.
     */
    if (command == 0 && g_board_diag.last_command == 2 &&
        (g_board_diag.road != g_board_diag.applied_road ||
         g_board_diag.code != g_board_diag.applied_code)) {
        command = 2;
    }
    if (command != 0) {
        uint32_t road = g_board_diag.road;
        uint32_t code = g_board_diag.code;
        g_board_diag.command = 0;
        g_board_diag.last_command = command;
        g_board_diag.result = 0;
        if (command == 1) {
            i2c_scanner_init(&hi2c1);
        } else if (command == 2 && (road == 1 || road == 2) && code <= 3) {
            GPIO_InitTypeDef gpio = {0};
            uint32_t a = TL1_A_Pin, b = TL1_B_Pin;
            uint32_t set = 0;
            release_lights();
            if (road == 2) { a = TL2_A_Pin; b = TL2_B_Pin; }
            if (code & 2) set |= a;
            if (code & 1) set |= b;
            /* Preload the selected code before enabling outputs. */
            GPIOB->BSRR = set | (((a | b) & ~set) << 16);
            gpio.Pin = a | b;
            gpio.Mode = GPIO_MODE_OUTPUT_PP;
            gpio.Pull = GPIO_NOPULL;
            gpio.Speed = GPIO_SPEED_FREQ_LOW;
            HAL_GPIO_Init(GPIOB, &gpio);
            g_board_diag.applied_road = road;
            g_board_diag.applied_code = code;
            g_board_diag.applied_count++;
            if ((GPIOB->ODR & (a | b)) != set) {
                g_board_diag.result = 2;
            }
        } else if (command == 3) {
            release_lights();
        } else {
            g_board_diag.result = 1;
        }
    }
    g_board_diag.gpio_b_odr = GPIOB->ODR;
    g_board_diag.gpio_b_idr = GPIOB->IDR;
    g_board_diag.gpio_b_crl = GPIOB->CRL;
    g_board_diag.gpio_b_crh = GPIOB->CRH;
    if (g_board_diag.applied_road == 1 || g_board_diag.applied_road == 2) {
        uint32_t a = TL1_A_Pin, b = TL1_B_Pin;
        if (g_board_diag.applied_road == 2) { a = TL2_A_Pin; b = TL2_B_Pin; }
        g_board_diag.pin_code = 0;
        if (GPIOB->IDR & a) g_board_diag.pin_code |= 2;
        if (GPIOB->IDR & b) g_board_diag.pin_code |= 1;
    }
    if ((uint32_t)(now - heartbeat_at) >= 500) {
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        heartbeat_at = now;
    }
    if (!g_i2c_scan.complete) {
        i2c_scanner_service();
        return;
    }
    if ((uint32_t)(now - sensor_at) >= 100) {
        sensor_at = now;
        dht20_service();
    }
    if ((uint32_t)(now - lcd_at) >= 200) {
        lcd_at = now;
        lcd_service();
    }
}
#endif
