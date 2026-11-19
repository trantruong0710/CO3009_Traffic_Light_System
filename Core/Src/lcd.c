#include "lcd.h"
#include "config.h"
#include "dht20.h"
#include "i2c_scanner.h"
#include <stdio.h>
#include <string.h>
extern I2C_HandleTypeDef hi2c1;
#ifndef TV3_LCD_ADDRESS
#define TV3_LCD_ADDRESS 0x27
#endif

#define LCD_COLUMNS            16
#define LCD_ROWS               2
#define LCD_FRAME_SIZE         (LCD_COLUMNS * LCD_ROWS)

#define LCD_I2C_TIMEOUT_MS     2
#define LCD_INIT_POWERON_MS    50
#define LCD_IO_INTERVAL_MS     5
#define LCD_NOTICE_DURATION_MS 1000
#define LCD_MAX_CHARS_PER_CALL  4

#define LCD_BACKLIGHT_BIT      0x08
#define LCD_ENABLE_BIT         0x04
#define LCD_RS_BIT             0x01
#define LCD_SET_DDRAM_CMD      0x80
#define LCD_ROW2_DDRAM         0x40

static app_mode_t mode;
static traffic_phase_t phase;
static tuning_field_t field;
static bool dirty;
static char notice[LCD_COLUMNS + 1];
static uint32_t notice_at, last_io;
static unsigned init_step, cursor;
static char shown[LCD_FRAME_SIZE];

