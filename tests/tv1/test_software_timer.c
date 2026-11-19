#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "scheduler.h"
#include "software_timer.h"

#ifdef SCH_TEST
/* Private test hook: implemented only when scheduler is built with SCH_TEST. */
void SCH_Test_SetTick(uint32_t tick);
#endif

static void setup(void)
{
    SCH_Init();
    swtimer_init();
    assert(SCH_Add_Task(swtimer_tick_10ms, 1U, 1U) != SCH_INVALID_TASK_ID);
}

static void advance(unsigned ticks)
{
    for (unsigned i = 0U; i < ticks; ++i) {
        SCH_Update();
        SCH_Dispatch_Tasks();
    }
}

static void test_exact_expiry(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 3U));
    advance(2U);
    assert(swtimer_remaining(TMR_PHASE) == 1U);
    assert(!swtimer_expired(TMR_PHASE));
    advance(1U);
    assert(swtimer_remaining(TMR_PHASE) == 0U);
    assert(swtimer_expired(TMR_PHASE));
    assert(!swtimer_expired(TMR_PHASE));
}

static void test_stop_and_restart(void)
{
    setup();
    assert(swtimer_start(TMR_CLEARANCE, 2U));
    advance(1U);
    swtimer_stop(TMR_CLEARANCE);
    advance(3U);
    assert(swtimer_remaining(TMR_CLEARANCE) == 0U);
    assert(!swtimer_expired(TMR_CLEARANCE));
    assert(swtimer_start(TMR_CLEARANCE, 1U));
    advance(1U);
    assert(swtimer_start(TMR_CLEARANCE, 2U));
    assert(!swtimer_expired(TMR_CLEARANCE));
    assert(swtimer_remaining(TMR_CLEARANCE) == 2U);
}

static void test_late_dispatch(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 2U));
    assert(swtimer_start(TMR_UI_MSG, 4U));
    SCH_Update();
    SCH_Update();
    SCH_Update();
    SCH_Dispatch_Tasks();
    assert(swtimer_expired(TMR_PHASE));
    assert(!swtimer_expired(TMR_UI_MSG));
    assert(swtimer_remaining(TMR_UI_MSG) == 1U);
    advance(1U);
    assert(swtimer_expired(TMR_UI_MSG));
}

static void test_invalid_inputs(void)
{
    setup();
    assert(!swtimer_start(TMR_PHASE, 0U));
    assert(!swtimer_start(TMR_COUNT, 1U));
    assert(!swtimer_start(TMR_PHASE, (uint32_t)INT32_MAX + 1U));
    assert(!swtimer_expired(TMR_COUNT));
    assert(swtimer_remaining(TMR_COUNT) == 0U);
    swtimer_stop(TMR_COUNT);
}

static void test_overdue_without_service(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 3U));
    for (unsigned i = 0U; i < 5U; ++i) {
        SCH_Update();
    }
    assert(swtimer_remaining(TMR_PHASE) == 0U);
    assert(!swtimer_expired(TMR_PHASE));
    SCH_Dispatch_Tasks();
    assert(swtimer_expired(TMR_PHASE));
    assert(!swtimer_expired(TMR_PHASE));
}

static void test_all_timer_slots(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 2U));
    assert(swtimer_start(TMR_CLEARANCE, 4U));
    assert(swtimer_start(TMR_UI_MSG, 3U));
    advance(2U);
    assert(swtimer_expired(TMR_PHASE));
    assert(!swtimer_expired(TMR_CLEARANCE));
    assert(!swtimer_expired(TMR_UI_MSG));
    advance(1U);
    assert(swtimer_expired(TMR_UI_MSG));
    assert(swtimer_remaining(TMR_CLEARANCE) == 1U);
    advance(1U);
    assert(swtimer_expired(TMR_CLEARANCE));
}

static void test_latched_once(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 1U));
    advance(5U);
    assert(swtimer_expired(TMR_PHASE));
    assert(!swtimer_expired(TMR_PHASE));
    advance(5U);
    assert(!swtimer_expired(TMR_PHASE));
}

static void test_restart_resets_deadline(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 5U));
    advance(2U);
    assert(swtimer_start(TMR_PHASE, 4U));
    advance(3U);
    assert(swtimer_remaining(TMR_PHASE) == 1U);
    assert(!swtimer_expired(TMR_PHASE));
    advance(1U);
    assert(swtimer_expired(TMR_PHASE));
}

static void test_invalid_start_preserves_timer(void)
{
    setup();
    assert(swtimer_start(TMR_PHASE, 5U));
    advance(1U);
    assert(!swtimer_start(TMR_PHASE, 0U));
    assert(!swtimer_start(TMR_PHASE, (uint32_t)INT32_MAX + 1U));
    assert(swtimer_remaining(TMR_PHASE) == 4U);
}

#ifdef SCH_TEST
static void test_tick_rollover(void)
{
    SCH_Init();
    swtimer_init();
    SCH_Test_SetTick(UINT32_MAX - 2U);
    assert(SCH_Add_Task(swtimer_tick_10ms, 1U, 1U) != SCH_INVALID_TASK_ID);
    assert(swtimer_start(TMR_PHASE, 5U));
    advance(3U);
    assert(swtimer_remaining(TMR_PHASE) == 2U);
    advance(2U);
    assert(swtimer_remaining(TMR_PHASE) == 0U);
    assert(swtimer_expired(TMR_PHASE));
}
#endif

int main(void)
{
    test_exact_expiry();
    test_stop_and_restart();
    test_late_dispatch();
    test_invalid_inputs();
    test_overdue_without_service();
    test_all_timer_slots();
    test_latched_once();
    test_restart_resets_deadline();
    test_invalid_start_preserves_timer();
#ifdef SCH_TEST
    test_tick_rollover();
    puts("PASS: 10 software timer test groups");
#else
    puts("PASS: 9 software timer test groups");
#endif
    return 0;
}
