#include "i2c_scanner.h"
#include <stddef.h>

volatile i2c_scan_result_t g_i2c_scan;
static I2C_HandleTypeDef *scan_bus = NULL;			//scan_bus nhớ địa chỉ handle, không sao chép toàn handle



void i2c_scanner_init(I2C_HandleTypeDef *bus){
	const i2c_scan_result_t init_res = {0};			//count/error/next bằng 0, complete=false, mảng toàn 0
	g_i2c_scan = init_res;
	g_i2c_scan.next_address = 0x08U;
	scan_bus = bus;
}


void i2c_scanner_service(void){
	uint8_t address;
	HAL_StatusTypeDef status;						//HAL_OK/HAL_ERROR/HAL_BUSY/HAL_TIMEOUT

	if(scan_bus == NULL || g_i2c_scan.complete){	//Nếu chưa có bus hoặc đã quét xong
		return;
	}

	address = g_i2c_scan.next_address;
	status = HAL_I2C_IsDeviceReady(scan_bus, (uint16_t)(address << 1U), 1U, 5U);		//1U: số trial. 5U: tham số timeout

	if(status == HAL_OK){							//đánh dấu địa chỉ ACK và tăng count
		g_i2c_scan.detected[address] = 1U;
		g_i2c_scan.device_count++;

	}else if(status != HAL_ERROR || HAL_I2C_GetError(scan_bus) != HAL_I2C_ERROR_AF){		//AF là acknowledge failure : địa chỉ không có thiết bị trả lời
		g_i2c_scan.bus_error_count++;														//Bộ đếm này tăng khi quá trình giao tiếp I2C gặp trục trặc

	}

	if(address == 0x77U){							//Toàn scan cần 0x77−0x08+1=112 lần service
		g_i2c_scan.complete = true;

	}else{
		g_i2c_scan.next_address++;					//Chưa phải cuối thì tăng địa chỉ cho lần gọi tiếp
	}


}
