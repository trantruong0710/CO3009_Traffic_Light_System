#include "scheduler.h"

#include <stdbool.h>
#include <stddef.h>

#if defined(__arm__) || defined(__thumb__)
#include "stm32f1xx.h"
#endif

/* Fixed-array delta queue, sorted by absolute DueTick. */
static sTask tasks[SCH_MAX_TASKS];
static uint8_t task_count;
static volatile uint32_t sch_tick_epoch;
static uint32_t next_task_id;
static uint32_t running_task_id;
static bool cancel_running_reinsert;

/* Preserve the caller's interrupt mask on Cortex-M3. */
static uint32_t irq_save(void)
{
#if defined(__arm__) || defined(__thumb__)
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
#else
    /* Host unit tests are single-threaded; no real interrupt can preempt. */
    return 0U;
#endif
}

static void irq_restore(uint32_t saved)
{
#if defined(__arm__) || defined(__thumb__)
    __set_PRIMASK(saved);
#else
    (void)saved;
#endif
}

/* Valid when no deadline is more than INT32_MAX ticks from 'now'. */
static bool due(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

/* Stable ascending order, including deadlines across uint32 rollover. */
static bool before_or_equal(uint32_t a, uint32_t b, uint32_t now)
{
    bool a_due = due(now, a);
    bool b_due = due(now, b);

    if (a_due != b_due) {
        return a_due;
    }
    if (a_due) {
        return (uint32_t)(now - a) >= (uint32_t)(now - b);
    }
    return (uint32_t)(a - now) <= (uint32_t)(b - now);
}

/* MAIN context only, with IRQ masked: O(n), n <= 16. */
static void rebase_queue(uint32_t now)
{
    for (uint8_t i = 0U; i < task_count; ++i) {
        if (i == 0U) {
            tasks[i].Delay = due(now, tasks[i].DueTick)
                             ? 0U : tasks[i].DueTick - now;
            tasks[i].RunMe = due(now, tasks[i].DueTick) ? 1U : 0U;
        } else {
            tasks[i].Delay = tasks[i].DueTick - tasks[i - 1U].DueTick;
            tasks[i].RunMe = 0U;
        }
    }
}

static bool id_in_use(uint32_t id)
{
    if (running_task_id == id) {
        return true;
    }
    for (uint8_t i = 0U; i < task_count; ++i) {
        if (tasks[i].TaskID == id) {
            return true;
        }
    }
    return false;
}

/* Called with IRQ masked; at most 16 IDs can be occupied. */
static uint32_t allocate_id(void)
{
    uint32_t id;
    do {
        id = next_task_id++;
    } while (id == SCH_INVALID_TASK_ID || id_in_use(id));
    return id;
}

/* Called with IRQ masked; no new TaskID is allocated here. */
static void insert_task(sTask task, uint32_t now)
{
    uint8_t position = 0U;

    while (position < task_count &&
           before_or_equal(tasks[position].DueTick, task.DueTick, now)) {
        ++position;
    }

    for (uint8_t i = task_count; i > position; --i) {
        tasks[i] = tasks[i - 1U];
    }

    tasks[position] = task;
    ++task_count;
    rebase_queue(now);
}

/* Called with IRQ masked; caller checked the index. */
static sTask pop_at(uint8_t position, uint32_t now)
{
    sTask removed = tasks[position];

    for (uint8_t i = position; i + 1U < task_count; ++i) {
        tasks[i] = tasks[i + 1U];
    }
    --task_count;
    rebase_queue(now);
    return removed;
}

void SCH_Init(void)
{
    uint32_t saved = irq_save();
    task_count = 0U;
    sch_tick_epoch = 0U;
    next_task_id = 1U;
    running_task_id = SCH_INVALID_TASK_ID;
    cancel_running_reinsert = false;
    irq_restore(saved);
}

uint32_t SCH_Add_Task(void (*pFunc)(void), uint32_t delay_ticks,
                      uint32_t period_ticks)
{
    if (pFunc == NULL || delay_ticks > (uint32_t)INT32_MAX ||
        period_ticks > (uint32_t)INT32_MAX) {
        return SCH_INVALID_TASK_ID;
    }

    uint32_t saved = irq_save();
    if ((uint32_t)task_count +
        (running_task_id != SCH_INVALID_TASK_ID ? 1U : 0U)
        >= SCH_MAX_TASKS) {
        irq_restore(saved);
        return SCH_INVALID_TASK_ID;
    }

    uint32_t now = sch_tick_epoch;
    uint32_t id = allocate_id();
    sTask task = {
        .pTask = pFunc,
        .Delay = delay_ticks,
        .Period = period_ticks,
        .RunMe = delay_ticks == 0U ? 1U : 0U,
        .TaskID = id,
        .DueTick = now + delay_ticks
    };
    insert_task(task, now);
    irq_restore(saved);
    return id;
}

/* Baseline v1.2: return 1 if found, 0 otherwise. */
uint8_t SCH_Delete_Task(uint32_t taskID)
{
    if (taskID == SCH_INVALID_TASK_ID) {
        return 0U;
    }

    uint32_t saved = irq_save();
    for (uint8_t i = 0U; i < task_count; ++i) {
        if (tasks[i].TaskID == taskID) {
            (void)pop_at(i, sch_tick_epoch);
            irq_restore(saved);
            return 1U;
        }
    }

    if (running_task_id == taskID) {
        cancel_running_reinsert = true;
        irq_restore(saved);
        return 1U;
    }
    irq_restore(saved);
    return 0U;
}

/* TIM2 ISR: constant time, examines only the head. */
void SCH_Update(void)
{
    ++sch_tick_epoch;
    if (task_count != 0U) {
        if (tasks[0].Delay > 0U) {
            --tasks[0].Delay;
        }
        if (due(sch_tick_epoch, tasks[0].DueTick)) {
            tasks[0].Delay = 0U;
            tasks[0].RunMe = 1U;
        }
    }
}

uint32_t SCH_GetTick(void)
{
    return sch_tick_epoch;
}

void SCH_Dispatch_Tasks(void)
{
    for (uint8_t executed = 0U; executed < SCH_MAX_TASKS; ++executed) {
        uint32_t saved = irq_save();
        uint32_t now = sch_tick_epoch;

        if (task_count == 0U || !due(now, tasks[0].DueTick)) {
            irq_restore(saved);
            break;
        }

        sTask task = pop_at(0U, now);
        running_task_id = task.TaskID;
        cancel_running_reinsert = false;
        irq_restore(saved);

        /* Never invoke application callbacks with IRQ disabled. */
        task.pTask();

        saved = irq_save();
        now = sch_tick_epoch;
        if (task.Period != 0U && !cancel_running_reinsert) {
            task.DueTick = now + task.Period;
            task.Delay = task.Period;
            task.RunMe = 0U;
            /* One slot is reserved by running_task_id. */
            insert_task(task, now);
        }
        running_task_id = SCH_INVALID_TASK_ID;
        cancel_running_reinsert = false;
        irq_restore(saved);
    }
}

#ifdef SCH_TEST
/* Test-only hooks. Never compile firmware with -DSCH_TEST. */
void SCH_Test_SetTick(uint32_t tick)
{
    uint32_t saved = irq_save();
    if (task_count == 0U && running_task_id == SCH_INVALID_TASK_ID) {
        sch_tick_epoch = tick;
    }
    irq_restore(saved);
}

bool SCH_Test_Peek(uint8_t index, sTask *out)
{
    if (out == NULL || index >= task_count) {
        return false;
    }
    *out = tasks[index];
    return true;
}
#endif
