#include "software_timer.h"
#include "scheduler.h"

typedef struct {
    bool running;
    uint32_t due_tick;
    bool expired_latch;
} soft_timer_t;

static soft_timer_t timers[TMR_COUNT];

static bool timer_id_valid(soft_timer_id_t id)
{
    return (uint32_t)id < (uint32_t)TMR_COUNT;
}

/* Valid for deadlines no more than INT32_MAX ticks from now. */
static bool tick_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

void swtimer_init(void)
{
    for (uint32_t i = 0U; i < (uint32_t)TMR_COUNT; ++i) {
        timers[i] = (soft_timer_t){0};
    }
}

bool swtimer_start(soft_timer_id_t id, uint32_t ticks)
{
    if (!timer_id_valid(id) || ticks == 0U ||
        ticks > (uint32_t)INT32_MAX) {
        return false;
    }

    timers[id] = (soft_timer_t){
        .running = true,
        .due_tick = SCH_GetTick() + ticks,
        .expired_latch = false
    };
    return true;
}

void swtimer_stop(soft_timer_id_t id)
{
    if (timer_id_valid(id)) {
        timers[id] = (soft_timer_t){0};
    }
}

void swtimer_tick_10ms(void)
{
    uint32_t now = SCH_GetTick();

    for (uint32_t i = 0U; i < (uint32_t)TMR_COUNT; ++i) {
        soft_timer_t *timer = &timers[i];

        if (timer->running && tick_reached(now, timer->due_tick)) {
            timer->running = false;
            timer->expired_latch = true;
        }
    }
}

bool swtimer_expired(soft_timer_id_t id)
{
    if (!timer_id_valid(id)) {
        return false;
    }

    bool result = timers[id].expired_latch;
    timers[id].expired_latch = false;
    return result;
}

uint32_t swtimer_remaining(soft_timer_id_t id)
{
    if (!timer_id_valid(id) || !timers[id].running) {
        return 0U;
    }

    uint32_t now = SCH_GetTick();
    if (tick_reached(now, timers[id].due_tick)) {
        return 0U;
    }

    return timers[id].due_tick - now;
}
