#ifndef SOFTWARE_TIMER_H
#define SOFTWARE_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TMR_PHASE = 0, TMR_CLEARANCE, TMR_UI_MSG, TMR_COUNT
} soft_timer_id_t;

void swtimer_init(void);
void swtimer_tick_10ms(void);
bool swtimer_start(soft_timer_id_t id, uint32_t ticks);
void swtimer_stop(soft_timer_id_t id);
bool swtimer_expired(soft_timer_id_t id);
uint32_t swtimer_remaining(soft_timer_id_t id);

#endif
