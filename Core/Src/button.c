
#include "button.h"
#include "main.h"


#include <string.h>

#define BTN_COUNT          4U
#define DEBOUNCE_SAMPLES   3U
#define LONG_PRESS_TICKS   100U
#define EVENT_CAPACITY     8U

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
} button_state_t;

static button_state_t states[BTN_COUNT];
static uint32_t scan_ticks = 0U;

static GPIO_TypeDef * const ports[BTN_COUNT] = {
    BTN_1_GPIO_Port, BTN_2_GPIO_Port,
    BTN_3_GPIO_Port, BTN_4_GPIO_Port
};

static const uint16_t pins[BTN_COUNT] = {
    BTN_1_Pin, BTN_2_Pin, BTN_3_Pin, BTN_4_Pin
};

/* PLACEHOLDERS ONLY: verify V-03 before enabling. */
static const GPIO_PinState active_levels[BTN_COUNT] = {
    GPIO_PIN_RESET, GPIO_PIN_RESET,
    GPIO_PIN_RESET, GPIO_PIN_RESET
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

    if (event_count == EVENT_CAPACITY) {
        queue_overflow = true;

        /* Preserve MODE and STOP over auxiliary events. */
        if (!is_critical_event(e)) {
            return;
        }

        uint8_t victim = 0U;

        while (victim < event_count &&
               is_critical_event(events[victim])) {
            victim++;
        }

        if (victim == event_count) {
            victim = 0U; /* Replace oldest if all critical. */
        }

        for (uint8_t i = victim;
             i + 1U < event_count; i++) {
            events[i] = events[i + 1U];
        }

        event_count--;
    }

    events[event_count++] = e;
}

void button_init(void)
{
    memset(states, 0, sizeof(states));
    event_count = 0U;
    queue_overflow = false;
    scan_ticks = 0U;
}

void button_scan_10ms(void)
{
    if (!BUTTON_HW_VERIFIED) {
        return;
    }

    uint32_t now = ++scan_ticks;

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
