#include "mode_manager.h"

#include "button.h"
#include "config.h"
#include "fsm_automatic.h"
#include "fsm_manual.h"
#include "fsm_tuning.h"
#include "global.h"
#include "lcd.h"
#include "software_timer.h"
#include "traffic_light.h"

#include <stddef.h>

#define ACTION_CAPACITY 8U

/* C5 fault-bit agreement with TV2; keep public headers unchanged.
 * Bit 0: critical traffic interlock, bit 1: noncritical button/inbox overflow.
 * Bit 2: critical missing core task (reserved for TV1 integration, C6).
 */
#define FAULT_TRAFFIC_INVALID_PAIR (1UL << 0)
#define FAULT_BUTTON_QUEUE_FULL   (1UL << 1)
#define FAULT_CORE_TASK_MISSING   (1UL << 2)
#define SAFETY_FAULT_MASK (FAULT_TRAFFIC_INVALID_PAIR | FAULT_CORE_TASK_MISSING)

static button_event_t actions[ACTION_CAPACITY];
static uint8_t action_count;
static bool safety_latched;

static app_mode_t next_mode(app_mode_t current)
{
    switch (current) {
    case MODE_AUTO:
        return MODE_MANUAL;
    case MODE_MANUAL:
        return MODE_TUNING;
    case MODE_TUNING:
    default:
        return MODE_AUTO;
    }
}

static bool is_manual_stop(button_event_t event)
{
    return event.id == BTN_3 && event.kind == BTN_EVT_DOWN;
}

static bool is_action_for_mode(app_mode_t mode, button_event_t event)
{
    switch (mode) {
    case MODE_MANUAL:
        return (event.id == BTN_2 && event.kind == BTN_EVT_SHORT) ||
               is_manual_stop(event) ||
               (event.id == BTN_4 && event.kind == BTN_EVT_DOWN);
    case MODE_TUNING:
        return (event.id == BTN_2 &&
                (event.kind == BTN_EVT_SHORT || event.kind == BTN_EVT_LONG)) ||
               ((event.id == BTN_3 || event.id == BTN_4) &&
                event.kind == BTN_EVT_DOWN);
    case MODE_AUTO:
    default:
        return false;
    }
}

static bool safety_fault_active(void)
{
    return (g_app_state.fault_flags & SAFETY_FAULT_MASK) != 0U;
}

static void stop_control_timers(void)
{
    swtimer_stop(TMR_PHASE);
    swtimer_stop(TMR_CLEARANCE);
    swtimer_stop(TMR_UI_MSG);
}

static void clear_input(void)
{
    action_count = 0U;
    button_clear_events();
    button_block_held_until_release();
}

static void enter_safety_lockdown(void)
{
    traffic_force_all_red();
    g_app_state.phase = PHASE_SAFE;
    stop_control_timers();
    clear_input();
    lcd_set_view(g_app_state.mode, PHASE_SAFE, FIELD_R1, false);
    safety_latched = true;
}

static void queue_action(app_mode_t mode, button_event_t event)
{
    if (action_count < ACTION_CAPACITY) {
        actions[action_count++] = event;
        return;
    }

    /* Both the raw button FIFO and action inbox share the diagnostic bit. */
    global_raise_fault(FAULT_BUTTON_QUEUE_FULL);

    if (mode == MODE_MANUAL && is_manual_stop(event)) {
        /* Preserve an incoming STOP by evicting the oldest non-STOP.
         * Existing STOP events are not discarded. MODE is handled directly.
         */
        for (uint8_t i = 0U; i < action_count; ++i) {
            if (!is_manual_stop(actions[i])) {
                for (uint8_t j = i + 1U; j < action_count; ++j) {
                    actions[j - 1U] = actions[j];
                }
                actions[action_count - 1U] = event;
                return;
            }
        }
    }
    /* Full of higher/equal-priority events: do not invent an action. */
}

static void enter_mode(app_mode_t mode)
{
    /* Make the output safe before clearing timers or switching the mode. */
    traffic_force_all_red();
    g_app_state.phase = PHASE_SAFE;
    stop_control_timers();
    clear_input();

    g_app_state.mode = mode;

    switch (mode) {
    case MODE_MANUAL:
        fsm_manual_reset();
        break;
    case MODE_TUNING:
        config_begin_edit();
        fsm_tuning_reset();
        break;
    case MODE_AUTO:
    default:
        fsm_automatic_reset();
        break;
    }

    /* This updates the LCD view model only; no I2C transactions here. */
    lcd_set_view(mode, PHASE_SAFE, FIELD_R1, false);
}

void mode_manager_init(void)
{
    safety_latched = false;
    enter_mode(MODE_AUTO);
    if (safety_fault_active()) {
        enter_safety_lockdown();
    }
}

void mode_manager_run(void)
{
    button_event_t event;
    app_mode_t mode = g_app_state.mode;

    if (safety_latched) {
        return;
    }

    if (safety_fault_active()) {
        enter_safety_lockdown();
        return;
    }

    while (button_pop_event(&event)) {
        if (event.id == BTN_1 && event.kind == BTN_EVT_SHORT) {
            if (mode == MODE_TUNING) {
                config_cancel_edit();
            }
            enter_mode(next_mode(mode));
            return;
        }

        if (is_action_for_mode(mode, event)) {
            queue_action(mode, event);
        }
    }
}

app_mode_t mode_manager_get_mode(void)
{
    return g_app_state.mode;
}

bool mode_manager_take_action(button_event_t *out_event)
{
    if (out_event == NULL || action_count == 0U ||
        safety_latched || safety_fault_active()) {
        return false;
    }

    *out_event = actions[0];

    for (uint8_t i = 1U; i < action_count; ++i) {
        actions[i - 1U] = actions[i];
    }

    --action_count;
    return true;
}
