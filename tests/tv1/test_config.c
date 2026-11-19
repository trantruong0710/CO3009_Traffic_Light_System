
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

static void adjust_many(tuning_field_t field, int8_t delta, unsigned times)
{
    for (unsigned i = 0U; i < times; ++i) {
        assert(config_adjust_staged(field, delta));
    }
}

static void assert_config_equal(const traffic_config_t *a,
                                const traffic_config_t *b)
{
    for (unsigned i = 0U; i < 2U; ++i) {
        assert(a->green_s[i] == b->green_s[i]);
        assert(a->amber_s[i] == b->amber_s[i]);
    }
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

    adjust_many(FIELD_G1, 1, 4U);

    assert(config_is_dirty());
    assert(config_get_staged()->green_s[ROAD_1] == 14U);
    assert(config_get_active()->green_s[ROAD_1] == 10U);

    assert(config_save_edit());
    assert(config_get_active()->green_s[ROAD_1] == 14U);
    assert(!config_is_dirty());

    assert(config_adjust_staged(FIELD_A1, 1));
    assert(config_get_staged()->amber_s[ROAD_1] == 4U);
    assert(config_get_staged()->green_s[ROAD_1] == 13U);
    assert(config_get_red_s(config_get_staged(), ROAD_2) == 19U);
    assert(config_is_dirty());

    config_cancel_edit();

    assert(config_get_staged()->amber_s[ROAD_1] == 3U);
    assert(config_get_staged()->green_s[ROAD_1] == 14U);
    assert(!config_is_dirty());
}

static void test_red_adjusts_opposite_green(void)
{
    setup();

    adjust_many(FIELD_R1, 1, 2U);
    assert(config_adjust_staged(FIELD_R2, -1));

    const traffic_config_t *cfg = config_get_staged();

    assert(cfg->green_s[ROAD_2] == 12U);
    assert(cfg->green_s[ROAD_1] == 9U);
    assert(cfg->amber_s[ROAD_1] == 3U);
    assert(cfg->amber_s[ROAD_2] == 3U);
    assert(config_get_red_s(cfg, ROAD_1) == 17U);
    assert(config_get_red_s(cfg, ROAD_2) == 14U);
}

static void test_green_adjusts_opposite_red(void)
{
    setup();

    assert(config_adjust_staged(FIELD_G1, 1));
    assert(config_get_red_s(config_get_staged(), ROAD_2) == 16U);

    assert(config_adjust_staged(FIELD_G2, -1));

    const traffic_config_t *cfg = config_get_staged();

    assert(cfg->green_s[ROAD_1] == 11U);
    assert(cfg->green_s[ROAD_2] == 9U);
    assert(config_get_red_s(cfg, ROAD_1) == 14U);
    assert(config_get_red_s(cfg, ROAD_2) == 16U);
}

static void test_amber_coupling_both_roads(void)
{
    setup();

    assert(config_adjust_staged(FIELD_A2, 1));
    assert(config_get_staged()->amber_s[ROAD_2] == 4U);
    assert(config_get_staged()->green_s[ROAD_2] == 9U);
    assert(config_get_red_s(config_get_staged(), ROAD_1) == 15U);
    assert(config_get_active()->amber_s[ROAD_2] == 3U);
    assert(config_get_active()->green_s[ROAD_2] == 10U);

    setup();

    assert(config_adjust_staged(FIELD_A1, 1));
    assert(config_get_staged()->amber_s[ROAD_1] == 4U);
    assert(config_get_staged()->green_s[ROAD_1] == 9U);
    assert(config_get_red_s(config_get_staged(), ROAD_2) == 15U);

    setup();

    assert(config_adjust_staged(FIELD_A2, -1));
    assert(config_get_staged()->amber_s[ROAD_2] == 2U);
    assert(config_get_staged()->green_s[ROAD_2] == 11U);
    assert(config_get_red_s(config_get_staged(), ROAD_1) == 15U);

    setup();

    assert(config_adjust_staged(FIELD_A1, -1));
    assert(config_get_staged()->amber_s[ROAD_1] == 2U);
    assert(config_get_staged()->green_s[ROAD_1] == 11U);
    assert(config_get_red_s(config_get_staged(), ROAD_2) == 15U);
}

