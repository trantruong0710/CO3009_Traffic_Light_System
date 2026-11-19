
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_types.h"
#include "fsm_automatic.h"
#include "global.h"
#include "software_timer.h"
#include "traffic_light.h"

/* ===== MOCK SOFTWARE TIMER ===== */

typedef struct {
    uint32_t remaining;
    bool running;
    bool expired;
} mock_timer_t;

static mock_timer_t timers[TMR_COUNT];

static bool fail_phase_timer;
static bool fail_clearance_timer;
static bool fail_traffic_apply;

bool swtimer_start(soft_timer_id_t id, uint32_t ticks)
{
    if (id >= TMR_COUNT || ticks == 0U)
        return false;

    if ((id == TMR_PHASE && fail_phase_timer) ||
        (id == TMR_CLEARANCE && fail_clearance_timer)) {
        return false;
    }

    timers[id].remaining = ticks;
    timers[id].running = true;
    timers[id].expired = false;
    return true;
}

void swtimer_stop(soft_timer_id_t id)
{
    if (id < TMR_COUNT)
        memset(&timers[id], 0, sizeof(timers[id]));
}

bool swtimer_expired(soft_timer_id_t id)
{
    if (id >= TMR_COUNT || !timers[id].expired)
        return false;

    timers[id].expired = false;
    return true;
}

/* ===== MOCK TRAFFIC DRIVER ===== */

static light_t road1_light;
static light_t road2_light;

void traffic_force_all_red(void)
{
    road1_light = LIGHT_RED;
    road2_light = LIGHT_RED;
}

traffic_result_t traffic_apply_pair(light_t r1, light_t r2)
{
    if (fail_traffic_apply)
        return TRAFFIC_HW_NOT_READY;

    assert(!(r1 == LIGHT_GREEN && r2 == LIGHT_GREEN));

    road1_light = r1;
    road2_light = r2;
    return TRAFFIC_OK;
}

/* ===== TEST HELPERS ===== */

static void reset_environment(void)
{
    memset(timers, 0, sizeof(timers));

    fail_phase_timer = false;
    fail_clearance_timer = false;
    fail_traffic_apply = false;

    global_init();
    traffic_force_all_red();
}

static void advance_ticks(uint32_t ticks)
{
    while (ticks-- > 0U) {
        for (unsigned i = 0U; i < TMR_COUNT; i++) {
            if (timers[i].running &&
                timers[i].remaining > 0U) {

                timers[i].remaining--;

                if (timers[i].remaining == 0U) {
                    timers[i].running = false;
                    timers[i].expired = true;
                }
            }
        }

        fsm_automatic_run();

        /* Safety invariant at every simulated tick. */
        assert(!(road1_light == LIGHT_GREEN &&
                 road2_light == LIGHT_GREEN));
    }
}

static void check_state(
    traffic_phase_t phase,
    light_t r1,
    light_t r2)
{
    assert(g_app_state.phase == phase);
    assert(road1_light == r1);
    assert(road2_light == r2);
}

static void check_transition(
    uint32_t duration,
    traffic_phase_t current,
    light_t current_r1,
    light_t current_r2,
    traffic_phase_t next,
    light_t next_r1,
    light_t next_r2)
{
    advance_ticks(duration - 1U);
    check_state(current, current_r1, current_r2);

    advance_ticks(1U);
    check_state(next, next_r1, next_r2);
}

/* ===== TEST CASES ===== */

