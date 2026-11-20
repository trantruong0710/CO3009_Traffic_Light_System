/* Host tests. GPIO, scheduler and TV1 config/router are deliberately not implemented here. */
#include "mock_hal.h"
#include "board_config.h"
#include "dht20.h"
#include "lcd.h"
#include "i2c_scanner.h"
#include "fsm_tuning.h"
#include "config.h"
#include "global.h"
#include "software_timer.h"
#include "mode_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
app_shared_state_t g_app_state = {.active_config = {{10, 10}, {3, 3}}};
static uint32_t remaining_ticks = 901;
uint32_t swtimer_remaining(soft_timer_id_t id) { assert(id == TMR_PHASE); return remaining_ticks; }
static uint32_t now_ms;
static unsigned calls, trigger_count, probe_count;
static uint32_t mock_error = HAL_I2C_ERROR_AF;
static HAL_StatusTypeDef sensor_result = HAL_OK;
static HAL_StatusTypeDef probe_result = HAL_OK;
static uint8_t frame[7] = {0x18, 0x80, 0, 6, 0, 0, 0x23};
static char screen[128];
static unsigned position, init_nibbles, half_byte, high_bits;
static int lcd_fail_at = -1;
static unsigned lcd_calls;
static app_mode_t current_mode = MODE_TUNING;
static traffic_config_t active = {{10, 10}, {3, 3}};
static traffic_config_t staged;
static tuning_field_t last_field;
static button_event_t queue[8];
static unsigned queued;
static bool allow_save = true;

uint32_t HAL_GetTick(void) { return now_ms; }
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus) { assert(bus == &hi2c1); return mock_error; }
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *bus, uint16_t address, uint32_t trials, uint32_t timeout)
{
    assert(bus == &hi2c1 && trials == 1 && timeout == 2);
    assert(address >= (0x08 << 1) && address <= (0x77 << 1));
    probe_count++;
    if (probe_result != HAL_OK) return probe_result;
    if (address == (TV3_LCD_ADDRESS << 1) || address == (0x38 << 1)) return HAL_OK;
    return HAL_ERROR;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *bus, uint16_t address, uint8_t *out, uint16_t length, uint32_t timeout)
{
    assert(bus == &hi2c1 && address == (0x38 << 1) && timeout == 2);
    assert(length == 1 || length == 7);
    calls++;
    if (sensor_result != HAL_OK) return sensor_result;
    memcpy(out, frame, length);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *bus, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout)
{
    assert(bus == &hi2c1 && timeout == 2 && length == 3);
    calls++;
    if (address == (0x38 << 1)) {
        assert(data[0] == 0xAC && data[1] == 0x33 && data[2] == 0);
        trigger_count++;
        return sensor_result;
    }
    assert(address == (TV3_LCD_ADDRESS << 1));
    lcd_calls++;
    if (lcd_fail_at >= 0 && (int)lcd_calls >= lcd_fail_at) return HAL_ERROR;
    /* Decode actual PCF8574 pin sequences, rather than reading internal LCD buffers. */
    assert(data[0] == data[2] && data[1] == (data[0] | 4));
    assert((data[0] & 0x0E) == 0x08);
    if (init_nibbles < 4) {
        const uint8_t expected[] = {0x30, 0x30, 0x30, 0x20};
        assert((data[0] & 0xF0) == expected[init_nibbles++]);
        return HAL_OK;
    }
    if (!half_byte) {
        high_bits = data[0] & 0xF0;
        half_byte = 1;
    } else {
        unsigned value = high_bits | (data[0] >> 4);
        half_byte = 0;
        if (data[0] & 1) screen[position++ & 127] = (char)value;
        else if (value & 0x80) position = value & 127;
        else if (value == 1) memset(screen, ' ', sizeof screen);
    }
    return HAL_OK;
}

