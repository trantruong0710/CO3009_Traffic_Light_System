#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "scheduler.h"

/* Private test hooks built only with -DSCH_TEST. */
extern void SCH_Test_SetTick(uint32_t tick);
extern bool SCH_Test_Peek(uint8_t index, sTask *out);

static unsigned periodic_runs, one_shot_runs;
static uint32_t last_periodic_tick, last_one_shot_tick, self_id;
static unsigned order[32], order_count;
static uint32_t slow_start_tick;

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
    assert(SCH_Delete_Task(self_id) == 1U);
    assert(SCH_Delete_Task(self_id) == 1U);
}

static void record_one(void) { order[order_count++] = 1U; }
static void record_two(void) { order[order_count++] = 2U; }
static void record_three(void) { order[order_count++] = 3U; }

static void slow_task(void)
{
    slow_start_tick = SCH_GetTick();
    for (unsigned i = 0U; i < 6U; ++i) {
        SCH_Update(); /* Simulate TIM2 interrupts during a slow callback. */
    }
    record_one();
}

static void reset_test(void)
{
    SCH_Init();
    periodic_runs = 0U;
    one_shot_runs = 0U;
    last_periodic_tick = 0U;
    last_one_shot_tick = 0U;
    self_id = 0U;
    order_count = 0U;
    slow_start_tick = 0U;
}

static void tick_only(unsigned ticks)
{
    for (unsigned i = 0U; i < ticks; ++i) {
        SCH_Update();
    }
}

static void advance(unsigned ticks)
{
    for (unsigned i = 0U; i < ticks; ++i) {
        SCH_Update();
        SCH_Dispatch_Tasks();
    }
}

static void test_delta_queue_and_runme(void)
{
    reset_test();
    uint32_t id8 = SCH_Add_Task(record_three, 8U, 0U);
    uint32_t id3 = SCH_Add_Task(record_one, 3U, 0U);
    uint32_t id5 = SCH_Add_Task(record_two, 5U, 0U);
    sTask t;
    assert(id8 && id3 && id5);
    assert(SCH_Test_Peek(0U, &t) && t.TaskID == id3 && t.Delay == 3U && t.RunMe == 0U);
    assert(SCH_Test_Peek(1U, &t) && t.TaskID == id5 && t.Delay == 2U && t.RunMe == 0U);
    assert(SCH_Test_Peek(2U, &t) && t.TaskID == id8 && t.Delay == 3U && t.RunMe == 0U);
    tick_only(2U);
    assert(SCH_Test_Peek(0U, &t) && t.Delay == 1U && t.RunMe == 0U);
    SCH_Update();
    assert(SCH_Test_Peek(0U, &t) && t.Delay == 0U && t.RunMe == 1U);
    assert(order_count == 0U); /* IRQ never runs callbacks. */
    SCH_Dispatch_Tasks();
    assert(order_count == 1U && order[0] == 1U);
    assert(SCH_Test_Peek(0U, &t) && t.TaskID == id5 && t.Delay == 2U);
    advance(5U);
    assert(order_count == 3U && order[1] == 2U && order[2] == 3U);
}

static void test_equal_deadline_fifo_and_zero_delay(void)
{
    reset_test();
    assert(SCH_Add_Task(record_one, 0U, 0U) != 0U);
    assert(SCH_Add_Task(record_two, 0U, 0U) != 0U);
    assert(SCH_Add_Task(record_three, 0U, 0U) != 0U);
    SCH_Dispatch_Tasks(); /* No preceding tick required. */
    assert(order_count == 3U);
    assert(order[0] == 1U && order[1] == 2U && order[2] == 3U);
}

static void test_delete_head_middle_tail(void)
{
    reset_test();
    uint32_t id3 = SCH_Add_Task(record_one, 3U, 0U);
    uint32_t id5 = SCH_Add_Task(record_two, 5U, 0U);
    uint32_t id8 = SCH_Add_Task(record_three, 8U, 0U);
    assert(id3 && id5 && id8);
    assert(SCH_Delete_Task(id5) == 1U);
    assert(SCH_Delete_Task(id5) == 0U);
    advance(8U);
    assert(order_count == 2U && order[0] == 1U && order[1] == 3U);

    reset_test();
    id3 = SCH_Add_Task(record_one, 3U, 0U);
    id5 = SCH_Add_Task(record_two, 5U, 0U);
    id8 = SCH_Add_Task(record_three, 8U, 0U);
    assert(SCH_Delete_Task(id3) == 1U); /* head */
    assert(SCH_Delete_Task(id8) == 1U); /* tail */
    advance(8U);
    assert(order_count == 1U && order[0] == 2U);
    assert(SCH_Delete_Task(0U) == 0U);
}

static void test_periodic_one_shot_and_stable_id(void)
{
    reset_test();
    uint32_t id1 = SCH_Add_Task(periodic_task, 3U, 2U);
    uint32_t id2 = SCH_Add_Task(one_shot_task, 3U, 0U);
    assert(id1 != 0U && id2 != 0U && id1 != id2);
    advance(3U);
    assert(periodic_runs == 1U && one_shot_runs == 1U);
    assert(last_periodic_tick == 3U && last_one_shot_tick == 3U);
    sTask t;
    assert(SCH_Test_Peek(0U, &t) && t.TaskID == id1);
    advance(2U);
    assert(periodic_runs == 2U && one_shot_runs == 1U);
    assert(last_periodic_tick == 5U);
    assert(SCH_Delete_Task(id1) == 1U);
    advance(5U);
    assert(periodic_runs == 2U);
}

