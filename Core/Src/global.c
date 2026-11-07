#include "global.h"

app_shared_state_t g_app_state;

void global_init(void)
{

    const app_shared_state_t initial_state = {
        .active_config = { .green_s = {10U, 10U}, .amber_s = {3U, 3U} },
        .staged_config = { .green_s = {10U, 10U}, .amber_s = {3U, 3U} },
        .mode = MODE_AUTO,
        .phase = PHASE_SAFE,
        .fault_flags = 0U
    };

    g_app_state = initial_state;
}

void global_raise_fault(uint32_t mask)
{

    g_app_state.fault_flags |= mask;
}

bool global_has_fault(void)
{
    return g_app_state.fault_flags != 0U;
}
