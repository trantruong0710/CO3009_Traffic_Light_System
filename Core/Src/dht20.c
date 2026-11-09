#include "dht20.h"
#include <stddef.h>

static dht20_data_t sensor_data;

void dht20_init(void){
	sensor_data.temperature_c = 0.0f;
	sensor_data.humidity_pct = 0.0f;
	sensor_data.status = SENSOR_UNKNOWN;
}

void dht20_service(void){

}


bool dht20_get_data(dht20_data_t *out){					//Caller đưa địa chỉ vùng nhớ nhận kết quả
	if(out == NULL){
		return false;

	}

	*out = sensor_data;
	return sensor_data.status == SENSOR_OK;
}