/* TEST 1: Initial clearance lasts exactly 1 second. */
static void test_initial_clearance(void)
{
    reset_environment();
    fsm_automatic_reset();

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance_ticks(99U);
    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance_ticks(1U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* TEST 2: All six phases and their exact durations. */
static void test_full_cycle(void)
{
    reset_environment();
    fsm_automatic_reset();

    advance_ticks(100U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    check_transition(
        1000U, PHASE_P0, LIGHT_RED, LIGHT_GREEN,
        PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    check_transition(
        300U, PHASE_P1, LIGHT_RED, LIGHT_AMBER,
        PHASE_P2, LIGHT_RED, LIGHT_RED);

    check_transition(
        100U, PHASE_P2, LIGHT_RED, LIGHT_RED,
        PHASE_P3, LIGHT_GREEN, LIGHT_RED);

    check_transition(
        1000U, PHASE_P3, LIGHT_GREEN, LIGHT_RED,
        PHASE_P4, LIGHT_AMBER, LIGHT_RED);

    check_transition(
        300U, PHASE_P4, LIGHT_AMBER, LIGHT_RED,
        PHASE_P5, LIGHT_RED, LIGHT_RED);

    check_transition(
        100U, PHASE_P5, LIGHT_RED, LIGHT_RED,
        PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* TEST 3: Reset must return to ALL_RED + clearance. */
static void test_reset_during_green(void)
{
    reset_environment();
    fsm_automatic_reset();

    advance_ticks(100U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    fsm_automatic_reset();
    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance_ticks(99U);
    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance_ticks(1U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* TEST 4: No automatic transitions in another mode. */
static void test_mode_guard(void)
{
    reset_environment();
    fsm_automatic_reset();

    g_app_state.mode = MODE_MANUAL;

    advance_ticks(150U);
    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    g_app_state.mode = MODE_AUTO;
    fsm_automatic_run();

    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* TEST 5: Hardware failure must stop the FSM. */
static void test_traffic_failure(void)
{
    reset_environment();
    fsm_automatic_reset();

    fail_traffic_apply = true;
    advance_ticks(100U);

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    fail_traffic_apply = false;
    advance_ticks(1000U);

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}

/* TEST 6: Phase timer failure must force ALL_RED. */
static void test_phase_timer_failure(void)
{
    reset_environment();
    fsm_automatic_reset();

    fail_phase_timer = true;
    advance_ticks(100U);

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    fail_phase_timer = false;
    advance_ticks(1000U);

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}

/* TEST 7: Clearance timer failure must stay SAFE. */
static void test_clearance_timer_failure(void)
{
    reset_environment();

    fail_clearance_timer = true;
    fsm_automatic_reset();

    advance_ticks(500U);

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}

/* TEST 8: Invalid zero duration must not activate GREEN. */
static void test_zero_duration(void)
{
    reset_environment();

    g_app_state.active_config.green_s[ROAD_2] = 0U;

    fsm_automatic_reset();
    advance_ticks(100U);

    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}
/* TEST 9: Invalid phase must enter a consistent SAFE state. */
static void test_invalid_phase_fails_safe(void)
{
    reset_environment();
    fsm_automatic_reset();

    /* Enter P0 normally. */
    advance_ticks(100U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    /* Simulate a corrupted phase value. */
    g_app_state.phase = (traffic_phase_t)99;

    /* Expire the current phase timer. */
    advance_ticks(1000U);

    /* Both physical output and shared state must be SAFE. */
    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    /* FSM must remain stopped after detecting the error. */
    advance_ticks(100U);
    check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}
/* TEST 10: Reset from every phase must restart safe clearance. */
static void test_reset_from_every_phase(void)
{
    static const struct {
        uint32_t offset;
        traffic_phase_t phase;
        light_t r1;
        light_t r2;
    } cases[] = {
        {   0U, PHASE_P0, LIGHT_RED,   LIGHT_GREEN },
        {1000U, PHASE_P1, LIGHT_RED,   LIGHT_AMBER },
        {1300U, PHASE_P2, LIGHT_RED,   LIGHT_RED   },
        {1400U, PHASE_P3, LIGHT_GREEN, LIGHT_RED   },
        {2400U, PHASE_P4, LIGHT_AMBER, LIGHT_RED   },
        {2700U, PHASE_P5, LIGHT_RED,   LIGHT_RED   }
    };

    for (unsigned i = 0U;
         i < sizeof(cases) / sizeof(cases[0]);
         i++) {

        reset_environment();
        fsm_automatic_reset();

        /* Complete startup clearance and reach target phase. */
        advance_ticks(100U);
        advance_ticks(cases[i].offset);

        check_state(cases[i].phase,
                    cases[i].r1,
                    cases[i].r2);

        /* Reset must cancel the old phase timer. */
        fsm_automatic_reset();

        check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

        assert(!timers[TMR_PHASE].running);
        assert(timers[TMR_CLEARANCE].running);
        assert(timers[TMR_CLEARANCE].remaining == 100U);

        /* No premature GREEN at tick 99. */
        advance_ticks(99U);
        check_state(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

        /* GREEN is permitted at tick 100. */
        advance_ticks(1U);
        check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
    }
}
/* TEST 11: FSM must use the active configuration durations. */
static void test_custom_phase_durations(void)
{
    reset_environment();

    g_app_state.active_config.green_s[ROAD_1] = 5U;
    g_app_state.active_config.amber_s[ROAD_1] = 2U;
    g_app_state.active_config.green_s[ROAD_2] = 7U;
    g_app_state.active_config.amber_s[ROAD_2] = 4U;

    fsm_automatic_reset();

    advance_ticks(100U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    check_transition(
        700U, PHASE_P0, LIGHT_RED, LIGHT_GREEN,
        PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    check_transition(
        400U, PHASE_P1, LIGHT_RED, LIGHT_AMBER,
        PHASE_P2, LIGHT_RED, LIGHT_RED);

    check_transition(
        100U, PHASE_P2, LIGHT_RED, LIGHT_RED,
        PHASE_P3, LIGHT_GREEN, LIGHT_RED);

    check_transition(
        500U, PHASE_P3, LIGHT_GREEN, LIGHT_RED,
        PHASE_P4, LIGHT_AMBER, LIGHT_RED);

    check_transition(
        200U, PHASE_P4, LIGHT_AMBER, LIGHT_RED,
        PHASE_P5, LIGHT_RED, LIGHT_RED);

    check_transition(
        100U, PHASE_P5, LIGHT_RED, LIGHT_RED,
        PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}
/* TEST 12: Expiry latch must trigger only one phase transition. */
static void test_expiry_consumed_once(void)
{
    reset_environment();
    fsm_automatic_reset();

    advance_ticks(100U);
    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    /* Repeated dispatch without advancing time. */
    for (unsigned i = 0U; i < 10U; i++) {
        fsm_automatic_run();
    }

    check_state(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    /* Reach the exact phase deadline. */
    advance_ticks(1000U);
    check_state(PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    /* Old expiry must not trigger another transition. */
    for (unsigned i = 0U; i < 10U; i++) {
        fsm_automatic_run();
    }

    check_state(PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    advance_ticks(299U);
    check_state(PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    advance_ticks(1U);
    check_state(PHASE_P2, LIGHT_RED, LIGHT_RED);
}
int main(void)
{
    test_initial_clearance();
    test_full_cycle();
    test_reset_during_green();
    test_mode_guard();
    test_traffic_failure();
    test_phase_timer_failure();
    test_clearance_timer_failure();
    test_zero_duration();
    test_invalid_phase_fails_safe();
    test_reset_from_every_phase();
    test_custom_phase_durations();
    test_expiry_consumed_once();

    puts("PASS: 12/12 Automatic FSM unit tests");
    return 0;
}
