#ifndef CONFIG_H
#define CONFIG_H

#include "app_types.h"

void config_init_defaults(void);
const traffic_config_t *config_get_active(void);
const traffic_config_t *config_get_staged(void);
uint16_t config_get_red_s(const traffic_config_t *cfg, road_t road);
bool config_validate(const traffic_config_t *cfg);
void config_begin_edit(void);

bool config_adjust_staged(tuning_field_t field, int8_t delta_seconds);
bool config_save_edit(void);
void config_cancel_edit(void);
bool config_is_dirty(void);

#endif
