
#include <assert.h>
#include <stdio.h>

#include "main.h"
#include "button.h"
#include "global.h"
app_shared_state_t g_app_state;
static uint32_t mock_tick = 0U;

GPIO_TypeDef mock_gpioa = {0};
GPIO_TypeDef mock_gpiob = {1};
GPIO_TypeDef mock_gpioc = {2};

static uint32_t mock_fault_flags = 0U;

void global_raise_fault(uint32_t mask)
{
    mock_fault_flags |= mask;
}

static GPIO_PinState levels[4] = {
    GPIO_PIN_SET, GPIO_PIN_SET,
    GPIO_PIN_SET, GPIO_PIN_SET
};

GPIO_PinState HAL_GPIO_ReadPin(
    GPIO_TypeDef *port,
    uint16_t pin
) {
    if (port == &mock_gpioa && pin == BTN_1_Pin)
        return levels[0];
    if (port == &mock_gpioa && pin == BTN_2_Pin)
        return levels[1];
    if (port == &mock_gpioc && pin == BTN_3_Pin)
        return levels[2];
    if (port == &mock_gpiob && pin == BTN_4_Pin)
        return levels[3];

    return GPIO_PIN_SET;
}

static void scan(unsigned ticks)
{
    while (ticks-- > 0U) {
        mock_tick++;
        button_scan_10ms();
    }
}

static void expect_event(button_id_t id, button_kind_t kind)
{
    button_event_t event;

    assert(button_pop_event(&event));
    assert(event.id == id);
    assert(event.kind == kind);
}

static void expect_empty(void)
{
    button_event_t event;
    assert(!button_pop_event(&event));
}

static void reset_test(void)
{
    mock_fault_flags = 0U;
    mock_tick = 0U;
    for (unsigned i = 0; i < 4; i++) {
        levels[i] = GPIO_PIN_SET;
    }

    button_init();
    scan(3);
}
static void test_short_press(void)
{
    reset_test();

    /* Contact bounce: one brief LOW sample. */
    levels[0] = GPIO_PIN_RESET;
    scan(1);
    levels[0] = GPIO_PIN_SET;
    scan(2);
    expect_empty();

    /* Valid press after three stable samples. */
    levels[0] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_DOWN);

    levels[0] = GPIO_PIN_SET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_SHORT);
    expect_empty();
}

static void test_long_press(void)
{
    reset_test();

    levels[1] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_2, BTN_EVT_DOWN);

    scan(100);
    expect_event(BTN_2, BTN_EVT_LONG);

    levels[1] = GPIO_PIN_SET;
    scan(3);
    expect_empty(); /* No second LONG or SHORT. */
}

static void test_block_held_button(void)
{
    reset_test();

    levels[2] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_3, BTN_EVT_DOWN);

    button_block_held_until_release();
    scan(110);
    expect_empty();

    levels[2] = GPIO_PIN_SET;
    scan(3);
    expect_empty();

    levels[2] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_3, BTN_EVT_DOWN);
}


static void test_simultaneous_buttons(void)
{
    reset_test();

    levels[0] = GPIO_PIN_RESET;
    levels[1] = GPIO_PIN_RESET;
    scan(3);

    expect_event(BTN_1, BTN_EVT_DOWN);
    expect_event(BTN_2, BTN_EVT_DOWN);
    expect_empty();

    /* Holding should not generate repeated DOWN. */
    scan(20);
    expect_empty();

    levels[0] = GPIO_PIN_SET;
    levels[1] = GPIO_PIN_SET;
    scan(3);

    expect_event(BTN_1, BTN_EVT_SHORT);
    expect_event(BTN_2, BTN_EVT_SHORT);
    expect_empty();
}


static void test_long_press_boundary(void)
{
    reset_test();

    levels[0] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_DOWN);

    /* Release at 990 ms: SHORT. */
    scan(96);
    levels[0] = GPIO_PIN_SET;
    scan(3);

    expect_event(BTN_1, BTN_EVT_SHORT);
    expect_empty();

    /* Hold for exactly 100 ticks: LONG. */
    reset_test();

    levels[0] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_DOWN);

    scan(100);
    expect_event(BTN_1, BTN_EVT_LONG);

    levels[0] = GPIO_PIN_SET;
    scan(3);
    expect_empty();
}



