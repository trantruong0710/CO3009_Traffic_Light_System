
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "app_types.h"
#include "button.h"
#include "fsm_automatic.h"
#include "fsm_manual.h"
#include "global.h"
#include "mode_manager.h"
#include "software_timer.h"
#include "traffic_light.h"

GPIO_TypeDef mock_gpioa = {0};
GPIO_TypeDef mock_gpiob = {1};
GPIO_TypeDef mock_gpioc = {2};

static GPIO_PinState levels[4];
static uint16_t gpio_output;

typedef struct {
    uint32_t remaining;
    bool running;
    bool expired;
} test_timer_t;

static test_timer_t timers[TMR_COUNT];

/* ---------- Mock GPIO input ---------- */

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port,
                               uint16_t pin)
{
    if (port == &mock_gpioa && pin == BTN_1_Pin)
        return levels[0];
    if (port == &mock_gpioa && pin == BTN_2_Pin)
        return levels[1];
    if (port == &mock_gpioc && pin == BTN_3_Pin)
        return levels[2];
    if (port == &mock_gpiob && pin == BTN_4_Pin)
        return levels[3];

    assert(0 && "Unknown GPIO input");
    return GPIO_PIN_SET;
}

/* ---------- Mock physical traffic outputs ---------- */

static uint8_t physical_code(uint16_t a, uint16_t b)
{
    return (uint8_t)(
        ((gpio_output & a) ? 2U : 0U) |
        ((gpio_output & b) ? 1U : 0U));
}

static void assert_safe(void)
{
    uint8_t c1 = physical_code(TL1_A_Pin, TL1_B_Pin);
    uint8_t c2 = physical_code(TL2_A_Pin, TL2_B_Pin);

    assert(!(c1 == TL1_GREEN_CODE &&
             c2 == TL2_GREEN_CODE));
}

void traffic_test_bsrr(uint32_t value)
{
    uint16_t set = (uint16_t)value;
    uint16_t reset = (uint16_t)(value >> 16U);

    assert((set & reset) == 0U);

    gpio_output |= set;
    gpio_output &= (uint16_t)~reset;

    /* Verify safety after every simulated GPIO write. */
    assert_safe();
}

/* ---------- Mock software timer ---------- */

bool swtimer_start(soft_timer_id_t id, uint32_t ticks)
{
    if (id >= TMR_COUNT || ticks == 0U)
        return false;

    timers[id] = (test_timer_t){ticks, true, false};
    return true;
}

void swtimer_stop(soft_timer_id_t id)
{
    if (id < TMR_COUNT)
        timers[id] = (test_timer_t){0};
}

bool swtimer_expired(soft_timer_id_t id)
{
    if (id >= TMR_COUNT || !timers[id].expired)
        return false;

    timers[id].expired = false;
    return true;
}

/*
 * Integration stub:
 * Forward Button events directly to Manual FSM.
 * TV1's real Mode Manager is not included here.
 */
bool mode_manager_take_action(button_event_t *out)
{
    return button_pop_event(out);
}

/* ---------- Simulation helpers ---------- */

static void check(traffic_phase_t phase,
                  light_t expected1,
                  light_t expected2)
{
    light_t r1, r2;

    traffic_get_pair(&r1, &r2);

    assert(g_app_state.phase == phase);
    assert(r1 == expected1);
    assert(r2 == expected2);
    assert_safe();
}

static void advance(uint32_t ticks)
{
    while (ticks-- > 0U) {
        button_scan_10ms();

        for (unsigned i = 0U; i < TMR_COUNT; i++) {
            if (timers[i].running &&
                timers[i].remaining > 0U) {

                if (--timers[i].remaining == 0U) {
                    timers[i].running = false;
                    timers[i].expired = true;
                }
            }
        }

        if (g_app_state.mode == MODE_AUTO)
            fsm_automatic_run();
        else if (g_app_state.mode == MODE_MANUAL)
            fsm_manual_run();

        assert_safe();
    }
}

static void tap(unsigned id)
{
    assert(id < 4U);

    levels[id] = GPIO_PIN_RESET;
    advance(3U);

    levels[id] = GPIO_PIN_SET;
    advance(3U);
}

static void reset_environment(void)
{
    memset(timers, 0, sizeof(timers));
    gpio_output = 0U;

    for (unsigned i = 0U; i < 4U; i++)
        levels[i] = GPIO_PIN_SET;

    global_init();
    button_init();
    traffic_init();

    assert(!global_has_fault());
}

/* ---------- Integration scenarios ---------- */

static void test_automatic_full_cycle(void)
{
    reset_environment();
    fsm_automatic_reset();

    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance(100U);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    advance(1000U);
    check(PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    advance(300U);
    check(PHASE_P2, LIGHT_RED, LIGHT_RED);

    advance(100U);
    check(PHASE_P3, LIGHT_GREEN, LIGHT_RED);

    advance(1000U);
    check(PHASE_P4, LIGHT_AMBER, LIGHT_RED);

    advance(300U);
    check(PHASE_P5, LIGHT_RED, LIGHT_RED);

    advance(100U);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

static void test_auto_to_manual(void)
{
    reset_environment();
    fsm_automatic_reset();

    advance(100U);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    /* Simulate TV1's mode-switch reset sequence. */
    g_app_state.mode = MODE_MANUAL;
    fsm_manual_reset();

    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance(100U);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    tap(BTN_2);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    tap(BTN_2);
    check(PHASE_P1, LIGHT_RED, LIGHT_AMBER);

    tap(BTN_2);
    check(PHASE_P2, LIGHT_RED, LIGHT_RED);

    /* NEXT during clearance must be ignored. */
    tap(BTN_2);
    check(PHASE_P2, LIGHT_RED, LIGHT_RED);

    advance(94U);

    tap(BTN_2);
    check(PHASE_P3, LIGHT_GREEN, LIGHT_RED);
}

static void test_manual_stop_restart(void)
{
    reset_environment();

    g_app_state.mode = MODE_MANUAL;
    fsm_manual_reset();

    advance(100U);
    tap(BTN_2);

    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);

    /* STOP must immediately request ALL_RED. */
    tap(BTN_3);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance(100U);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    /* RESTART returns automatically to P0 after 1 s. */
    tap(BTN_4);
    check(PHASE_SAFE, LIGHT_RED, LIGHT_RED);

    advance(97U);
    check(PHASE_P0, LIGHT_RED, LIGHT_GREEN);
}

int main(void)
{
    test_automatic_full_cycle();
    test_auto_to_manual();
    test_manual_stop_restart();

    puts("PASS: TV2 integration tests (Auto, Manual, Button, Traffic)");
    return 0;
}