static void test_green_limits(void)
{
    setup();

    adjust_many(FIELD_G1, -1, 5U);

    traffic_config_t before = *config_get_staged();

    assert(before.green_s[ROAD_1] == MIN_GREEN_S);
    assert(!config_adjust_staged(FIELD_G1, -1));
    assert_config_equal(&before, config_get_staged());

    setup();

    adjust_many(FIELD_G2, 1, 50U);

    before = *config_get_staged();

    assert(before.green_s[ROAD_2] == MAX_GREEN_S);
    assert(!config_adjust_staged(FIELD_G2, 1));
    assert_config_equal(&before, config_get_staged());
}

static void test_amber_limits(void)
{
    setup();

    assert(config_adjust_staged(FIELD_A1, -1));

    traffic_config_t before = *config_get_staged();

    assert(before.amber_s[ROAD_1] == MIN_AMBER_S);
    assert(!config_adjust_staged(FIELD_A1, -1));
    assert_config_equal(&before, config_get_staged());

    setup();

    adjust_many(FIELD_A2, 1, 2U);

    before = *config_get_staged();

    assert(before.amber_s[ROAD_2] == MAX_AMBER_S);
    assert(before.green_s[ROAD_2] == 8U);
    assert(!config_adjust_staged(FIELD_A2, 1));
    assert_config_equal(&before, config_get_staged());
}

static void test_coupling_rejects_out_of_range_green(void)
{
    setup();

    adjust_many(FIELD_G2, -1, 5U);

    traffic_config_t before = *config_get_staged();

    assert(!config_adjust_staged(FIELD_A2, 1));
    assert_config_equal(&before, config_get_staged());

    setup();

    adjust_many(FIELD_G1, 1, 50U);

    before = *config_get_staged();

    assert(!config_adjust_staged(FIELD_A1, -1));
    assert_config_equal(&before, config_get_staged());
}

static void test_red_limits(void)
{
    setup();

    adjust_many(FIELD_G2, 1, 50U);

    traffic_config_t before = *config_get_staged();

    assert(!config_adjust_staged(FIELD_R1, 1));
    assert_config_equal(&before, config_get_staged());

    setup();

    adjust_many(FIELD_G1, -1, 5U);

    before = *config_get_staged();

    assert(!config_adjust_staged(FIELD_R2, -1));
    assert_config_equal(&before, config_get_staged());
}

static void test_invalid_inputs_and_atomicity(void)
{
    setup();

    traffic_config_t before = *config_get_staged();

    assert(!config_adjust_staged(FIELD_G1, 0));
    assert(!config_adjust_staged(FIELD_G1, 2));
    assert(!config_adjust_staged(FIELD_R1, -2));
    assert(!config_adjust_staged((tuning_field_t)99, 1));

    assert_config_equal(&before, config_get_staged());
    assert(!config_is_dirty());
    assert(!config_validate(NULL));
    assert(config_get_red_s(NULL, ROAD_1) == 0U);
    assert(config_get_red_s(config_get_staged(), (road_t)99) == 0U);

    traffic_config_t invalid = before;
    invalid.green_s[ROAD_1] = 0U;
    assert(!config_validate(&invalid));

    invalid = before;
    invalid.amber_s[ROAD_2] = 0U;
    assert(!config_validate(&invalid));

    g_app_state.staged_config.amber_s[ROAD_1] = 0U;
    before = *config_get_staged();

    assert(!config_adjust_staged(FIELD_G2, 1));
    assert_config_equal(&before, config_get_staged());
}

static void test_save_rejects_invalid_staged(void)
{
    setup();

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
    test_green_adjusts_opposite_red();
    test_amber_coupling_both_roads();
    test_green_limits();
    test_amber_limits();
    test_coupling_rejects_out_of_range_green();
    test_red_limits();
    test_invalid_inputs_and_atomicity();
    test_save_rejects_invalid_staged();

    puts("PASS: 11 config tests");
    return 0;
}
