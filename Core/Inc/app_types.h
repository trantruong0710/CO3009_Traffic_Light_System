#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { ROAD_1 = 0, ROAD_2 = 1 } road_t;
typedef enum { LIGHT_RED = 0, LIGHT_AMBER, LIGHT_GREEN, LIGHT_OFF } light_t;
typedef enum { MODE_AUTO = 0, MODE_MANUAL, MODE_TUNING } app_mode_t;
typedef enum {
    PHASE_P0 = 0, PHASE_P1, PHASE_P2,
    PHASE_P3, PHASE_P4, PHASE_P5, PHASE_SAFE
} traffic_phase_t;
typedef enum { BTN_1 = 0, BTN_2, BTN_3, BTN_4 } button_id_t;
typedef enum { BTN_EVT_DOWN = 0, BTN_EVT_SHORT, BTN_EVT_LONG } button_kind_t;

typedef struct {
    button_id_t id;
    button_kind_t kind;
} button_event_t;

typedef struct {
    uint16_t green_s[2];
    uint8_t amber_s[2];
} traffic_config_t;

typedef enum {
    FIELD_R1 = 0, FIELD_G1, FIELD_A1,
    FIELD_R2, FIELD_G2, FIELD_A2
} tuning_field_t;

#define TICK_MS       (10U)
#define CLEARANCE_S   (1U)
#define MIN_GREEN_S   (5U)
#define MAX_GREEN_S   (60U)
#define MIN_AMBER_S   (2U)
#define MAX_AMBER_S   (5U)

#endif
