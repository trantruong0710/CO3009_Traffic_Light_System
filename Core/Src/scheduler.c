#include "scheduler.h"

#include <stddef.h>

#define NO_SLOT UINT8_MAX

typedef enum {
    SLOT_FREE = 0,
    SLOT_WAITING,
    SLOT_RUNNING,
    SLOT_CANCELLED
} slot_state_t;

typedef struct {
    sTask task;
    uint8_t next;
    slot_state_t state;
} task_slot_t;

static task_slot_t slots[SCH_MAX_TASKS];
static uint8_t head = NO_SLOT;
static uint32_t next_task_id = 1U;
static volatile uint32_t current_tick;

/*
 * Tick comparisons assume the two times are separated by no more
 * than INT32_MAX ticks in the relevant direction.
 */
static int tick_reached(uint32_t now, uint32_t due)
{
    return (now - due) <= (uint32_t)INT32_MAX;
}

static int id_in_use(uint32_t id)
{
    for (uint8_t i = 0U; i < SCH_MAX_TASKS; ++i) {
        if (slots[i].state != SLOT_FREE && slots[i].task.TaskID == id) {
            return 1;
        }
    }
    return 0;
}

static uint32_t allocate_task_id(void)
{
    uint32_t id;

    do {
        id = next_task_id++;
    } while (id == SCH_INVALID_TASK_ID || id_in_use(id));

    return id;
}

static int deadline_at_or_before(uint32_t left, uint32_t right, uint32_t now)
{
    int left_due = tick_reached(now, left);
    int right_due = tick_reached(now, right);

    if (left_due != right_due) {
        return left_due;
    }

    if (left_due) {
        return (now - left) >= (now - right);
    }

    return (left - now) <= (right - now);
}

/* Insert after tasks that already have the same deadline. */
static void insert_waiting(uint8_t index)
{
    uint8_t *link = &head;
    uint32_t due = slots[index].task.DueTick;
    uint32_t now = SCH_GetTick();

    while (*link != NO_SLOT &&
           deadline_at_or_before(slots[*link].task.DueTick, due, now)) {
        link = &slots[*link].next;
    }

    slots[index].next = *link;
    slots[index].state = SLOT_WAITING;
    *link = index;
}

void SCH_Init(void)
{
    for (uint8_t i = 0U; i < SCH_MAX_TASKS; ++i) {
        slots[i].state = SLOT_FREE;
    }

    head = NO_SLOT;
    next_task_id = 1U;
    current_tick = 0U;
}

uint32_t SCH_Add_Task(void (*pFunc)(void), uint32_t delay_ticks,
                      uint32_t period_ticks)
{
    if (pFunc == NULL ||
        delay_ticks > (uint32_t)INT32_MAX ||
        period_ticks > (uint32_t)INT32_MAX) {
        return SCH_INVALID_TASK_ID;
    }

    for (uint8_t i = 0U; i < SCH_MAX_TASKS; ++i) {
        if (slots[i].state == SLOT_FREE) {
            uint32_t id = allocate_task_id();

            slots[i].task = (sTask){
                .pTask = pFunc,
                .Period = period_ticks,
                .TaskID = id,
                .DueTick = SCH_GetTick() + delay_ticks
            };
            insert_waiting(i);
            return id;
        }
    }

    return SCH_INVALID_TASK_ID;
}

uint8_t SCH_Delete_Task(uint32_t taskID)
{
    if (taskID == SCH_INVALID_TASK_ID) {
        return 1U;
    }

    uint8_t *link = &head;

    while (*link != NO_SLOT) {
        uint8_t index = *link;

        if (slots[index].task.TaskID == taskID) {
            *link = slots[index].next;
            slots[index].state = SLOT_FREE;
            return 0U;
        }

        link = &slots[index].next;
    }

    for (uint8_t i = 0U; i < SCH_MAX_TASKS; ++i) {
        if (slots[i].state == SLOT_RUNNING && slots[i].task.TaskID == taskID) {
            slots[i].state = SLOT_CANCELLED;
            return 0U;
        }
    }

    return 1U;
}

/* TIM2 ISR calls only this function. */
void SCH_Update(void)
{
    ++current_tick;
}

uint32_t SCH_GetTick(void)
{
    return current_tick;
}

void SCH_Dispatch_Tasks(void)
{
    for (uint8_t executed = 0U; executed < SCH_MAX_TASKS; ++executed) {
        uint8_t index = head;

        if (index == NO_SLOT ||
            !tick_reached(SCH_GetTick(), slots[index].task.DueTick)) {
            break;
        }

        task_slot_t *slot = &slots[index];
        head = slot->next;
        slot->state = SLOT_RUNNING;
        slot->task.pTask();

        uint32_t period = slot->task.Period;
        if (slot->state == SLOT_CANCELLED || period == 0U) {
            slot->state = SLOT_FREE;
        } else {
            uint32_t now = SCH_GetTick();
            uint32_t elapsed = now - slot->task.DueTick;

            /* Keep the original cadence and skip missed releases. */
            slot->task.DueTick = now + (period - elapsed % period);
            insert_waiting(index);
        }
    }
}