static void test_capacity_and_reuse(void)
{
    reset_test();
    uint32_t ids[SCH_MAX_TASKS];
    assert(SCH_Add_Task(NULL, 0U, 0U) == 0U);
    assert(SCH_Add_Task(record_one, (uint32_t)INT32_MAX + 1U, 0U) == 0U);
    assert(SCH_Add_Task(record_one, 0U, (uint32_t)INT32_MAX + 1U) == 0U);
    for (unsigned i = 0U; i < SCH_MAX_TASKS; ++i) {
        ids[i] = SCH_Add_Task(one_shot_task, 1U, 0U);
        assert(ids[i] != 0U);
    }
    assert(SCH_Add_Task(one_shot_task, 1U, 0U) == 0U);
    assert(SCH_Delete_Task(ids[0]) == 1U);
    uint32_t replacement = SCH_Add_Task(one_shot_task, 1U, 0U);
    assert(replacement != 0U && replacement != ids[0]);
    advance(1U);
    assert(one_shot_runs == SCH_MAX_TASKS);
    assert(SCH_Add_Task(one_shot_task, 1U, 0U) != 0U);
}

static void test_self_cancel(void)
{
    reset_test();
    self_id = SCH_Add_Task(self_cancel_task, 1U, 1U);
    assert(self_id != 0U);
    advance(10U);
    assert(periodic_runs == 1U);
    assert(SCH_Delete_Task(self_id) == 0U);
}

static void test_late_dispatch_no_replay(void)
{
    reset_test();
    assert(SCH_Add_Task(periodic_task, 3U, 2U) != 0U);
    tick_only(6U);
    assert(periodic_runs == 0U);
    SCH_Dispatch_Tasks();
    assert(periodic_runs == 1U && last_periodic_tick == 6U);
    advance(1U);
    assert(periodic_runs == 1U); /* No catch-up at tick 7. */
    advance(1U);
    assert(periodic_runs == 2U && last_periodic_tick == 8U);
}

static void test_multiple_overdue_and_interrupt_during_callback(void)
{
    reset_test();
    assert(SCH_Add_Task(record_one, 10U, 0U) != 0U);
    assert(SCH_Add_Task(record_two, 12U, 0U) != 0U);
    tick_only(16U);
    SCH_Dispatch_Tasks();
    assert(order_count == 2U && order[0] == 1U && order[1] == 2U);

    reset_test();
    assert(SCH_Add_Task(slow_task, 10U, 0U) != 0U);
    assert(SCH_Add_Task(record_two, 12U, 0U) != 0U);
    tick_only(10U);
    SCH_Dispatch_Tasks();
    assert(slow_start_tick == 10U && SCH_GetTick() == 16U);
    assert(order_count == 2U && order[0] == 1U && order[1] == 2U);
}

static void test_wrap_around(void)
{
    reset_test();
    SCH_Test_SetTick(UINT32_MAX - 2U);
    assert(SCH_Add_Task(record_one, 1U, 0U) != 0U);
    assert(SCH_Add_Task(record_two, 5U, 0U) != 0U);
    advance(1U);
    assert(order_count == 1U && order[0] == 1U);
    advance(4U);
    assert(SCH_GetTick() == 2U);
    assert(order_count == 2U && order[1] == 2U);
}

#define CORE_TASK_COUNT 8U
static const uint32_t core_periods[CORE_TASK_COUNT] =
    {1U, 1U, 1U, 2U, 2U, 2U, 10U, 20U};
static uint32_t core_runs[CORE_TASK_COUNT];
static void record_core_run(unsigned i)
{
    ++core_runs[i];
    assert(SCH_GetTick() == core_runs[i] * core_periods[i]);
}
static void core_0(void) { record_core_run(0U); }
static void core_1(void) { record_core_run(1U); }
static void core_2(void) { record_core_run(2U); }
static void core_3(void) { record_core_run(3U); }
static void core_4(void) { record_core_run(4U); }
static void core_5(void) { record_core_run(5U); }
static void core_6(void) { record_core_run(6U); }
static void core_7(void) { record_core_run(7U); }

static void test_eight_task_cadences(void)
{
    void (*const callbacks[CORE_TASK_COUNT])(void) =
        {core_0, core_1, core_2, core_3, core_4, core_5, core_6, core_7};
    reset_test();
    for (unsigned i = 0U; i < CORE_TASK_COUNT; ++i) {
        core_runs[i] = 0U;
        assert(SCH_Add_Task(callbacks[i], core_periods[i], core_periods[i]) != 0U);
    }
    advance(1000U);
    for (unsigned i = 0U; i < CORE_TASK_COUNT; ++i) {
        assert(core_runs[i] == 1000U / core_periods[i]);
    }
}

static void test_ten_thousand_ticks(void)
{
    reset_test();
    assert(SCH_Add_Task(periodic_task, 2U, 2U) != 0U);
    assert(SCH_Add_Task(one_shot_task, 20U, 20U) != 0U);
    advance(10000U);
    assert(SCH_GetTick() == 10000U);
    assert(periodic_runs == 5000U && one_shot_runs == 500U);
}

int main(void)
{
    test_delta_queue_and_runme();
    test_equal_deadline_fifo_and_zero_delay();
    test_delete_head_middle_tail();
    test_periodic_one_shot_and_stable_id();
    test_capacity_and_reuse();
    test_self_cancel();
    test_late_dispatch_no_replay();
    test_multiple_overdue_and_interrupt_during_callback();
    test_wrap_around();
    test_eight_task_cadences();
    test_ten_thousand_ticks();
    puts("PASS: 11 scheduler test groups");
    return 0;
}
