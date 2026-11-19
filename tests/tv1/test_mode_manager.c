#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "button.h"
#include "config.h"
#include "fsm_automatic.h"
#include "fsm_manual.h"
#include "fsm_tuning.h"
#include "global.h"
#include "lcd.h"
#include "mode_manager.h"
#include "software_timer.h"
#include "traffic_light.h"

#define MOCK_QUEUE_CAPACITY 8U
#define FAULT_TRAFFIC_INVALID_PAIR (1UL << 0)
#define FAULT_BUTTON_QUEUE_FULL   (1UL << 1)
#define FAULT_CORE_TASK_MISSING   (1UL << 2)

static button_event_t input[MOCK_QUEUE_CAPACITY];
static unsigned input_count;
static unsigned input_read;
static unsigned held_block_calls;
static unsigned resets[3];
static unsigned all_red_calls;
static unsigned stopped[TMR_COUNT];
static unsigned lcd_view_calls;
static app_mode_t lcd_mode;
static traffic_phase_t lcd_phase;
static tuning_field_t lcd_field;
static bool lcd_dirty;

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
    ++stopped[id];
}

void traffic_force_all_red(void)
{
    ++all_red_calls;
}

void lcd_set_view(app_mode_t mode, traffic_phase_t phase,
                  tuning_field_t field, bool dirty)
{
    ++lcd_view_calls;
    lcd_mode = mode;
    lcd_phase = phase;
    lcd_field = field;
    lcd_dirty = dirty;
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
    lcd_view_calls = 0U;
    lcd_mode = MODE_AUTO;
    lcd_phase = PHASE_SAFE;
    lcd_field = FIELD_R1;
    lcd_dirty = false;

    for (unsigned i = 0U; i < 3U; ++i) {
        resets[i] = 0U;
        stopped[i] = 0U;
    }

    mode_manager_init();
}

static void press_mode(void)
{
    push(BTN_1, BTN_EVT_SHORT);
    mode_manager_run();
}

static void test_mode_cycle_and_lcd(void)
{
    setup();
    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(resets[MODE_AUTO] == 1U);
    assert(all_red_calls == 1U);
    assert(lcd_view_calls == 1U);
    assert(lcd_mode == MODE_AUTO && lcd_phase == PHASE_SAFE);

    press_mode();
    assert(mode_manager_get_mode() == MODE_MANUAL);
    assert(resets[MODE_MANUAL] == 1U);
    assert(lcd_mode == MODE_MANUAL && lcd_phase == PHASE_SAFE);

    press_mode();
    assert(mode_manager_get_mode() == MODE_TUNING);
    assert(resets[MODE_TUNING] == 1U);
    assert(g_app_state.phase == PHASE_SAFE);
    assert(lcd_mode == MODE_TUNING && lcd_field == FIELD_R1);
    assert(!lcd_dirty);

    press_mode();
    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(resets[MODE_AUTO] == 2U);
    assert(all_red_calls == 4U);
    assert(held_block_calls == 4U);
    assert(lcd_view_calls == 4U);
    for (unsigned i = 0U; i < (unsigned)TMR_COUNT; ++i) {
        assert(stopped[i] == 4U);
    }
}

static void test_mode_priority_and_old_actions(void)
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
    assert(input_count == 0U);

    push(BTN_1, BTN_EVT_SHORT);
    push(BTN_3, BTN_EVT_DOWN);
    mode_manager_run();
    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(!mode_manager_take_action(&event));
    assert(held_block_calls == 4U);
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
    assert(!mode_manager_take_action(NULL));
    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_2 && event.kind == BTN_EVT_SHORT);
    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_3 && event.kind == BTN_EVT_DOWN);
    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_4 && event.kind == BTN_EVT_DOWN);
    assert(!mode_manager_take_action(&event));
}