static bool nibble(uint8_t n, bool rs)
{
    uint8_t v = (n & 0xF0) | LCD_BACKLIGHT_BIT;
    if (rs) {
        v = v | LCD_RS_BIT;
    }
    uint8_t bytes[3];
    bytes[0] = v;
    bytes[1] = v | LCD_ENABLE_BIT;
    bytes[2] = v;
    return HAL_I2C_Master_Transmit(&hi2c1, TV3_LCD_ADDRESS << 1, bytes, 3, LCD_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool send_byte(uint8_t v, bool rs)
{
    if (!nibble(v, rs)) return false;
    return nibble((uint8_t)(v << 4), rs);
}

static void make_line(char *out)
{
    char row1[32];
    char row2[32];
    const char *names[] = {"R1", "G1", "A1", "R2", "G2", "A2"};
    const char *lights[] = {"R/G", "R/A", "R/R", "G/R", "A/R", "R/R", "R/R"};
    const char *mode_name;
    dht20_data_t sensor;
    size_t length;

    if (mode == MODE_TUNING) {
        const traffic_config_t *cfg = config_get_staged();
        int road = 0;
        int kind = (int)field % 3;
        int value;
        const char *mark = "";
        if (field >= FIELD_R2) road = 1;
        if (dirty) mark = "*";
        if (kind == 0) {
            value = config_get_red_s(cfg, (road_t)road);
            snprintf(row1, sizeof row1, "T %s:%03d G%d:%03d%s",
                     names[field], value, 2 - road, cfg->green_s[1 - road], mark);
        } else {
            if (kind == 1) value = cfg->green_s[road];
            else value = cfg->amber_s[road];
            snprintf(row1, sizeof row1, "TUNE %s=%03ds%s", names[field], value, mark);
        }
    } else {
        if (mode == MODE_AUTO) mode_name = "AUTO";
        else mode_name = "MAN";
        if (phase == PHASE_SAFE) {
            snprintf(row1, sizeof row1, "%s SAFE R/R", mode_name);
        } else {
            snprintf(row1, sizeof row1, "%s P%d %s", mode_name, (int)phase, lights[phase]);
        }
    }

    if (dht20_get_data(&sensor)) {
        int tenths;
        int humidity = (int)(sensor.humidity_pct + 0.5f);
        const char *sign = "";
        if (sensor.temperature_c >= 0) tenths = (int)(sensor.temperature_c * 10 + 0.5f);
        else tenths = (int)(sensor.temperature_c * 10 - 0.5f);
        if (tenths < 0) {
            sign = "-";
            tenths = -tenths;
        }
        if (humidity < 0) humidity = 0;
        if (humidity > 100) humidity = 100;
        snprintf(row2, sizeof row2, "T:%s%d.%dC H:%d%%", sign, tenths / 10, tenths % 10, humidity);
    } else {
        snprintf(row2, sizeof row2, "T:--.-C H:--%%");
    }
    if (notice[0] != '\0' && (uint32_t)(HAL_GetTick() - notice_at) < LCD_NOTICE_DURATION_MS) {
        snprintf(row2, sizeof row2, "%s", notice);
    }
    memset(out, ' ', LCD_FRAME_SIZE);
    length = strlen(row1);
    if (length > LCD_COLUMNS) length = LCD_COLUMNS;
    memcpy(out, row1, length);
    length = strlen(row2);
    if (length > LCD_COLUMNS) length = LCD_COLUMNS;
    memcpy(out + LCD_COLUMNS, row2, length);
}

void lcd_init(void)
{
    mode = MODE_AUTO;
    phase = PHASE_SAFE;
    field = FIELD_R1;
    dirty = false;
    notice[0] = '\0';
    init_step = 0;
    cursor = 0;
    last_io = HAL_GetTick();
    memset(shown, 0, sizeof shown);
}

void lcd_set_view(app_mode_t m, traffic_phase_t p, tuning_field_t f, bool d)
{
    if ((unsigned)m > MODE_TUNING || (unsigned)p > PHASE_SAFE || (unsigned)f > FIELD_A2) return;
    mode = m;
    phase = p;
    field = f;
    dirty = d;
}

void lcd_set_notice(const char *msg)
{
    if (msg == NULL) {
        notice[0] = '\0';
    } else {
        snprintf(notice, sizeof notice, "%s", msg);
    }
    notice_at = HAL_GetTick();
    cursor = LCD_COLUMNS; /* Render notices on row 2 before the next row-1 update. */
}

void lcd_service(void)
{
    static const uint8_t setup[] = {0x30, 0x30, 0x30, 0x20, 0x28, 0x08, 0x01, 0x06, 0x0C};
    uint32_t now = HAL_GetTick();
    char desired[LCD_FRAME_SIZE];
    unsigned start_cursor = cursor;
    int sent = 0;
    if (g_i2c_scan.next_address != 0 && !g_i2c_scan.complete) return;
    uint32_t wait_ms = LCD_IO_INTERVAL_MS;
    if (init_step == 0) {
        wait_ms = LCD_INIT_POWERON_MS;
    }
    if ((uint32_t)(now - last_io) < wait_ms) return;
    last_io = now;
    if (init_step < sizeof setup) {
        bool ok;
        if (init_step < 4) {
            ok = nibble(setup[init_step], false);
        } else {
            ok = send_byte(setup[init_step], false);
        }
        if (ok) {
            init_step++;
        } else {
            init_step = 0;
            memset(shown, 0, sizeof shown);
        }
        return;
    }
    make_line(desired);
    for (int n = 0; n < LCD_FRAME_SIZE && sent < LCD_MAX_CHARS_PER_CALL; n++) {
        int i = (start_cursor + n) % LCD_FRAME_SIZE;
        if (shown[i] == desired[i]) continue;
        uint8_t address;
        if (i < LCD_COLUMNS) {
            address = LCD_SET_DDRAM_CMD | i;
        } else {
            address = LCD_SET_DDRAM_CMD | (LCD_ROW2_DDRAM + i - LCD_COLUMNS);
        }
        if (!send_byte(address, false) || !send_byte((uint8_t)desired[i], true)) {
            init_step = 0;
            memset(shown, 0, sizeof shown);
            return;
        }
        shown[i] = desired[i];
        cursor = (i + 1) % LCD_FRAME_SIZE;
        sent++;
    }
}
