#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "scheduler.h"

static unsigned periodic_runs;
static unsigned one_shot_runs;
static uint32_t last_periodic_tick;
static uint32_t last_one_shot_tick;
static uint32_t self_id;

static void periodic_task(void)
{
    ++periodic_runs;
    last_periodic_tick = SCH_GetTick();
}

static void one_shot_task(void)
{
    ++one_shot_runs;
    last_one_shot_tick = SCH_GetTick();
}

static void self_cancel_task(void)
{
    ++periodic_runs;
    assert(SCH_Delete_Task(self_id) == 0U);
}

static void reset_test(void)
{
    SCH_Init();
    periodic_runs = 0U;
    one_shot_runs = 0U;
    last_periodic_tick = 0U;
    last_one_shot_tick = 0U;
}

static void advance(unsigned ticks)
{
    for (unsigned i = 0U; i < ticks; ++i) {
        SCH_Update();
        SCH_Dispatch_Tasks();
    }
}

static void test_same_deadline(void)
{
    reset_test();

    uint32_t periodic_id = SCH_Add_Task(periodic_task, 3U, 1U);
    uint32_t one_shot_id = SCH_Add_Task(one_shot_task, 3U, 0U);
    assert(periodic_id != SCH_INVALID_TASK_ID);
    assert(one_shot_id != SCH_INVALID_TASK_ID);
    assert(periodic_id != one_shot_id);

    advance(2U);
    assert(periodic_runs == 0U && one_shot_runs == 0U);

    SCH_Update();
    assert(SCH_GetTick() == 3U);
    assert(periodic_runs == 0U && one_shot_runs == 0U);

    SCH_Dispatch_Tasks();
    assert(periodic_runs == 1U && one_shot_runs == 1U);
    assert(last_periodic_tick == 3U && last_one_shot_tick == 3U);

    advance(1U);
    assert(periodic_runs == 2U && one_shot_runs == 1U);
    assert(last_periodic_tick == 4U);
}

static void test_delete_middle(void)
{
    reset_test();

    assert(SCH_Add_Task(one_shot_task, 8U, 0U) != SCH_INVALID_TASK_ID);
    assert(SCH_Add_Task(one_shot_task, 3U, 0U) != SCH_INVALID_TASK_ID);
    uint32_t middle_id = SCH_Add_Task(periodic_task, 5U, 0U);
    assert(middle_id != SCH_INVALID_TASK_ID);
    assert(SCH_Delete_Task(middle_id) == 0U);
    assert(SCH_Delete_Task(middle_id) != 0U);

    advance(3U);
    assert(one_shot_runs == 1U && last_one_shot_tick == 3U);

    advance(4U);
    assert(periodic_runs == 0U && one_shot_runs == 1U);

    advance(1U);
    assert(one_shot_runs == 2U && last_one_shot_tick == 8U);
}

static void test_capacity_and_reuse(void)
{
    reset_test();

    assert(SCH_Add_Task(NULL, 0U, 0U) == SCH_INVALID_TASK_ID);
    assert(SCH_Delete_Task(SCH_INVALID_TASK_ID) != 0U);

    uint32_t ids[SCH_MAX_TASKS];

    for (unsigned i = 0U; i < SCH_MAX_TASKS; ++i) {
        ids[i] = SCH_Add_Task(one_shot_task, 1U, 0U);
        assert(ids[i] != SCH_INVALID_TASK_ID);
    }

    assert(SCH_Add_Task(one_shot_task, 1U, 0U) == SCH_INVALID_TASK_ID);
    assert(SCH_Delete_Task(ids[0]) == 0U);
    assert(SCH_Add_Task(one_shot_task, 1U, 0U) != SCH_INVALID_TASK_ID);

    advance(1U);
    assert(one_shot_runs == SCH_MAX_TASKS);
    assert(SCH_Add_Task(one_shot_task, 1U, 0U) != SCH_INVALID_TASK_ID);
}

