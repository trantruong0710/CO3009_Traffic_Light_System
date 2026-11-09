#ifndef INC_I2C_SCANNER_H_
#define INC_I2C_SCANNER_H_

#include "main.h"
#include <stdbool.h>

typedef struct {
	uint8_t detected[128];
	uint8_t device_count;					//số địa chỉ ACK
	uint8_t next_address;					// địa chỉ sẽ thử trong lần service tới
	uint16_t bus_error_count;				//đếm kết quả lỗi ngoài NACK thông thường
	bool complete;
}i2c_scan_result_t;

extern volatile i2c_scan_result_t g_i2c_scan;

void i2c_scanner_init(I2C_HandleTypeDef *bus);
void i2c_scanner_service(void);


#endif /* INC_I2C_SCANNER_H_ */
