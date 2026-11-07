#ifndef LCD_H
#define LCD_H

#include "app_types.h"

void lcd_init(void);

void lcd_set_view(app_mode_t mode, traffic_phase_t phase,
                  tuning_field_t field, bool dirty);
void lcd_set_notice(const char *ascii_msg);
void lcd_service(void);

#endif
