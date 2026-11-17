#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "config.h"
#include "global.h"

static void setup(void)
{
    global_init();
    config_init_defaults();
    config_begin_edit();
}

static void test_defaults(void)
{
    setup();

    const traffic_config_t *cfg = config_get_active();

    assert(config_validate(cfg));
    assert(cfg->green_s[ROAD_1] == 10U);
    assert(cfg->green_s[ROAD_2] == 10U);
    assert(cfg->amber_s[ROAD_1] == 3U);
    assert(cfg->amber_s[ROAD_2] == 3U);
    assert(config_get_red_s(cfg, ROAD_1) == 15U);
    assert(config_get_red_s(cfg, ROAD_2) == 15U);
    assert(!config_is_dirty());
}

static void test_staged_save_and_cancel(void)
{
    setup();

    assert(config_adjust_staged(FIELD_G1, 4));
    assert(config_is_dirty());
    assert(config_get_staged()->green_s[ROAD_1] == 14U);
    assert(config_get_active()->green_s[ROAD_1] == 10U);

    assert(config_save_edit());
    assert(config_get_active()->green_s[ROAD_1] == 14U);
    assert(!config_is_dirty());

    assert(config_adjust_staged(FIELD_A1, 1));
    assert(config_is_dirty());
    config_cancel_edit();

    assert(config_get_staged()->amber_s[ROAD_1] == 3U);
    assert(config_get_staged()->green_s[ROAD_1] == 14U);
    assert(!config_is_dirty());
}

static void test_red_adjusts_opposite_green(void)
{
    setup();

    assert(config_adjust_staged(FIELD_R1, 2));
    assert(config_adjust_staged(FIELD_R2, -1));

    const traffic_config_t *cfg = config_get_staged();

    assert(cfg->green_s[ROAD_2] == 12U);
    assert(cfg->green_s[ROAD_1] == 9U);
    assert(cfg->amber_s[ROAD_1] == 3U);
    assert(cfg->amber_s[ROAD_2] == 3U);
    assert(config_get_red_s(cfg, ROAD_1) == 17U);
    assert(config_get_red_s(cfg, ROAD_2) == 14U);
}

static void test_limits(void)
{
    setup();

    assert(config_adjust_staged(FIELD_G1, -5));
    assert(!config_adjust_staged(FIELD_G1, -1));
    assert(config_get_staged()->green_s[ROAD_1] == MIN_GREEN_S);

    assert(config_adjust_staged(FIELD_G2, 50));
    assert(!config_adjust_staged(FIELD_R1, 1));
    assert(config_get_staged()->green_s[ROAD_2] == MAX_GREEN_S);

    assert(config_adjust_staged(FIELD_A1, -1));
    assert(!config_adjust_staged(FIELD_A1, -1));
    assert(config_get_staged()->amber_s[ROAD_1] == MIN_AMBER_S);

    assert(config_adjust_staged(FIELD_A2, 2));
    assert(!config_adjust_staged(FIELD_A2, 1));
    assert(config_get_staged()->amber_s[ROAD_2] == MAX_AMBER_S);

    assert(!config_adjust_staged((tuning_field_t)99, 1));
    assert(!config_validate(NULL));
    assert(config_validate(config_get_staged()));

    traffic_config_t invalid = *config_get_staged();
    invalid.green_s[ROAD_1] = 0U;
    assert(!config_validate(&invalid));
}

static void test_save_rejects_invalid_staged(void)
{
    setup();

    /* Inject bad data to verify that SAVE validates independently. */
    g_app_state.staged_config.amber_s[ROAD_2] = 0U;

    assert(!config_save_edit());
    assert(config_get_active()->amber_s[ROAD_2] == 3U);

    config_cancel_edit();
    assert(config_validate(config_get_staged()));
    assert(!config_is_dirty());
}

int main(void)
{
    test_defaults();
    test_staged_save_and_cancel();
    test_red_adjusts_opposite_green();
    test_limits();
    test_save_rejects_invalid_staged();

    puts("PASS: 5 config tests");
    return 0;
}