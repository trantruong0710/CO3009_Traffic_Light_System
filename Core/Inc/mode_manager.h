#ifndef MODE_MANAGER_H
#define MODE_MANAGER_H

#include "app_types.h"

void mode_manager_init(void);
void mode_manager_run(void);
app_mode_t mode_manager_get_mode(void);
bool mode_manager_take_action(button_event_t *out_event);

#endif
