#ifndef DIAG_MOCK_HAL_H
#define DIAG_MOCK_HAL_H
#define __MAIN_H
#include <stdint.h>
typedef struct {uint32_t BSRR,ODR,IDR,CRL,CRH;} GPIO_TypeDef;
typedef struct {uint32_t Pin,Mode,Pull,Speed;} GPIO_InitTypeDef;
typedef struct {int unused;} I2C_HandleTypeDef;
typedef enum {HAL_OK,HAL_ERROR,HAL_BUSY,HAL_TIMEOUT} HAL_StatusTypeDef;
typedef enum {GPIO_PIN_RESET,GPIO_PIN_SET} GPIO_PinState;
extern GPIO_TypeDef port_a,port_b,port_c;
#define GPIOB (&port_b)
#define TL1_A_Pin (1U<<3)
#define TL1_B_Pin (1U<<5)
#define TL2_A_Pin (1U<<4)
#define TL2_B_Pin (1U<<10)
#define BTN_1_Pin (1U<<8)
#define BTN_2_Pin (1U<<9)
#define BTN_3_Pin (1U<<7)
#define BTN_4_Pin (1U<<6)
#define BTN_1_GPIO_Port (&port_a)
#define BTN_2_GPIO_Port (&port_a)
#define BTN_3_GPIO_Port (&port_c)
#define BTN_4_GPIO_Port (&port_b)
#define LD2_GPIO_Port (&port_a)
#define LD2_Pin (1U<<5)
#define GPIO_MODE_INPUT 0
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_LOW 0
#define HAL_I2C_ERROR_NONE 0U
#define HAL_I2C_ERROR_AF 4
uint32_t HAL_GetTick(void);
void HAL_GPIO_Init(GPIO_TypeDef*,GPIO_InitTypeDef*);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*,uint16_t);
void HAL_GPIO_TogglePin(GPIO_TypeDef*,uint16_t);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef*);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*,uint16_t,uint32_t,uint32_t);
#endif
