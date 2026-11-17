#include <assert.h>
#include <stdio.h>

#include "scheduler.h"
#include "software_timer.h"

static void setup(void)
{
    SCH_Init();
    swtimer_init();

    uint32_t task_id = SCH_Add_Task(swtimer_tick_10ms, 1U, 1U);
    assert(task_id != SCH_INVALID_TASK_ID);
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
    advance(1U); /* Leave the expiry flag unread. */

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
    assert(!swtimer_expired(TMR_COUNT));
    assert(swtimer_remaining(TMR_COUNT) == 0U);
}

int main(void)
{
    test_exact_expiry();
    test_stop_and_restart();
    test_late_dispatch();
    test_invalid_inputs();

    puts("PASS: 4 software timer tests");
    return 0;
}