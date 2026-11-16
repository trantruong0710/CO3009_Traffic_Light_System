
#include "fsm_automatic.h"
#include "global.h"
#include "software_timer.h"
#include "traffic_light.h"

#define TICKS_PER_SECOND 100U

static bool waiting_clearance;
static bool phase_running;
static bool stopped;

static void enter_phase(traffic_phase_t phase)
{
    light_t r1 = LIGHT_RED;
    light_t r2 = LIGHT_RED;
    uint32_t ticks = 0U;

    const traffic_config_t *cfg = &g_app_state.active_config;

    switch (phase) {
    case PHASE_P0:
        r2 = LIGHT_GREEN;
        ticks = cfg->green_s[ROAD_2] * TICKS_PER_SECOND;
        break;
    case PHASE_P1:
        r2 = LIGHT_AMBER;
        ticks = cfg->amber_s[ROAD_2] * TICKS_PER_SECOND;
        break;
    case PHASE_P2:
        ticks = CLEARANCE_S * TICKS_PER_SECOND;
        break;
    case PHASE_P3:
        r1 = LIGHT_GREEN;
        ticks = cfg->green_s[ROAD_1] * TICKS_PER_SECOND;
        break;
    case PHASE_P4:
        r1 = LIGHT_AMBER;
        ticks = cfg->amber_s[ROAD_1] * TICKS_PER_SECOND;
        break;
    case PHASE_P5:
        ticks = CLEARANCE_S * TICKS_PER_SECOND;
        break;
    default:
        stopped = true;
        traffic_force_all_red();
        return;
    }

    if (ticks == 0U ||
        traffic_apply_pair(r1, r2) != TRAFFIC_OK) {
        stopped = true;
        traffic_force_all_red();
        g_app_state.phase = PHASE_SAFE;
        return;
    }

    if (!swtimer_start(TMR_PHASE, ticks)) {
        stopped = true;
        traffic_force_all_red();
        g_app_state.phase = PHASE_SAFE;
        return;
    }

    g_app_state.phase = phase;
    phase_running = true;
}

void fsm_automatic_reset(void)
{
    swtimer_stop(TMR_PHASE);
    swtimer_stop(TMR_CLEARANCE);

    traffic_force_all_red();
    g_app_state.phase = PHASE_SAFE;

    waiting_clearance = true;
    phase_running = false;
    stopped = false;

    if (!swtimer_start(
            TMR_CLEARANCE,
            CLEARANCE_S * TICKS_PER_SECOND)) {
        stopped = true;
    }
}

void fsm_automatic_run(void)
{
    if (g_app_state.mode != MODE_AUTO || stopped) {
        return;
    }

    if (waiting_clearance) {
        if (swtimer_expired(TMR_CLEARANCE)) {
            waiting_clearance = false;
            enter_phase(PHASE_P0);
        }
        return;
    }

    if (phase_running && swtimer_expired(TMR_PHASE)) {
        traffic_phase_t next =
            (g_app_state.phase == PHASE_P5)
                ? PHASE_P0
                : (traffic_phase_t)(g_app_state.phase + 1);

        enter_phase(next);
    }
}