static void test_queue_overflow(void)
{
    reset_test();

    /* Four DOWN events. */
    for (unsigned i = 0; i < 4; i++) {
        levels[i] = GPIO_PIN_RESET;
    }
    scan(3);

    /* Four SHORT events: queue now contains 8. */
    for (unsigned i = 0; i < 4; i++) {
        levels[i] = GPIO_PIN_SET;
    }
    scan(3);

    /* Noncritical event should be discarded. */
    levels[3] = GPIO_PIN_RESET;
    scan(3);

    /* Critical BTN3 DOWN must be preserved. */
    levels[2] = GPIO_PIN_RESET;
    scan(3);

    /* Oldest noncritical BTN1 DOWN was evicted. */
    expect_event(BTN_2, BTN_EVT_DOWN);
    expect_event(BTN_3, BTN_EVT_DOWN);
    expect_event(BTN_4, BTN_EVT_DOWN);

    expect_event(BTN_1, BTN_EVT_SHORT);
    expect_event(BTN_2, BTN_EVT_SHORT);
    expect_event(BTN_3, BTN_EVT_SHORT);
    expect_event(BTN_4, BTN_EVT_SHORT);

    expect_event(BTN_3, BTN_EVT_DOWN);
    expect_empty();
    assert((mock_fault_flags & (1UL << 1)) != 0U);
}



/* TEST 7: Idle must not generate events. */
static void test_idle(void)
{
    reset_test();
    scan(1000);
    expect_empty();
}

/* TEST 8: Bounce during release must not trigger early SHORT. */
static void test_release_bounce(void)
{
    reset_test();

    levels[0] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_DOWN);

    levels[0] = GPIO_PIN_SET;
    scan(2);
    expect_empty();

    levels[0] = GPIO_PIN_RESET;
    scan(1);
    expect_empty();

    levels[0] = GPIO_PIN_SET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_SHORT);

    scan(20);
    expect_empty();
}

/* TEST 9: All four buttons may generate LONG independently. */
static void test_all_buttons_long(void)
{
    reset_test();

    for (unsigned i = 0; i < 4; i++)
        levels[i] = GPIO_PIN_RESET;

    scan(3);

    for (unsigned i = 0; i < 4; i++)
        expect_event((button_id_t)i, BTN_EVT_DOWN);

    scan(100);

    for (unsigned i = 0; i < 4; i++)
        expect_event((button_id_t)i, BTN_EVT_LONG);

    scan(100);
    expect_empty();

    for (unsigned i = 0; i < 4; i++)
        levels[i] = GPIO_PIN_SET;

    scan(3);
    expect_empty();
}

/* TEST 10: Block a button even before debounce completes. */
static void test_block_before_debounce(void)
{
    reset_test();

    levels[3] = GPIO_PIN_RESET;
    scan(1);

    button_block_held_until_release();
    scan(120);
    expect_empty();

    levels[3] = GPIO_PIN_SET;
    scan(3);
    expect_empty();

    levels[3] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_4, BTN_EVT_DOWN);

    levels[3] = GPIO_PIN_SET;
    scan(3);
    expect_event(BTN_4, BTN_EVT_SHORT);
    expect_empty();
}

/* TEST 11: Clear queue and reject NULL without losing events. */
static void test_clear_and_null_pop(void)
{
    reset_test();

    levels[3] = GPIO_PIN_RESET;
    scan(3);

    assert(!button_pop_event(NULL));

    button_clear_events();
    expect_empty();

    levels[3] = GPIO_PIN_SET;
    scan(3);

    expect_event(BTN_4, BTN_EVT_SHORT);
    expect_empty();
}

/* TEST 12: Full queue containing only critical events. */
static void test_full_critical_queue(void)
{
    reset_test();

    for (unsigned cycle = 0; cycle < 9; cycle++) {
        levels[0] = GPIO_PIN_RESET;
        scan(3);

        levels[0] = GPIO_PIN_SET;
        scan(3);
    }

    /* Queue retains its eight most recent critical SHORTs. */
    for (unsigned i = 0; i < 8; i++)
        expect_event(BTN_1, BTN_EVT_SHORT);

    expect_empty();
}

