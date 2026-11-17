#include "software_timer.h"
#include "scheduler.h"

typedef struct {
    uint32_t remaining;
    uint32_t last_tick;
    bool expired;
} soft_timer_t;

static soft_timer_t timers[TMR_COUNT];

static bool timer_id_valid(soft_timer_id_t id)
{
    return (uint32_t)id < (uint32_t)TMR_COUNT;
}

void swtimer_init(void)
{
    for (uint32_t i = 0U; i < (uint32_t)TMR_COUNT; ++i) {
        timers[i] = (soft_timer_t){0};
    }
}

bool swtimer_start(soft_timer_id_t id, uint32_t ticks)
{
    if (!timer_id_valid(id) || ticks == 0U) {
        return false;
    }

    timers[id] = (soft_timer_t){
        .remaining = ticks,
        .last_tick = SCH_GetTick(),
        .expired = false
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

        if (timer->remaining == 0U) {
            continue;
        }

        uint32_t elapsed = now - timer->last_tick;
        timer->last_tick = now;

        if (elapsed >= timer->remaining) {
            timer->remaining = 0U;
            timer->expired = true;
        } else {
            timer->remaining -= elapsed;
        }
    }
}

bool swtimer_expired(soft_timer_id_t id)
{
    if (!timer_id_valid(id)) {
        return false;
    }

    bool expired = timers[id].expired;
    timers[id].expired = false;
    return expired;
}

uint32_t swtimer_remaining(soft_timer_id_t id)
{
    if (!timer_id_valid(id)) {
        return 0U;
    }

    return timers[id].remaining;
}