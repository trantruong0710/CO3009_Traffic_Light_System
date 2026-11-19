
#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H

#include <stdint.h>

typedef struct {
    uint8_t id;
} GPIO_TypeDef;

typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET = 1
} GPIO_PinState;

extern GPIO_TypeDef mock_gpioa;
extern GPIO_TypeDef mock_gpiob;
extern GPIO_TypeDef mock_gpioc;

#define BTN_1_GPIO_Port (&mock_gpioa)
#define BTN_2_GPIO_Port (&mock_gpioa)
#define BTN_3_GPIO_Port (&mock_gpioc)
#define BTN_4_GPIO_Port (&mock_gpiob)

#define BTN_1_Pin (1U << 8)
#define BTN_2_Pin (1U << 9)
#define BTN_3_Pin (1U << 7)
#define BTN_4_Pin (1U << 6)

GPIO_PinState HAL_GPIO_ReadPin(
    GPIO_TypeDef *port,
    uint16_t pin
);

#endif