static void test_tuning_event_filter(void)
{
    setup();
    press_mode();
    press_mode();

    push(BTN_2, BTN_EVT_DOWN);
    push(BTN_3, BTN_EVT_SHORT);
    push(BTN_4, BTN_EVT_LONG);
    push(BTN_2, BTN_EVT_SHORT);
    push(BTN_2, BTN_EVT_LONG);
    push(BTN_3, BTN_EVT_DOWN);
    push(BTN_4, BTN_EVT_DOWN);
    mode_manager_run();

    button_event_t event;
    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_2 && event.kind == BTN_EVT_SHORT);
    assert(mode_manager_take_action(&event));
    assert(event.id == BTN_2 && event.kind == BTN_EVT_LONG);
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

    assert(config_adjust_staged(FIELD_G1, 1));
    assert(config_adjust_staged(FIELD_G1, 1));
    assert(config_save_edit());
    assert(config_adjust_staged(FIELD_G1, 1));
    assert(config_is_dirty());

    press_mode();
    assert(mode_manager_get_mode() == MODE_AUTO);
    assert(config_get_active()->green_s[ROAD_1] == 12U);
    assert(config_get_staged()->green_s[ROAD_1] == 12U);
    assert(!config_is_dirty());
}

static void test_inbox_full_preserves_stop(void)
{
    setup();
    press_mode();

    for (unsigned i = 0U; i < MOCK_QUEUE_CAPACITY; ++i) {
        push(BTN_2, BTN_EVT_SHORT);
    }
    mode_manager_run();

    push(BTN_3, BTN_EVT_DOWN);
    mode_manager_run();
    assert((g_app_state.fault_flags & FAULT_BUTTON_QUEUE_FULL) != 0U);

    unsigned next = 0U;
    unsigned stop = 0U;
    button_event_t event;
    while (mode_manager_take_action(&event)) {
        if (event.id == BTN_2) ++next;
        if (event.id == BTN_3) ++stop;
    }
    assert(next == 7U && stop == 1U);
    assert(all_red_calls == 2U); /* Diagnostic overflow is not fatal. */
}

static void test_inbox_full_drops_noncritical(void)
{
    setup();
    press_mode();

    for (unsigned i = 0U; i < MOCK_QUEUE_CAPACITY; ++i) {
        push(BTN_3, BTN_EVT_DOWN);
    }
    mode_manager_run();
    push(BTN_2, BTN_EVT_SHORT);
    mode_manager_run();

    assert((g_app_state.fault_flags & FAULT_BUTTON_QUEUE_FULL) != 0U);
    unsigned count = 0U;
    button_event_t event;
    while (mode_manager_take_action(&event)) {
        assert(event.id == BTN_3 && event.kind == BTN_EVT_DOWN);
        ++count;
    }
    assert(count == MOCK_QUEUE_CAPACITY);
}

static void test_traffic_safety_fault_latches(void)
{
    setup();
    press_mode();
    push(BTN_2, BTN_EVT_SHORT);
    mode_manager_run();
    unsigned previous_red_calls = all_red_calls;

    global_raise_fault(FAULT_TRAFFIC_INVALID_PAIR);
    push(BTN_1, BTN_EVT_SHORT);
    mode_manager_run();

    button_event_t event;
    assert(all_red_calls == previous_red_calls + 1U);
    assert(mode_manager_get_mode() == MODE_MANUAL);
    assert(g_app_state.phase == PHASE_SAFE);
    assert(!mode_manager_take_action(&event));
    assert(input_count == 0U);
    assert(lcd_mode == MODE_MANUAL && lcd_phase == PHASE_SAFE);

    mode_manager_run();
    assert(all_red_calls == previous_red_calls + 1U);
}

static void test_missing_core_task_fault(void)
{
    setup();
    global_raise_fault(FAULT_CORE_TASK_MISSING);
    mode_manager_run();
    assert(all_red_calls == 2U);
    assert(g_app_state.phase == PHASE_SAFE);
    assert(mode_manager_get_mode() == MODE_AUTO);

    press_mode();
    assert(mode_manager_get_mode() == MODE_AUTO);
}

static void test_diagnostic_fault_does_not_stop_mode(void)
{
    setup();
    global_raise_fault(FAULT_BUTTON_QUEUE_FULL);
    press_mode();
    assert(mode_manager_get_mode() == MODE_MANUAL);
    assert(all_red_calls == 2U);
}

int main(void)
{
    test_mode_cycle_and_lcd();
    test_mode_priority_and_old_actions();
    test_manual_routing();
    test_tuning_event_filter();
    test_tuning_keeps_saved_values();
    test_inbox_full_preserves_stop();
    test_inbox_full_drops_noncritical();
    test_traffic_safety_fault_latches();
    test_missing_core_task_fault();
    test_diagnostic_fault_does_not_stop_mode();

    puts("PASS: 10 mode manager test groups");
    return 0;
}
