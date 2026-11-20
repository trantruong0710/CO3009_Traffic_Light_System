#ifndef TV3_MOCK_HAL_H
#define TV3_MOCK_HAL_H
#define __MAIN_H
#include <stdint.h>
typedef struct { int unused; } I2C_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
#define HAL_I2C_ERROR_NONE 0U
#define HAL_I2C_ERROR_AF 4U
#define HAL_I2C_ERROR_BERR 1U
uint32_t HAL_GetTick(void);
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *bus);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *bus, uint16_t address, uint32_t trials, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *bus, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *bus, uint16_t address, uint8_t *data, uint16_t length, uint32_t timeout);
#endif
