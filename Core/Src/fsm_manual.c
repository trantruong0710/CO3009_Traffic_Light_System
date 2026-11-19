
#include "fsm_manual.h"
#include "app_types.h"
#include "global.h"
#include "mode_manager.h"
#include "software_timer.h"
#include "traffic_light.h"

#define CLEARANCE_TICKS (CLEARANCE_S * 100U)
#define MAX_EVENTS_PER_RUN 8U

static bool waiting_clearance;
static bool restart_pending;
static bool fault_latched;

static void enter_fault_state(void)
{
    swtimer_stop(TMR_CLEARANCE);
    traffic_force_all_red();
    g_app_state.phase = PHASE_SAFE;

    waiting_clearance = false;
    restart_pending = false;
    fault_latched = true;
}

static void start_safe_clearance(bool restart)
{
    swtimer_stop(TMR_CLEARANCE);
    traffic_force_all_red();

    g_app_state.phase = PHASE_SAFE;
    waiting_clearance = true;
    restart_pending = restart;

    if (!swtimer_start(TMR_CLEARANCE, CLEARANCE_TICKS)) {
        enter_fault_state();
    }
}

static void enter_phase(traffic_phase_t phase)
{
    light_t road1 = LIGHT_RED;
    light_t road2 = LIGHT_RED;

    switch (phase) {
    case PHASE_P0:
        road2 = LIGHT_GREEN;
        break;

    case PHASE_P1:
        road2 = LIGHT_AMBER;
        break;

    case PHASE_P2:
        break;

    case PHASE_P3:
        road1 = LIGHT_GREEN;
        break;

    case PHASE_P4:
        road1 = LIGHT_AMBER;
        break;

    case PHASE_P5:
        break;

    default:
        enter_fault_state();
        return;
    }

    if (traffic_apply_pair(road1, road2) != TRAFFIC_OK) {
        enter_fault_state();
        return;
    }

    g_app_state.phase = phase;

    if (phase == PHASE_P2 || phase == PHASE_P5) {
        swtimer_stop(TMR_CLEARANCE);
        waiting_clearance = true;
        restart_pending = false;

        if (!swtimer_start(
                TMR_CLEARANCE, CLEARANCE_TICKS)) {
            enter_fault_state();
        }
    }
}

void fsm_manual_reset(void)
{
    swtimer_stop(TMR_PHASE);

    fault_latched = false;
    waiting_clearance = false;
    restart_pending = false;

    start_safe_clearance(false);
}

void fsm_manual_run(void)
{
    if (g_app_state.mode != MODE_MANUAL ||
        fault_latched) {
        return;
    }

    /*if (waiting_clearance &&
        swtimer_expired(TMR_CLEARANCE)) {

        waiting_clearance = false;

        if (restart_pending) {
            restart_pending = false;
            enter_phase(PHASE_P0);

            if (fault_latched) {
                return;
            }
        }
    } */

    /*
     * Collect events first so STOP always takes
     * precedence over RESTART and NEXT.
     */
    bool clearance_was_pending = waiting_clearance;
    bool stop_requested = false;
    bool restart_requested = false;
    bool next_requested = false;
    button_event_t event;

    for (uint8_t i = 0U;
         i < MAX_EVENTS_PER_RUN &&
         mode_manager_take_action(&event);
         i++) {

        if (event.id == BTN_3 &&
            event.kind == BTN_EVT_DOWN) {
            stop_requested = true;
        } else if (event.id == BTN_4 &&
                   event.kind == BTN_EVT_DOWN) {
            restart_requested = true;
        } else if (event.id == BTN_2 &&
                   event.kind == BTN_EVT_SHORT) {
            next_requested = true;
        }
    }

    if (stop_requested) {
        start_safe_clearance(false);
        return;
    }

    if (restart_requested) {
        start_safe_clearance(true);
        return;
    }
    /* Process timer expiry only after STOP/RESTART priority. */
if (waiting_clearance &&
    swtimer_expired(TMR_CLEARANCE)) {

    waiting_clearance = false;

    if (restart_pending) {
        restart_pending = false;
        enter_phase(PHASE_P0);
        return;
    }
}

    if (!next_requested ||
    waiting_clearance ||
    clearance_was_pending) {
    return;
}

    traffic_phase_t next;

    switch (g_app_state.phase) {
    case PHASE_SAFE:
        next = PHASE_P0;
        break;
    case PHASE_P0:
        next = PHASE_P1;
        break;
    case PHASE_P1:
        next = PHASE_P2;
        break;
    case PHASE_P2:
        next = PHASE_P3;
        break;
    case PHASE_P3:
        next = PHASE_P4;
        break;
    case PHASE_P4:
        next = PHASE_P5;
        break;
    case PHASE_P5:
        next = PHASE_P0;
        break;
    default:
        enter_fault_state();
        return;
    }

    enter_phase(next);
}
