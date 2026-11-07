#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include "app_types.h"

typedef enum {
    TRAFFIC_OK = 0, TRAFFIC_INVALID_PAIR, TRAFFIC_HW_NOT_READY
} traffic_result_t;

void traffic_init(void);

traffic_result_t traffic_apply_pair(light_t road1, light_t road2);
void traffic_force_all_red(void);
void traffic_get_pair(light_t *road1, light_t *road2);

#endif