static void test_self_cancel(void)
{
    reset_test();

    self_id = SCH_Add_Task(self_cancel_task, 1U, 1U);
    assert(self_id != SCH_INVALID_TASK_ID);

    advance(10U);
    assert(periodic_runs == 1U);
    assert(SCH_Delete_Task(self_id) != 0U);
}

static void test_late_dispatch(void)
{
    reset_test();

    assert(SCH_Add_Task(periodic_task, 3U, 2U) != SCH_INVALID_TASK_ID);

    for (unsigned i = 0U; i < 6U; ++i) {
        SCH_Update();
    }
    assert(periodic_runs == 0U);

    SCH_Dispatch_Tasks();
    assert(periodic_runs == 1U && last_periodic_tick == 6U);

    advance(1U);
    assert(periodic_runs == 2U && last_periodic_tick == 7U);
}

static void test_10000_ticks(void)
{
    reset_test();

    assert(SCH_Add_Task(periodic_task, 2U, 2U) != SCH_INVALID_TASK_ID);
    assert(SCH_Add_Task(one_shot_task, 20U, 20U) != SCH_INVALID_TASK_ID);

    advance(10000U);
    assert(SCH_GetTick() == 10000U);
    assert(periodic_runs == 5000U);
    assert(one_shot_runs == 500U);
}

static void test_overdue_before_far_future(void)
{
    reset_test();

    assert(SCH_Add_Task(one_shot_task, 0U, 0U) != SCH_INVALID_TASK_ID);

    /* Advance without dispatching, so the first task is overdue. */
    SCH_Update();
    assert(one_shot_runs == 0U);

    assert(SCH_Add_Task(periodic_task, (uint32_t)INT32_MAX, 0U)
           != SCH_INVALID_TASK_ID);

    SCH_Dispatch_Tasks();

    assert(one_shot_runs == 1U);
    assert(last_one_shot_tick == 1U);
    assert(periodic_runs == 0U);
}

#define CORE_TASK_COUNT 8U

/* Periods in ticks. One tick is 10 ms. */
static const uint32_t core_periods[CORE_TASK_COUNT] = {
    1U, 1U, 1U, 2U, 2U, 2U, 10U, 20U
};

static uint32_t core_runs[CORE_TASK_COUNT];

static void record_core_run(unsigned index)
{
    ++core_runs[index];

    /* Check every execution, including early, late and duplicate runs. */
    assert(SCH_GetTick() == core_runs[index] * core_periods[index]);
}

static void core_task_0(void) { record_core_run(0U); }
static void core_task_1(void) { record_core_run(1U); }
static void core_task_2(void) { record_core_run(2U); }
static void core_task_3(void) { record_core_run(3U); }
static void core_task_4(void) { record_core_run(4U); }
static void core_task_5(void) { record_core_run(5U); }
static void core_task_6(void) { record_core_run(6U); }
static void core_task_7(void) { record_core_run(7U); }

static void test_eight_task_cadences(void)
{
    void (*const callbacks[CORE_TASK_COUNT])(void) = {
        core_task_0, core_task_1, core_task_2, core_task_3,
        core_task_4, core_task_5, core_task_6, core_task_7
    };

    reset_test();

    for (unsigned i = 0U; i < CORE_TASK_COUNT; ++i) {
        core_runs[i] = 0U;

        uint32_t period = core_periods[i];
        uint32_t id = SCH_Add_Task(callbacks[i], period, period);
        assert(id != SCH_INVALID_TASK_ID);
    }

    advance(1000U);

    for (unsigned i = 0U; i < CORE_TASK_COUNT; ++i) {
        assert(core_runs[i] == 1000U / core_periods[i]);
    }
}

int main(void)
{
    test_same_deadline();
    test_delete_middle();
    test_capacity_and_reuse();
    test_self_cancel();
    test_late_dispatch();
    test_10000_ticks();
    test_overdue_before_far_future();
    test_eight_task_cadences();

    puts("PASS: 8 scheduler tests");
    return 0;
}