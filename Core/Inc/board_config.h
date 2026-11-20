#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/* First flash: diagnostic mode. Set to 0 only AFTER measuring this board. */
#ifndef APP_DIAGNOSTIC_MODE
#define APP_DIAGNOSTIC_MODE 0
#endif

#ifndef BUTTON_HW_VERIFIED
#define BUTTON_HW_VERIFIED 1
#endif
#ifndef TRAFFIC_HW_VERIFIED
#define TRAFFIC_HW_VERIFIED 1
#endif

/* Raw pin level when pressed: 0=LOW, 1=HIGH. Measure all four. */
#ifndef BTN1_ACTIVE_LEVEL
#define BTN1_ACTIVE_LEVEL 0
#endif
#ifndef BTN2_ACTIVE_LEVEL
#define BTN2_ACTIVE_LEVEL 0
#endif
#ifndef BTN3_ACTIVE_LEVEL
#define BTN3_ACTIVE_LEVEL 0
#endif
#ifndef BTN4_ACTIVE_LEVEL
#define BTN4_ACTIVE_LEVEL 0
#endif

/* Both roads measured by user: 00=OFF, 01=AMBER, 10=GREEN, 11=RED.
 * Code = (A << 1) | B. Green brightness still needs checking.
 */
#ifndef TL1_RED_CODE
#define TL1_RED_CODE 3
#endif
#ifndef TL1_AMBER_CODE
#define TL1_AMBER_CODE 1
#endif
#ifndef TL1_GREEN_CODE
#define TL1_GREEN_CODE 2
#endif
#ifndef TL1_OFF_CODE
#define TL1_OFF_CODE 0
#endif
#ifndef TL2_RED_CODE
#define TL2_RED_CODE 3
#endif
#ifndef TL2_AMBER_CODE
#define TL2_AMBER_CODE 1
#endif
#ifndef TL2_GREEN_CODE
#define TL2_GREEN_CODE 2
#endif
#ifndef TL2_OFF_CODE
#define TL2_OFF_CODE 0
#endif

#ifndef TV3_LCD_ADDRESS
#define TV3_LCD_ADDRESS 0x21 /* Scan: detected[33] = 1; confirm LCD identity/mapping. */
#endif

#if !APP_DIAGNOSTIC_MODE && (!BUTTON_HW_VERIFIED || !TRAFFIC_HW_VERIFIED)
#error "Runtime requires measured button levels and traffic color codes in board_config.h"
#endif
#if (BTN1_ACTIVE_LEVEL > 1) || (BTN2_ACTIVE_LEVEL > 1) || (BTN3_ACTIVE_LEVEL > 1) || (BTN4_ACTIVE_LEVEL > 1) || (BTN1_ACTIVE_LEVEL < 0) || (BTN2_ACTIVE_LEVEL < 0) || (BTN3_ACTIVE_LEVEL < 0) || (BTN4_ACTIVE_LEVEL < 0)
#error "Button active levels must be 0 or 1"
#endif
#if TV3_LCD_ADDRESS < 0x08 || TV3_LCD_ADDRESS > 0x77
#error "Invalid 7-bit LCD address"
#endif
#if TRAFFIC_HW_VERIFIED
#if !defined(TL1_RED_CODE) || !defined(TL1_AMBER_CODE) || !defined(TL1_GREEN_CODE) || !defined(TL1_OFF_CODE) || !defined(TL2_RED_CODE) || !defined(TL2_AMBER_CODE) || !defined(TL2_GREEN_CODE) || !defined(TL2_OFF_CODE)
#error "Define all eight measured traffic color codes"
#else
#define BOARD_VALID_CODES(r,a,g,o) ((r)>=0 && (r)<=3 && (a)>=0 && (a)<=3 && (g)>=0 && (g)<=3 && (o)>=0 && (o)<=3 && (r)!=(a) && (r)!=(g) && (r)!=(o) && (a)!=(g) && (a)!=(o) && (g)!=(o))
#if !BOARD_VALID_CODES(TL1_RED_CODE, TL1_AMBER_CODE, TL1_GREEN_CODE, TL1_OFF_CODE) || !BOARD_VALID_CODES(TL2_RED_CODE, TL2_AMBER_CODE, TL2_GREEN_CODE, TL2_OFF_CODE)
#error "Each traffic road needs four distinct codes in range 0..3"
#endif
#endif
#endif
#endif

