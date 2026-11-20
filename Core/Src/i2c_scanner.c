#include "i2c_scanner.h"
#include <stddef.h>

volatile i2c_scan_result_t g_i2c_scan;
static I2C_HandleTypeDef *scan_bus = NULL;			//scan_bus nhớ địa chỉ handle, không sao chép toàn handle

#define I2C_SCAN_ADDR_MIN (0x08U)
#define I2C_SCAN_ADDR_MAX (0x77U)
#define I2C_SCAN_TIMEOUT_MS (2U)

void i2c_scanner_init(I2C_HandleTypeDef *bus){
	uint16_t i;

	for(i = 0; i < 128; i++){
		g_i2c_scan.detected[i] = 0U;
	}

	g_i2c_scan.device_count = 0U;
	g_i2c_scan.next_address = I2C_SCAN_ADDR_MIN;
	g_i2c_scan.bus_error_count = 0U;
	g_i2c_scan.complete = (bus == NULL);
	scan_bus = bus;
}


void i2c_scanner_service(void){
	uint8_t address;
	HAL_StatusTypeDef status;						//HAL_OK/HAL_ERROR/HAL_BUSY/HAL_TIMEOUT

	if(scan_bus == NULL || g_i2c_scan.complete){	//Nếu chưa có bus hoặc đã quét xong
		return;
	}

	address = g_i2c_scan.next_address;
	status = HAL_I2C_IsDeviceReady(scan_bus, (uint16_t)(address << 1U), 1U, I2C_SCAN_TIMEOUT_MS);		//1U: số trial. 2 ms: tham số timeout

	if(status == HAL_OK){							//đánh dấu địa chỉ ACK và tăng count
		g_i2c_scan.detected[address] = 1U;
		g_i2c_scan.device_count = (uint8_t)(g_i2c_scan.device_count + 1U);

    } else {
        uint32_t error = HAL_I2C_GetError(scan_bus);
        /* STM32F1 HAL IsDeviceReady clears AF and can return HAL_ERROR
         * with ErrorCode=NONE for an ordinary address NACK. Other HAL
         * revisions report AF instead. Neither is a bus fault here.
         * This API does not distinguish every low-level timeout case.
         */
        if (status != HAL_ERROR ||
            (error != HAL_I2C_ERROR_NONE && error != HAL_I2C_ERROR_AF)) {
            g_i2c_scan.bus_error_count++;
        }
    }


	if(address >= I2C_SCAN_ADDR_MAX){							//Toàn scan cần 0x77−0x08+1=112 lần service
		g_i2c_scan.complete = true;

	}else{
		g_i2c_scan.next_address++;					//Chưa phải cuối thì tăng địa chỉ cho lần gọi tiếp
	}


}