/* TV1 contract stub with the six Lab-3 coupling rules, NOT production config.c. */
const traffic_config_t *config_get_staged(void) { return &staged; }
uint16_t config_get_red_s(const traffic_config_t *cfg, road_t road)
{
    assert(cfg && (road == ROAD_1 || road == ROAD_2));
    return cfg->green_s[1 - road] + cfg->amber_s[1 - road] + 2;
}
void config_begin_edit(void) { staged = active; }
bool config_is_dirty(void) { return memcmp(&staged, &active, sizeof staged) != 0; }
bool config_adjust_staged(tuning_field_t field, int8_t delta)
{
    traffic_config_t trial = staged;
    int road = field >= FIELD_R2;
    int kind = field % 3;
    int green, amber;
    last_field = field;
    if (kind == 0) road = 1 - road;
    green = trial.green_s[road];
    amber = trial.amber_s[road];
    if (kind == 2) { amber += delta; green -= delta; }
    else green += delta;
    if (green < 5 || green > 60 || amber < 2 || amber > 5) return false;
    trial.green_s[road] = (uint16_t)green;
    trial.amber_s[road] = (uint8_t)amber;
    staged = trial;
    return true;
}
bool config_save_edit(void) { if (!allow_save) return false; active = staged; return true; }
void config_cancel_edit(void) { staged = active; }
app_mode_t mode_manager_get_mode(void) { return current_mode; }
bool mode_manager_take_action(button_event_t *out)
{
    if (!queued) return false;
    *out = queue[0];
    queued--;
    memmove(queue, queue + 1, queued * sizeof queue[0]);
    return true;
}
static void action(button_id_t id, button_kind_t kind)
{
    assert(queued < 8);
    queue[queued++] = (button_event_t){id, kind};
    fsm_tuning_run();
}
static void defaults(void)
{
    active = (traffic_config_t){{10, 10}, {3, 3}};
    config_begin_edit();
    queued = 0;
    current_mode = MODE_TUNING;
    allow_save = true;
}
static void paint(unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        unsigned before = calls;
        now_ms += 200;
        lcd_service();
        assert(calls - before <= 16); /* Four cells, each at most four nibble transactions. */
    }
}
static void reset_lcd(void)
{
    init_nibbles = half_byte = high_bits = position = 0;
    lcd_calls = 0;
    lcd_fail_at = -1;
    memset(screen, '?', sizeof screen);
    lcd_init();
}
static void expect_row(unsigned row, const char *text)
{
    char expected[16];
    size_t length = strlen(text);
    assert(length <= 16);
    memset(expected, ' ', sizeof expected);
    memcpy(expected, text, length);
    if (memcmp(screen + (row ? 0x40 : 0), expected, sizeof expected) != 0) fprintf(stderr, "row %u expected [%.16s], got [%.16s]\n", row, expected, screen + (row ? 0x40 : 0));
    assert(memcmp(screen + (row ? 0x40 : 0), expected, sizeof expected) == 0);
}
static void measure(void)
{
    dht20_init();
    now_ms += 100;
    dht20_service();
    dht20_service();
    now_ms += 80;
    dht20_service();
}
static void test_sensor(void)
{
    dht20_data_t out;
    dht20_init();
    assert(!dht20_get_data(&out) && out.status == SENSOR_UNKNOWN);
    assert(!dht20_get_data(NULL));
    unsigned before = calls;
    now_ms += 99; dht20_service(); assert(calls == before);
    now_ms++; dht20_service(); assert(calls == before + 1);
    dht20_service(); assert(trigger_count == 1);
    before = calls;
    now_ms += 79; dht20_service(); assert(calls == before);
    now_ms++; dht20_service();
    assert(dht20_get_data(&out) && out.temperature_c == 25.0f && out.humidity_pct == 50.0f);
    before = calls;
    now_ms += 1999; dht20_service(); assert(calls == before);
    now_ms++; dht20_service(); assert(trigger_count == 2);
    frame[6] ^= 1; now_ms += 80; dht20_service();
    assert(!dht20_get_data(&out) && out.status == SENSOR_ERROR);
    assert(out.temperature_c == 25.0f); /* Old sample is retained but invalid. */
    frame[6] ^= 1;
    before = calls;
    now_ms += 1999; dht20_service(); assert(calls == before);
    now_ms++; dht20_service(); assert(calls == before); /* ERROR_WAIT -> INIT */
    now_ms += 100; dht20_service(); dht20_service(); now_ms += 80; dht20_service();
    assert(dht20_get_data(&out));
    for (int err = HAL_ERROR; err <= HAL_TIMEOUT; err++) {
        sensor_result = (HAL_StatusTypeDef)err; measure();
        assert(!dht20_get_data(&out) && out.status == SENSOR_ERROR);
    }
    sensor_result = HAL_OK;
    frame[0] = 0; measure(); assert(!dht20_get_data(&out)); frame[0] = 0x18;
    dht20_init(); now_ms += 100; dht20_service(); dht20_service();
    frame[0] = 0x98; now_ms += 80; dht20_service();
    assert(!dht20_get_data(&out) && out.status == SENSOR_UNKNOWN);
    now_ms += 120; dht20_service(); assert(!dht20_get_data(&out) && out.status == SENSOR_ERROR);
    frame[0] = 0x18;
    /* Corrupt status after a successful trigger, not only at startup. */
    dht20_init(); now_ms += 100; dht20_service(); dht20_service();
    frame[0] = 0; now_ms += 80; dht20_service(); assert(!dht20_get_data(&out)); frame[0] = 0x18;
    now_ms = 0xFFFFFFC0U; measure(); assert(dht20_get_data(&out));
    puts("TV3-02 PASS: DHT20 waits, known frame, CRC, HAL errors, status, BUSY, recovery, uint32 wrap");
}
static void test_scan(void)
{
    i2c_scanner_init(&hi2c1);
    unsigned before = calls;
    now_ms += 200; lcd_service(); dht20_service(); assert(calls == before);
    for (unsigned i = 0; i < 112; i++) i2c_scanner_service();
    assert(probe_count == 112 && g_i2c_scan.complete && g_i2c_scan.device_count == 2);
    assert(g_i2c_scan.detected[TV3_LCD_ADDRESS] && g_i2c_scan.detected[0x38] && !g_i2c_scan.detected[0x26]);
    assert(g_i2c_scan.bus_error_count == 0);
    i2c_scanner_service(); assert(probe_count == 112);
    /* Actual STM32F1 IsDeviceReady NACK behavior: HAL_ERROR + NONE. */
    i2c_scanner_init(&hi2c1);
    mock_error = HAL_I2C_ERROR_NONE;
    for (unsigned i = 0; i < 112; i++) i2c_scanner_service();
    assert(g_i2c_scan.complete && g_i2c_scan.device_count == 2);
    assert(g_i2c_scan.bus_error_count == 0);
    for (int err = HAL_ERROR; err <= HAL_TIMEOUT; err++) {
        i2c_scanner_init(&hi2c1); probe_result = (HAL_StatusTypeDef)err;
        mock_error = HAL_I2C_ERROR_AF | HAL_I2C_ERROR_BERR;
        i2c_scanner_service(); assert(g_i2c_scan.bus_error_count == 1);
    }
    probe_result = HAL_OK; mock_error = HAL_I2C_ERROR_AF;
    i2c_scanner_init(NULL); before = probe_count; i2c_scanner_service(); assert(probe_count == before);
    puts("TV3-03/05 PASS: scanner ACK/NACK/error/NULL/completion and no LCD/DHT bus access during scan");
}
static void test_lcd(void)
{
    measure(); defaults(); reset_lcd();
    unsigned before = calls;
    now_ms += 49; lcd_service(); assert(calls == before);
    paint(20);
    expect_row(0, "AUTO SAFE R/R"); expect_row(1, "T:25.0C H:50%");
    const char *expected[] = {
        "1 R:14s 2 G:10s", "1 R:11s 2 A:10s", "1 R:10s 2 R:24s",
        "1 G:10s 2 R:14s", "1 A:10s 2 R:11s", "1 R:24s 2 R:10s"
    };
    for (int p = PHASE_P0; p <= PHASE_P5; p++) {
        lcd_set_view(MODE_AUTO, (traffic_phase_t)p, FIELD_R1, false);
        paint(8); expect_row(0, expected[p]);
    }
    remaining_ticks = 100;
    lcd_set_view(MODE_AUTO, PHASE_P0, FIELD_R1, false); paint(8);
    expect_row(0, "1 R:05s 2 G:01s");
    remaining_ticks = 901;
    for (int mode = MODE_AUTO; mode <= MODE_MANUAL; mode++) {
        for (int phase = PHASE_P0; phase <= PHASE_SAFE; phase++) {
            lcd_set_view((app_mode_t)mode, (traffic_phase_t)phase, FIELD_R1, false); paint(8);
            if (mode == MODE_MANUAL && phase == PHASE_P3) expect_row(0, "MAN 1:G 2:R");
        }
    }
    lcd_set_view((app_mode_t)-1, PHASE_P0, FIELD_R1, false); paint(2); expect_row(0, "MAN 1:R 2:R");
    for (int f = FIELD_R1; f <= FIELD_A2; f++) {
        lcd_set_view(MODE_TUNING, PHASE_SAFE, (tuning_field_t)f, true); paint(8);
        if (f == FIELD_R1) expect_row(0, "T R1:015 G2:010*");
        if (f == FIELD_G1) expect_row(0, "TUNE G1=010s*");
        if (f == FIELD_R2) expect_row(0, "T R2:015 G1:010*");
        expect_row(1, "T:25.0C H:50%");
    }
    char local[] = "SAVED"; lcd_set_notice(local); local[0] = 'X';
    paint(3); expect_row(1, "SAVED"); paint(6); expect_row(1, "T:25.0C H:50%");
    lcd_set_notice("123456789012345678901234"); paint(4); expect_row(1, "1234567890123456");
    lcd_set_notice(NULL); paint(8); expect_row(1, "T:25.0C H:50%");
    sensor_result = HAL_ERROR; measure(); paint(8); expect_row(1, "T:--.-C H:--%"); sensor_result = HAL_OK;
    /* Fail the low nibble of an initialization command; driver must restart init. */
    reset_lcd(); lcd_fail_at = 6; paint(5);
    init_nibbles = half_byte = 0; lcd_fail_at = -1;
    paint(20); expect_row(0, "AUTO SAFE R/R");
    /* Fail an update between address/data, then verify complete redraw after recovery. */
    lcd_set_view(MODE_MANUAL, PHASE_P3, FIELD_R1, false);
    lcd_fail_at = (int)lcd_calls + 3; paint(1);
    init_nibbles = half_byte = 0; lcd_fail_at = -1; paint(20); expect_row(0, "MAN 1:G 2:R");
    puts("TV3-01/05 PASS: actual pin/nibble decode, modes/phases/fields, T/H, notice copy/truncate/expiry, I2C failure/re-init, <=4 cells per call");
}
static void test_tuning(void)
{
    defaults(); fsm_tuning_reset();
    for (int f = FIELD_R1; f <= FIELD_A2; f++) {
        traffic_config_t old = staged;
        action(BTN_3, BTN_EVT_DOWN); assert(last_field == (tuning_field_t)f);
        assert(config_is_dirty() && active.green_s[0] == 10 && active.green_s[1] == 10);
        action(BTN_4, BTN_EVT_DOWN); assert(memcmp(&old, &staged, sizeof old) == 0);
        action(BTN_2, BTN_EVT_SHORT);
    }
    action(BTN_3, BTN_EVT_DOWN); assert(last_field == FIELD_R1 && staged.green_s[1] == 11);
    traffic_config_t old = staged;
    action(BTN_3, BTN_EVT_SHORT); action(BTN_3, BTN_EVT_LONG); action(BTN_4, BTN_EVT_LONG);
    assert(memcmp(&old, &staged, sizeof old) == 0); /* No repeat and no BTN4 LONG cancel. */
    allow_save = false; action(BTN_2, BTN_EVT_LONG); assert(active.green_s[1] == 10);
    allow_save = true; action(BTN_2, BTN_EVT_LONG); assert(active.green_s[1] == 11 && !config_is_dirty());
    action(BTN_3, BTN_EVT_DOWN); config_cancel_edit(); assert(!config_is_dirty()); /* TV1 mode-exit contract. */
    defaults(); fsm_tuning_reset();
    for (int i = 0; i < 5; i++) action(BTN_2, BTN_EVT_SHORT); /* A2 */
    action(BTN_3, BTN_EVT_DOWN); assert(staged.amber_s[1] == 4 && staged.green_s[1] == 9 && config_get_red_s(&staged, ROAD_1) == 15);
    action(BTN_3, BTN_EVT_DOWN); old = staged; action(BTN_3, BTN_EVT_DOWN); assert(memcmp(&old, &staged, sizeof old) == 0);
    for (int i = 0; i < 10; i++) action(BTN_4, BTN_EVT_DOWN);
    assert(staged.amber_s[1] == 2);
    defaults(); fsm_tuning_reset(); action(BTN_2, BTN_EVT_SHORT); /* G1 */
    for (int i = 0; i < 100; i++) action(BTN_3, BTN_EVT_DOWN);
    assert(staged.green_s[0] == 60);
    for (int i = 0; i < 100; i++) action(BTN_4, BTN_EVT_DOWN);
    assert(staged.green_s[0] == 5);
    defaults(); fsm_tuning_reset();
    queue[0] = (button_event_t){BTN_3, BTN_EVT_DOWN}; queue[1] = queue[0]; queued = 2;
    fsm_tuning_run(); assert(queued == 1 && staged.green_s[1] == 11);
    current_mode = MODE_AUTO; fsm_tuning_run(); assert(queued == 1);
    queued = 0; current_mode = MODE_TUNING;
    fsm_tuning_reset();
    int before_repeat = staged.green_s[1];
    action(BTN_3, BTN_EVT_REPEAT); assert(staged.green_s[1] == before_repeat + 1);
    action(BTN_4, BTN_EVT_REPEAT); assert(staged.green_s[1] == before_repeat);
    puts("TV3-04 PASS: six fields, DOWN once, ignored SHORT/LONG, save/reject/cancel contract, config coupling, bounds, one event/call, mode isolation");
}
int main(void)
{
    test_sensor(); test_scan(); test_lcd(); test_tuning();
    puts("ALL TV3 HOST TESTS PASSED (hardware and production TV1 integration not verified)");
    return 0;
}