/* TEST 13: Stress test with 200 consecutive press cycles. */
static void test_repeated_presses(void)
{
    reset_test();

    for (unsigned cycle = 0; cycle < 200; cycle++) {
        unsigned id = cycle % 4U;

        levels[id] = GPIO_PIN_RESET;
        scan(3);
        expect_event((button_id_t)id, BTN_EVT_DOWN);

        scan(2);
        expect_empty();

        levels[id] = GPIO_PIN_SET;
        scan(3);
        expect_event((button_id_t)id, BTN_EVT_SHORT);

        expect_empty();
    }
}



uint32_t SCH_GetTick(void)
{
    return mock_tick;
}


/* TEST 14: LONG must use real elapsed ticks, not scan count. */
static void test_delayed_dispatch(void)
{
    reset_test();

    levels[1] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_2, BTN_EVT_DOWN);

    /* Simulate 100 system ticks without button scanning. */
    mock_tick += 100U;

    /* One late scan must detect the LONG press. */
    button_scan_10ms();
    expect_event(BTN_2, BTN_EVT_LONG);
    expect_empty();

    /* No repeated LONG or SHORT after release. */
    levels[1] = GPIO_PIN_SET;
    scan(3);
    expect_empty();
}


/* TEST 15: LONG timing survives uint32_t tick wrap-around. */
static void test_tick_wraparound(void)
{
    reset_test();

    mock_tick = UINT32_MAX - 5U;

    levels[0] = GPIO_PIN_RESET;
    scan(3);
    expect_event(BTN_1, BTN_EVT_DOWN);

    /* Advance 100 ticks across UINT32_MAX -> 0. */
    scan(100);
    expect_event(BTN_1, BTN_EVT_LONG);

    levels[0] = GPIO_PIN_SET;
    scan(3);

    /* Releasing after LONG must not generate SHORT. */
    expect_empty();
}
/* TEST 16: MODE must survive repeated STOP queue overflow. */
static void test_mode_priority_on_overflow(void)
{
    reset_test();

    /* Fill FIFO with critical STOP events. */
    for (unsigned i = 0U; i < 9U; i++) {
        levels[2] = GPIO_PIN_RESET;
        scan(3);
        levels[2] = GPIO_PIN_SET;
        scan(3);
    }

    /* Add one MODE event. */
    levels[0] = GPIO_PIN_RESET;
    scan(3);
    levels[0] = GPIO_PIN_SET;
    scan(3);

    /* Generate more STOP events to stress FIFO eviction. */
    for (unsigned i = 0U; i < 8U; i++) {
        levels[2] = GPIO_PIN_RESET;
        scan(3);
        levels[2] = GPIO_PIN_SET;
        scan(3);
    }

    unsigned mode_count = 0U;
    unsigned stop_count = 0U;
    button_event_t event;

    while (button_pop_event(&event)) {
        if (event.id == BTN_1 &&
            event.kind == BTN_EVT_SHORT) {
            mode_count++;
        }

        if (event.id == BTN_3 &&
            event.kind == BTN_EVT_DOWN) {
            stop_count++;
        }
    }

    assert(mode_count >= 1U);
    assert(stop_count >= 1U);
}

static void test_tuning_repeat(void)
{
    reset_test();
    g_app_state.mode = MODE_TUNING;
    levels[2] = GPIO_PIN_RESET;
    scan(3); expect_event(BTN_3, BTN_EVT_DOWN);
    scan(99); expect_empty();
    scan(1); expect_event(BTN_3, BTN_EVT_LONG); expect_event(BTN_3, BTN_EVT_REPEAT);
    scan(19); expect_empty();
    scan(1); expect_event(BTN_3, BTN_EVT_REPEAT);
    levels[2] = GPIO_PIN_SET;
    scan(25); expect_empty();
    g_app_state.mode = MODE_AUTO;
}

int main(void)
{
    test_tuning_repeat();
    test_short_press();
    test_long_press();
    test_block_held_button();
    test_simultaneous_buttons();
    test_long_press_boundary();
    test_queue_overflow();

    test_idle();
    test_release_bounce();
    test_all_buttons_long();
    test_block_before_debounce();
    test_clear_and_null_pop();
    test_full_critical_queue();
    test_repeated_presses();
    test_delayed_dispatch();
    test_tick_wraparound();
    test_mode_priority_on_overflow();
    puts("PASS: 17/17 Button unit tests (including tuning hold repeat)");
    return 0;
}
