#ifndef BOARD_DIAGNOSTICS_H
#define BOARD_DIAGNOSTICS_H
#include <stdint.h>
/* Debugger mailbox: write arguments FIRST, command LAST, then Resume.
 * 1=restart I2C scan; 2=apply raw A/B code to selected road;
 * 3=release both road outputs to high impedance.
 */
typedef struct {
    uint32_t alive;
    uint32_t tick_ms;
    uint32_t raw_buttons[4];
    uint32_t command;
    uint32_t road;
    uint32_t code;
    uint32_t last_command;
    uint32_t result; /* 0=OK, 1=invalid request, 2=output latch mismatch */
    uint32_t applied_count;
    uint32_t applied_road;
    uint32_t applied_code;
    uint32_t gpio_b_odr; /* Output latch: requested electrical levels. */
    uint32_t gpio_b_idr; /* Actual pin input levels, not LED colors. */
    uint32_t gpio_b_crl;
    uint32_t gpio_b_crh;
    uint32_t pin_code;   /* A/B decoded from IDR for applied_road. */
} board_diagnostics_t;
extern volatile board_diagnostics_t g_board_diag;
void board_diagnostics_init(void);
void board_diagnostics_service(void);
#endif
