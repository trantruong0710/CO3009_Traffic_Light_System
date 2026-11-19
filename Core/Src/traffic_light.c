
#include "traffic_light.h"
#include "main.h"
#include "global.h"
#include <stdint.h>

/* Keep disabled until both hardware truth tables are verified. */
#ifndef TRAFFIC_HW_VERIFIED
#define TRAFFIC_HW_VERIFIED 0
#endif

/* Provisional fault bit: coordinate with TV1. */
#define TV2_FAULT_INVALID_PAIR (1UL << 0)

static light_t current_r1 = LIGHT_OFF;
static light_t current_r2 = LIGHT_OFF;
static bool fault_latched = false;

#if TRAFFIC_HW_VERIFIED

/* Each color code uses bit1=A, bit0=B. */
#ifndef TL1_RED_CODE
#error "Missing verified traffic color codes"
#endif

static const uint8_t codes[2][4] = {
    {
        TL1_RED_CODE, TL1_AMBER_CODE,
        TL1_GREEN_CODE, TL1_OFF_CODE
    },
    {
        TL2_RED_CODE, TL2_AMBER_CODE,
        TL2_GREEN_CODE, TL2_OFF_CODE
    }
};

/* Test-only hook for observing atomic GPIO writes. */
#ifdef TRAFFIC_TEST
extern void traffic_test_bsrr(uint32_t value);
#define WRITE_BSRR(value) traffic_test_bsrr(value)
#else
#define WRITE_BSRR(value) (GPIOB->BSRR = (value))
#endif

static void write_pair(light_t r1, light_t r2)
{
    uint8_t a = codes[0][r1];
    uint8_t b = codes[1][r2];

    uint32_t all = TL1_A_Pin | TL1_B_Pin |
                   TL2_A_Pin | TL2_B_Pin;

    uint32_t set = 0U;

    if (a & 2U) set |= TL1_A_Pin;
    if (a & 1U) set |= TL1_B_Pin;
    if (b & 2U) set |= TL2_A_Pin;
    if (b & 1U) set |= TL2_B_Pin;

    /* STM32F1 BSRR sets and resets pins atomically. */
    WRITE_BSRR(set | ((all & ~set) << 16U));
}
#endif

static bool valid_light(light_t light)
{
    return light >= LIGHT_RED && light <= LIGHT_OFF;
}

void traffic_init(void)
{
    current_r1 = LIGHT_OFF;
    current_r2 = LIGHT_OFF;
    fault_latched = false;

    traffic_force_all_red();
}

void traffic_force_all_red(void)
{
#if TRAFFIC_HW_VERIFIED
    write_pair(LIGHT_RED, LIGHT_RED);
    current_r1 = LIGHT_RED;
    current_r2 = LIGHT_RED;
#else
    /*
     * Physical RED code is unknown.
     * Do not write guessed GPIO combinations.
     */
    current_r1 = LIGHT_OFF;
    current_r2 = LIGHT_OFF;
#endif
}

traffic_result_t traffic_apply_pair(light_t r1, light_t r2)
{
    if (!valid_light(r1) || !valid_light(r2) ||
        (r1 == LIGHT_GREEN && r2 == LIGHT_GREEN)) {

        fault_latched = true;
        global_raise_fault(TV2_FAULT_INVALID_PAIR);
        traffic_force_all_red();

        return TRAFFIC_INVALID_PAIR;
    }

    if (!TRAFFIC_HW_VERIFIED || fault_latched) {
        return TRAFFIC_HW_NOT_READY;
    }

#if TRAFFIC_HW_VERIFIED
    /*
     * Reject a direct transfer of GREEN between roads.
     * FSM must pass through the all-red clearance phase.
     */
    if ((current_r1 == LIGHT_GREEN &&
         r2 == LIGHT_GREEN) ||
        (current_r2 == LIGHT_GREEN &&
         r1 == LIGHT_GREEN)) {

        fault_latched = true;
        global_raise_fault(TV2_FAULT_INVALID_PAIR);
        traffic_force_all_red();

        return TRAFFIC_INVALID_PAIR;
    }

    write_pair(r1, r2);
    current_r1 = r1;
    current_r2 = r2;

    return TRAFFIC_OK;
#else
    return TRAFFIC_HW_NOT_READY;
#endif
}

void traffic_get_pair(light_t *r1, light_t *r2)
{
    if (r1 != NULL) *r1 = current_r1;
    if (r2 != NULL) *r2 = current_r2;
}
