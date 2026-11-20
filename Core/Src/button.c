#include "board_config.h"

#include "button.h"
#include "main.h"
#include "scheduler.h"
#include "global.h"


#include <string.h>

#define BTN_COUNT          4U
#define DEBOUNCE_SAMPLES   3U
#define LONG_PRESS_TICKS   100U
#define REPEAT_INTERVAL_TICKS 20U /* 200 ms after the initial 1 s hold. */
#define EVENT_CAPACITY     8U
/* Provisional fault bit; TV1 must confirm before merge. */
#define TV2_FAULT_BUTTON_QUEUE_FULL (1UL << 1)

/*
 * Hardware verification V-03 is pending.
 * Change to 1 only after confirming button polarity
 * and pull-up/pull-down on the actual board.
 */
#ifndef BUTTON_HW_VERIFIED
#define BUTTON_HW_VERIFIED 0U
#endif

typedef struct {
    bool last_raw;
    bool stable_pressed;
    bool long_sent;
    bool blocked;
    uint8_t consecutive;
    uint32_t press_tick;
    uint32_t repeat_tick;
} button_state_t;

static button_state_t states[BTN_COUNT];

static GPIO_TypeDef * const ports[BTN_COUNT] = {
    BTN_1_GPIO_Port, BTN_2_GPIO_Port,
    BTN_3_GPIO_Port, BTN_4_GPIO_Port
};

static const uint16_t pins[BTN_COUNT] = {
    BTN_1_Pin, BTN_2_Pin, BTN_3_Pin, BTN_4_Pin
};

/* Measured levels are configured centrally in board_config.h. */
static const GPIO_PinState active_levels[BTN_COUNT] = {
    BTN1_ACTIVE_LEVEL, BTN2_ACTIVE_LEVEL,
    BTN3_ACTIVE_LEVEL, BTN4_ACTIVE_LEVEL
};

static button_event_t events[EVENT_CAPACITY];
static uint8_t event_count = 0U;

/* Internal diagnostic; integrate fault flag with TV1. */
static bool queue_overflow = false;

static bool read_pressed(uint8_t id)
{
    return HAL_GPIO_ReadPin(ports[id], pins[id])
           == active_levels[id];
}

static bool is_critical_event(button_event_t e)
{
    return (e.id == BTN_1 && e.kind == BTN_EVT_SHORT)
        || (e.id == BTN_3 && e.kind == BTN_EVT_DOWN);
}

static void push_event(button_id_t id, button_kind_t kind)
{
    button_event_t e = { id, kind };

    if (event_count < EVENT_CAPACITY) {
        events[event_count++] = e;
        return;
    }

    queue_overflow = true;
    global_raise_fault(TV2_FAULT_BUTTON_QUEUE_FULL);

    /* Drop noncritical incoming events when FIFO is full. */
    if (!is_critical_event(e)) {
        return;
    }

    uint8_t victim = EVENT_CAPACITY;

    /* Priority 1: Evict the oldest noncritical event. */
    for (uint8_t i = 0U; i < event_count; i++) {
        if (!is_critical_event(events[i])) {
            victim = i;
            break;
        }
    }

    /*
     * Priority 2: All queued events are critical.
     * Evict an older duplicate, preserving the last
     * MODE and STOP event.
     */
    if (victim == EVENT_CAPACITY) {
        for (uint8_t i = 0U; i < event_count; i++) {
            for (uint8_t j = i + 1U; j < event_count; j++) {
                if (events[i].id == events[j].id &&
                    events[i].kind == events[j].kind) {
                    victim = i;
                    break;
                }
            }

            if (victim != EVENT_CAPACITY) {
                break;
            }
        }
    }

    /* No event can be safely replaced. */
    if (victim == EVENT_CAPACITY) {
        return;
    }

    /* Remove selected event while preserving FIFO order. */
    for (uint8_t i = victim; i + 1U < event_count; i++) {
        events[i] = events[i + 1U];
    }

    event_count--;

    events[event_count++] = e;
}

void button_init(void)
{
    memset(states, 0, sizeof(states));
    event_count = 0U;
    queue_overflow = false;
}

void button_scan_10ms(void)
{
    if (!BUTTON_HW_VERIFIED) {
        return;
    }

    uint32_t now = SCH_GetTick();

    for (uint8_t i = 0U; i < BTN_COUNT; i++) {
        button_state_t *b = &states[i];
        bool raw = read_pressed(i);

        if (raw == b->last_raw) {
            if (b->consecutive < DEBOUNCE_SAMPLES) {
                b->consecutive++;
            }
        } else {
            b->last_raw = raw;
            b->consecutive = 1U;
        }

        if (b->blocked) {
            if (!raw &&
                b->consecutive >= DEBOUNCE_SAMPLES) {
                b->blocked = false;
                b->stable_pressed = false;
                b->long_sent = false;
            }
            continue;
        }

        if (b->consecutive >= DEBOUNCE_SAMPLES &&
            raw != b->stable_pressed) {

            b->stable_pressed = raw;

            if (raw) {
                b->press_tick = now;
                b->repeat_tick = now;
                b->long_sent = false;

                push_event((button_id_t)i, BTN_EVT_DOWN);
            } else if (!b->long_sent) {
                uint32_t elapsed = now - b->press_tick;

                if (elapsed >= LONG_PRESS_TICKS) {
                    push_event((button_id_t)i, BTN_EVT_LONG);
                } else {
                    push_event((button_id_t)i, BTN_EVT_SHORT);
                }

                b->long_sent = true;
            }
        }

        if (b->stable_pressed && !b->long_sent &&
            (uint32_t)(now - b->press_tick)
                >= LONG_PRESS_TICKS) {

            push_event((button_id_t)i, BTN_EVT_LONG);
            b->long_sent = true;
            b->repeat_tick = now;
            if (g_app_state.mode == MODE_TUNING && (i == BTN_3 || i == BTN_4)) {
                push_event((button_id_t)i, BTN_EVT_REPEAT);
            }
        } else if (b->stable_pressed && raw && b->long_sent &&
                   g_app_state.mode == MODE_TUNING && (i == BTN_3 || i == BTN_4) &&
                   (uint32_t)(now - b->repeat_tick) >= REPEAT_INTERVAL_TICKS) {
            b->repeat_tick = now;
            push_event((button_id_t)i, BTN_EVT_REPEAT);
        }
    }
}

bool button_pop_event(button_event_t *out_event)
{
    if (out_event == NULL || event_count == 0U) {
        return false;
    }

    *out_event = events[0];

    for (uint8_t i = 1U; i < event_count; i++) {
        events[i - 1U] = events[i];
    }

    event_count--;
    return true;
}

void button_clear_events(void)
{
    event_count = 0U;
}

void button_block_held_until_release(void)
{
    for (uint8_t i = 0U; i < BTN_COUNT; i++) {
        if (states[i].stable_pressed || read_pressed(i)) {
            states[i].blocked = true;
        }
    }
}
