
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fsm_manual.h"
#include "global.h"
#include "mode_manager.h"
#include "software_timer.h"
#include "traffic_light.h"

typedef struct {
    uint32_t remaining;
    bool running;
    bool expired;
} mock_timer_t;

static mock_timer_t timers[TMR_COUNT];
static button_event_t inbox[16];
static unsigned inbox_count;
static unsigned inbox_read;

static light_t r1, r2;
static bool fail_traffic;
static bool fail_clearance;

bool swtimer_start(soft_timer_id_t id, uint32_t ticks)
{
    if (id >= TMR_COUNT || ticks == 0U ||
        (id == TMR_CLEARANCE && fail_clearance))
        return false;

    timers[id] = (mock_timer_t){ticks, true, false};
    return true;
}

void swtimer_stop(soft_timer_id_t id)
{
    if (id < TMR_COUNT)
        timers[id] = (mock_timer_t){0};
}

bool swtimer_expired(soft_timer_id_t id)
{
    if (id >= TMR_COUNT || !timers[id].expired)
        return false;

    timers[id].expired = false;
    return true;
}

void traffic_force_all_red(void)
{
    r1 = LIGHT_RED;
    r2 = LIGHT_RED;
}

traffic_result_t traffic_apply_pair(light_t a, light_t b)
{
    if (fail_traffic)
        return TRAFFIC_HW_NOT_READY;

    assert(!(a == LIGHT_GREEN && b == LIGHT_GREEN));
    r1 = a;
    r2 = b;
    return TRAFFIC_OK;
}

bool mode_manager_take_action(button_event_t *out)
{
    if (inbox_read == inbox_count)
        return false;

    *out = inbox[inbox_read++];
    return true;
}

static void push(button_id_t id, button_kind_t kind)
{
    assert(inbox_count < 16);
    inbox[inbox_count++] = (button_event_t){id, kind};
}

static void run_actions(void)
{
    fsm_manual_run();
    inbox_count = 0;
    inbox_read = 0;
}

static void next(void)
{
    push(BTN_2, BTN_EVT_SHORT);
    run_actions();
}

static void stop(void)
{
    push(BTN_3, BTN_EVT_DOWN);
    run_actions();
}

static void restart(void)
{
    push(BTN_4, BTN_EVT_DOWN);
    run_actions();
}

static void check(traffic_phase_t phase, light_t a, light_t b)
{
    assert(g_app_state.phase == phase);
    assert(r1 == a && r2 == b);
    assert(!(r1 == LIGHT_GREEN && r2 == LIGHT_GREEN));
}

static void tick(uint32_t count)
{
    while (count--) {
        for (unsigned i = 0; i < TMR_COUNT; i++) {
            if (timers[i].running && timers[i].remaining) {
                if (--timers[i].remaining == 0U) {
                    timers[i].running = false;
                    timers[i].expired = true;
                }
            }
        }
        fsm_manual_run();
        assert(!(r1 == LIGHT_GREEN && r2 == LIGHT_GREEN));
    }
}

static void setup(void)
{
    memset(timers, 0, sizeof(timers));
    inbox_count = 0;
    inbox_read = 0;
    fail_traffic = false;
    fail_clearance = false;

    global_init();
    g_app_state.mode = MODE_MANUAL;
    fsm_manual_reset();
}

static void enter_p0(void)
{
    tick(100);
    next();
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* 1. Initial clearance and blocked early NEXT. */
static void test_initial(void)
{
    setup();
    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tick(99);
    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tick(1);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
    next();
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* 2. Six-phase sequence and mandatory clearance. */
static void test_six_phases(void)
{
    setup();
    enter_p0();

    next();
    check(PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    next();
    check(PHASE_P2, LIGHT_RED, LIGHT_RED);

    next();
    check(PHASE_P2, LIGHT_RED, LIGHT_RED);

    tick(99);
    next();
    check(PHASE_P2, LIGHT_RED, LIGHT_RED);

    tick(1);
    next();
    check(PHASE_P3, LIGHT_GREEN, LIGHT_RED);

    next();
    check(PHASE_P4, LIGHT_AMBER, LIGHT_RED);

    next();
    check(PHASE_P5, LIGHT_RED, LIGHT_RED);

    next();
    check(PHASE_P5, LIGHT_RED, LIGHT_RED);

    tick(100);
    next();
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* 3. No automatic advance in Manual. */
static void test_no_automatic_advance(void)
{
    setup();
    enter_p0();

    tick(10000);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* 4. STOP holds SAFE until the next valid NEXT. */
static void test_stop(void)
{
    setup();
    enter_p0();

    stop();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tick(100);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    next();
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* 5. RESTART automatically enters P0 after 1 s. */
static void test_restart(void)
{
    setup();
    enter_p0();

    restart();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tick(99);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tick(1);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

/* 6. STOP wins over RESTART and NEXT. */
static void test_priority(void)
{
    setup();
    enter_p0();

    push(BTN_2, BTN_EVT_SHORT);
    push(BTN_4, BTN_EVT_DOWN);
    push(BTN_3, BTN_EVT_DOWN);
    run_actions();

    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tick(100);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}

/* 7. Traffic driver failure latches SAFE. */
static void test_driver_failure(void)
{
    setup();
    tick(100);

    fail_traffic = true;
    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    fail_traffic = false;
    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}

/* 8. Clearance timer failure latches SAFE. */
static void test_timer_failure(void)
{
    setup();
    enter_p0();
    next();

    fail_clearance = true;
    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    fail_clearance = false;
    tick(1000);
    next();
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);
}

/* 9. Ignore irrelevant events and wrong mode. */
static void test_mode_and_events(void)
{
    setup();
    enter_p0();

    push(BTN_1, BTN_EVT_SHORT);
    push(BTN_2, BTN_EVT_LONG);
    run_actions();
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    g_app_state.mode = MODE_AUTO;
    stop();
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

int main(void)
{
    test_initial();
    test_six_phases();
    test_no_automatic_advance();
    test_stop();
    test_restart();
    test_priority();
    test_driver_failure();
    test_timer_failure();
    test_mode_and_events();

    puts("PASS: 9/9 Manual FSM unit tests");
    return 0;
}
