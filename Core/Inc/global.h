#ifndef GLOBAL_H
#define GLOBAL_H

#include "app_types.h"

typedef struct {
    traffic_config_t active_config;
    traffic_config_t staged_config;
    app_mode_t mode;
    traffic_phase_t phase;
    uint32_t fault_flags;
} app_shared_state_t;

extern app_shared_state_t g_app_state;

void global_init(void);

void global_raise_fault(uint32_t mask);
bool global_has_fault(void);

#endif
