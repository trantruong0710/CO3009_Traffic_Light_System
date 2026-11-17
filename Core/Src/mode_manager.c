#include "mode_manager.h"

#include "button.h"
#include "config.h"
#include "fsm_automatic.h"
#include "fsm_manual.h"
#include "fsm_tuning.h"
#include "global.h"
#include "software_timer.h"
#include "traffic_light.h"

#include <stddef.h>

#define ACTION_CAPACITY 8U

static button_event_t actions[ACTION_CAPACITY];
static uint8_t action_count;

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
        return event.id == BTN_2 || event.id == BTN_3 || event.id == BTN_4;

    case MODE_AUTO:
    default:
        return false;
    }
}

static void queue_action(app_mode_t mode, button_event_t event)
{
    if (action_count < ACTION_CAPACITY) {
        actions[action_count++] = event;
    } else if (mode == MODE_MANUAL && is_manual_stop(event)) {
        /* If the queue is full, keep STOP ahead of older manual actions. */
        actions[0] = event;
        action_count = 1U;
    }
}

static void enter_mode(app_mode_t mode)
{
    action_count = 0U;
    button_clear_events();
    button_block_held_until_release();

    swtimer_stop(TMR_PHASE);
    swtimer_stop(TMR_CLEARANCE);
    swtimer_stop(TMR_UI_MSG);

    g_app_state.mode = mode;

    switch (mode) {
    case MODE_MANUAL:
        fsm_manual_reset();
        break;

    case MODE_TUNING:
        traffic_force_all_red();
        g_app_state.phase = PHASE_SAFE;
        config_begin_edit();
        fsm_tuning_reset();
        break;

    case MODE_AUTO:
    default:
        fsm_automatic_reset();
        break;
    }
}

void mode_manager_init(void)
{
    enter_mode(MODE_AUTO);
}

void mode_manager_run(void)
{
    app_mode_t mode = g_app_state.mode;
    button_event_t event;

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
    if (out_event == NULL || action_count == 0U) {
        return false;
    }

    *out_event = actions[0];

    for (uint8_t i = 1U; i < action_count; ++i) {
        actions[i - 1U] = actions[i];
    }

    --action_count;
    return true;
}