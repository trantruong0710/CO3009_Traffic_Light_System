
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "global.h"
#include "traffic_light.h"
#include "../traffic_mocks/main.h"

/* Simulated GPIO output latch. */
static uint16_t gpio_state;
static unsigned write_count;
static uint8_t read_code(uint16_t pin_a, uint16_t pin_b);

void traffic_test_bsrr(uint32_t value)
{
    uint16_t set = (uint16_t)value;
    uint16_t reset = (uint16_t)(value >> 16U);

    assert((set & reset) == 0U);

    gpio_state |= set;
    gpio_state &= (uint16_t)~reset;
    /* Verify physical outputs after every atomic GPIO write. */
assert(!(read_code(TL1_A_Pin, TL1_B_Pin) == TL1_GREEN_CODE &&
         read_code(TL2_A_Pin, TL2_B_Pin) == TL2_GREEN_CODE));
    write_count++;
}

static uint8_t read_code(
    uint16_t pin_a,
    uint16_t pin_b)
{
    return (uint8_t)(
        ((gpio_state & pin_a) ? 2U : 0U) |
        ((gpio_state & pin_b) ? 1U : 0U)
    );
}

static void check(
    light_t expected_r1,
    light_t expected_r2,
    uint8_t code1,
    uint8_t code2)
{
    light_t r1, r2;

    traffic_get_pair(&r1, &r2);

    assert(r1 == expected_r1);
    assert(r2 == expected_r2);

    assert(read_code(TL1_A_Pin, TL1_B_Pin) == code1);
    assert(read_code(TL2_A_Pin, TL2_B_Pin) == code2);

    assert(!(r1 == LIGHT_GREEN && r2 == LIGHT_GREEN));
}

static void reset_test(void)
{
    gpio_state = 0U;
    write_count = 0U;
    global_init();
    traffic_init();
}

/* TEST 1: Initialization produces both RED. */
static void test_initialization(void)
{
    reset_test();

    check(LIGHT_RED, LIGHT_RED,
          TL1_RED_CODE, TL2_RED_CODE);

    assert(write_count == 1U);
}

/* TEST 2: Every valid color pair is accepted. */
static void test_color_pairs(void)
{
    reset_test();

    for (unsigned a = 0U; a < 4U; a++) {
        for (unsigned b = 0U; b < 4U; b++) {

            if (a == LIGHT_GREEN && b == LIGHT_GREEN)
                continue;

            traffic_force_all_red();

            assert(traffic_apply_pair(
                (light_t)a, (light_t)b) == TRAFFIC_OK);

            check((light_t)a, (light_t)b,
                  (uint8_t)(a == LIGHT_RED ? TL1_RED_CODE :
                            a == LIGHT_AMBER ? TL1_AMBER_CODE :
                            a == LIGHT_GREEN ? TL1_GREEN_CODE :
                            TL1_OFF_CODE),
                  (uint8_t)(b == LIGHT_RED ? TL2_RED_CODE :
                            b == LIGHT_AMBER ? TL2_AMBER_CODE :
                            b == LIGHT_GREEN ? TL2_GREEN_CODE :
                            TL2_OFF_CODE));
        }
    }
}

/* TEST 3: GREEN/GREEN is rejected and latched. */
static void test_double_green(void)
{
    reset_test();

    assert(traffic_apply_pair(
        LIGHT_GREEN, LIGHT_GREEN) ==
        TRAFFIC_INVALID_PAIR);

    check(LIGHT_RED, LIGHT_RED,
          TL1_RED_CODE, TL2_RED_CODE);

    assert(global_has_fault());

    assert(traffic_apply_pair(
        LIGHT_RED, LIGHT_GREEN) ==
        TRAFFIC_HW_NOT_READY);
}

/* TEST 4: Direct green handoff is prohibited. */
static void test_direct_handoff(void)
{
    reset_test();

    assert(traffic_apply_pair(
        LIGHT_RED, LIGHT_GREEN) == TRAFFIC_OK);

    assert(traffic_apply_pair(
        LIGHT_GREEN, LIGHT_RED) ==
        TRAFFIC_INVALID_PAIR);

    check(LIGHT_RED, LIGHT_RED,
          TL1_RED_CODE, TL2_RED_CODE);
}

/* TEST 5: Legal handoff via ALL_RED succeeds. */
static void test_safe_handoff(void)
{
    reset_test();

    assert(traffic_apply_pair(
        LIGHT_RED, LIGHT_GREEN) == TRAFFIC_OK);

    traffic_force_all_red();

    assert(traffic_apply_pair(
        LIGHT_GREEN, LIGHT_RED) == TRAFFIC_OK);

    check(LIGHT_GREEN, LIGHT_RED,
          TL1_GREEN_CODE, TL2_RED_CODE);
}

/* TEST 6: Invalid enum values are rejected. */
static void test_invalid_values(void)
{
    reset_test();

    assert(traffic_apply_pair(
        (light_t)99, LIGHT_RED) ==
        TRAFFIC_INVALID_PAIR);

    assert(global_has_fault());
}

/* TEST 7: NULL output pointers are accepted. */
static void test_null_pointers(void)
{
    reset_test();

    light_t road1 = LIGHT_OFF;

    traffic_get_pair(&road1, NULL);
    assert(road1 == LIGHT_RED);

    traffic_get_pair(NULL, NULL);
}

/* TEST 8: Fault does not disappear without reinit. */
static void test_fault_latch(void)
{
    reset_test();

    traffic_apply_pair(LIGHT_GREEN, LIGHT_GREEN);

    assert(traffic_apply_pair(
        LIGHT_RED, LIGHT_RED) ==
        TRAFFIC_HW_NOT_READY);

    check(LIGHT_RED, LIGHT_RED,
          TL1_RED_CODE, TL2_RED_CODE);
}
/* TEST 9: Reverse GREEN handoff must be rejected. */
static void test_reverse_direct_handoff(void)
{
    reset_test();

    /* Road 1 is GREEN. */
    assert(traffic_apply_pair(
        LIGHT_GREEN, LIGHT_RED) == TRAFFIC_OK);

    /* Direct transfer to Road 2 GREEN is unsafe. */
    assert(traffic_apply_pair(
        LIGHT_RED, LIGHT_GREEN) ==
        TRAFFIC_INVALID_PAIR);

    /* Both roads must immediately become RED. */
    check(LIGHT_RED, LIGHT_RED,
          TL1_RED_CODE, TL2_RED_CODE);

    /* Safety fault must be recorded and latched. */
    assert((g_app_state.fault_flags & (1UL << 0)) != 0U);

    assert(traffic_apply_pair(
        LIGHT_RED, LIGHT_GREEN) ==
        TRAFFIC_HW_NOT_READY);
}

int main(void)
{
    test_initialization();
    test_color_pairs();
    test_double_green();
    test_direct_handoff();
    test_safe_handoff();
    test_invalid_values();
    test_null_pointers();
    test_fault_latch();
    test_reverse_direct_handoff();

    puts("PASS: 9/9 Traffic Driver unit tests");
    return 0;
}
