#include "fsm_tuning.h"
#include "config.h"
#include "mode_manager.h"
#include "lcd.h"

static tuning_field_t field;

void fsm_tuning_reset(void)
{
    field = FIELD_R1;
    config_begin_edit();
    lcd_set_notice("");
    lcd_set_view(MODE_TUNING, PHASE_SAFE, field, config_is_dirty());
}

void fsm_tuning_run(void)
{
    button_event_t event;
    bool ok;
    int8_t delta;

    if (mode_manager_get_mode() != MODE_TUNING) return;
    if (mode_manager_take_action(&event)) {
        if (event.id == BTN_2 && event.kind == BTN_EVT_SHORT) {
            field = (tuning_field_t)(((int)field + 1) % 6);
            lcd_set_notice("");
        } else if ((event.id == BTN_3 || event.id == BTN_4) && (event.kind == BTN_EVT_DOWN || event.kind == BTN_EVT_REPEAT)) {
            if (event.id == BTN_3) delta = 1;
            else delta = -1;
            ok = config_adjust_staged(field, delta);
            if (ok) lcd_set_notice("");
            else lcd_set_notice("LIMIT");
        } else if (event.id == BTN_2 && event.kind == BTN_EVT_LONG) {
            ok = config_save_edit();
            if (ok) lcd_set_notice("SAVED");
            else lcd_set_notice("INVALID");
        }
    }
    /* BTN1 SHORT cancels staged edits in TV1's mode manager, not in this FSM. */
    lcd_set_view(MODE_TUNING, PHASE_SAFE, field, config_is_dirty());
}
