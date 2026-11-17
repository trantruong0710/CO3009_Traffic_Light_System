#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "button.h"
#include "config.h"
#include "fsm_automatic.h"
#include "fsm_manual.h"
#include "fsm_tuning.h"
#include "global.h"
#include "mode_manager.h"
#include "software_timer.h"
#include "traffic_light.h"

#define MOCK_QUEUE_CAPACITY 8U

static button_event_t input[MOCK_QUEUE_CAPACITY];
static unsigned input_count;
static unsigned input_read;
static unsigned held_block_calls;
static unsigned resets[3];
static unsigned all_red_calls;

static void push(button_id_t id, button_kind_t kind)
{
    assert(input_count < MOCK_QUEUE_CAPACITY);
    input[input_count++] = (button_event_t){id, kind};
}

bool button_pop_event(button_event_t *out_event)
{
    if (input_read == input_count) {
        button_clear_events();
        return false;
    }

    *out_event = input[input_read++];
    return true;
}

void button_clear_events(void)
{
    input_count = 0U;
    input_read = 0U;
}

void button_block_held_until_release(void)
{
    ++held_block_calls;
}

void swtimer_stop(soft_timer_id_t id)
{
    assert((unsigned)id < (unsigned)TMR_COUNT);
}

void traffic_force_all_red(void)
{
    ++all_red_calls;
}

void fsm_automatic_reset(void)
{
    assert(mode_manager_get_mode() == MODE_AUTO);
    ++resets[MODE_AUTO];
}

void fsm_manual_reset(void)
{
    assert(mode_manager_get_mode() == MODE_MANUAL);
    ++resets[MODE_MANUAL];
}

void fsm_tuning_reset(void)
{
    assert(mode_manager_get_mode() == MODE_TUNING);
    assert(!config_is_dirty());
    ++resets[MODE_TUNING];
}

static void setup(void)
{
    global_init();
    config_init_defaults();
    button_clear_events();
    held_block_calls = 0U;
    all_red_calls = 0U;

    for (unsigned i = 0U; i < 3U; ++i) {
        resets[i] = 0U;
    }

    mode_manager_init();
}

static void press_mode(void)
{
    push(BTN_1, BTN_EVT_SHORT);
    mode_manager_run();
}

static void test_mode_cycle(void)
{
    setup();

    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(resets[MODE_AUTO] == 1U);

    press_mode();
    assert(mode_manager_get_mode() == MODE_MANUAL);
    assert(resets[MODE_MANUAL] == 1U);

    press_mode();
    assert(mode_manager_get_mode() == MODE_TUNING);
    assert(resets[MODE_TUNING] == 1U);
    assert(g_app_state.phase == PHASE_SAFE);
    assert(all_red_calls == 1U);

    press_mode();
    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(resets[MODE_AUTO] == 2U);
    assert(held_block_calls == 4U); /* Startup plus three mode changes. */
}

static void test_mode_discards_old_actions(void)
{
    setup();
    press_mode();

    push(BTN_2, BTN_EVT_SHORT);
    mode_manager_run();

    push(BTN_3, BTN_EVT_DOWN);
    push(BTN_1, BTN_EVT_SHORT);
    push(BTN_4, BTN_EVT_DOWN);
    mode_manager_run();

    button_event_t event;
    assert(mode_manager_get_mode() == MODE_TUNING);
    assert(!mode_manager_take_action(&event));

    mode_manager_run();
    assert(!mode_manager_take_action(&event));
    assert(held_block_calls == 3U);
}

static void test_manual_routing(void)
{
    setup();
    press_mode();

    push(BTN_1, BTN_EVT_DOWN);
    push(BTN_1, BTN_EVT_LONG);
    push(BTN_2, BTN_EVT_DOWN);
    push(BTN_2, BTN_EVT_SHORT);
    push(BTN_3, BTN_EVT_DOWN);
    push(BTN_4, BTN_EVT_DOWN);
    mode_manager_run();

    button_event_t event;
    assert(mode_manager_get_mode() == MODE_MANUAL);
    assert(!mode_manager_take_action(NULL));

    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_2 && event.kind == BTN_EVT_SHORT);

    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_3 && event.kind == BTN_EVT_DOWN);

    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_4 && event.kind == BTN_EVT_DOWN);

    assert(!mode_manager_take_action(&event));
}

static void test_tuning_keeps_saved_values(void)
{
    setup();
    press_mode();
    press_mode();

    assert(config_adjust_staged(FIELD_G1, 2));
    assert(config_save_edit());
    assert(config_adjust_staged(FIELD_G1, 1));
    assert(config_is_dirty());

    press_mode();

    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(config_get_active()->green_s[ROAD_1] == 12U);
    assert(config_get_staged()->green_s[ROAD_1] == 12U);
    assert(!config_is_dirty());
}

int main(void)
{
    test_mode_cycle();
    test_mode_discards_old_actions();
    test_manual_routing();
    test_tuning_keeps_saved_values();

    puts("PASS: 4 mode manager tests");
    return 0;
}
