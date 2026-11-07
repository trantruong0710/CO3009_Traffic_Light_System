#ifndef BUTTON_H
#define BUTTON_H

#include "app_types.h"

void button_init(void);
void button_scan_10ms(void);
bool button_pop_event(button_event_t *out_event);
void button_clear_events(void);
void button_block_held_until_release(void);

#endif
