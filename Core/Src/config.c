#include "config.h"
#include "global.h"

#include <stddef.h>

static bool green_valid(int32_t seconds)
{
    return seconds >= (int32_t)MIN_GREEN_S &&
           seconds <= (int32_t)MAX_GREEN_S;
}

static bool amber_valid(int32_t seconds)
{
    return seconds >= (int32_t)MIN_AMBER_S &&
           seconds <= (int32_t)MAX_AMBER_S;
}

void config_init_defaults(void)
{
    const traffic_config_t defaults = {
        .green_s = {10U, 10U},
        .amber_s = {3U, 3U}
    };

    g_app_state.active_config = defaults;
    g_app_state.staged_config = defaults;
}

const traffic_config_t *config_get_active(void)
{
    return &g_app_state.active_config;
}

const traffic_config_t *config_get_staged(void)
{
    return &g_app_state.staged_config;
}

uint16_t config_get_red_s(const traffic_config_t *cfg, road_t road)
{
    if (cfg == NULL || (road != ROAD_1 && road != ROAD_2)) {
        return 0U;
    }

    road_t opposite = (road == ROAD_1) ? ROAD_2 : ROAD_1;

    return (uint16_t)(cfg->green_s[opposite] +
                      cfg->amber_s[opposite] + 2U * CLEARANCE_S);
}

bool config_validate(const traffic_config_t *cfg)
{
    if (cfg == NULL) {
        return false;
    }

    for (unsigned i = 0U; i < 2U; ++i) {
        if (!green_valid(cfg->green_s[i]) ||
            !amber_valid(cfg->amber_s[i])) {
            return false;
        }
    }

    return true;
}

void config_begin_edit(void)
{
    g_app_state.staged_config = g_app_state.active_config;
}

bool config_adjust_staged(tuning_field_t field, int8_t delta_seconds)
{
    traffic_config_t *cfg = &g_app_state.staged_config;
    road_t road;
    bool adjust_amber = false;

    switch (field) {
    case FIELD_G1:
    case FIELD_R2:
        road = ROAD_1;
        break;

    case FIELD_G2:
    case FIELD_R1:
        road = ROAD_2;
        break;

    case FIELD_A1:
        road = ROAD_1;
        adjust_amber = true;
        break;

    case FIELD_A2:
        road = ROAD_2;
        adjust_amber = true;
        break;

    default:
        return false;
    }

    if (adjust_amber) {
        int32_t next = (int32_t)cfg->amber_s[road] + delta_seconds;
        if (!amber_valid(next)) {
            return false;
        }

        cfg->amber_s[road] = (uint8_t)next;
    } else {
        int32_t next = (int32_t)cfg->green_s[road] + delta_seconds;
        if (!green_valid(next)) {
            return false;
        }

        cfg->green_s[road] = (uint16_t)next;
    }

    return true;
}

bool config_save_edit(void)
{
    if (!config_validate(&g_app_state.staged_config)) {
        return false;
    }

    g_app_state.active_config = g_app_state.staged_config;
    return true;
}

void config_cancel_edit(void)
{
    config_begin_edit();
}

bool config_is_dirty(void)
{
    const traffic_config_t *active = config_get_active();
    const traffic_config_t *staged = config_get_staged();

    for (unsigned i = 0U; i < 2U; ++i) {
        if (active->green_s[i] != staged->green_s[i] ||
            active->amber_s[i] != staged->amber_s[i]) {
            return true;
        }
    }

    return false;
}