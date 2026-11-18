#include "dht20.h"
#include "i2c_scanner.h"
#include <stddef.h>
#include <stdint.h>

#define DHT20_I2C_ADDR           0x38
#define DHT20_I2C_TIMEOUT_MS     2
#define DHT20_POWERON_WAIT_MS    100						//Khoảng chờ trước kiểm tra ban đầu
#define DHT20_MEASURE_PERIOD_MS  2000						//Khoảng cách chờ giữa các lần đo
#define DHT20_CONVERSION_WAIT_MS 80							//Chờ tối thiểu sau trigger rồi mới đọc kết quả
#define DHT20_BUSY_TIMEOUT_MS    200						//	Quá hạn nếu cảm biến vẫn báo BUSY
#define DHT20_ERROR_RETRY_MS     2000
#define DHT20_STATUS_CALIBRATED  0x18						//bit trạng thái yêu cầu
#define DHT20_STATUS_BUSY        0x80						//bit BUSY

typedef enum {
    DHT_STATE_INIT = 0,
    DHT_STATE_IDLE,
    DHT_STATE_WAIT_CONVERSION,
    DHT_STATE_ERROR_WAIT
} dht20_state_t;

extern I2C_HandleTypeDef hi2c1;
static dht20_data_t data;
static uint32_t since;						//mốc ms
static dht20_state_t state;						//INIT, IDLE, WAIT_CONVERSION hoac ERROR_WAIT


static uint8_t crc8(const uint8_t *p){
	uint8_t crc = 0xFF;					   	//11111111 làm giá trị ban đầu
	for(int i = 0; i < 6; i++ ){
		crc ^= p[i];
		for(int b = 0; b < 8; b++){
			if((crc & 0x80) != 0){							//x80 để chỉ giữ bit 7.
				crc = (uint8_t)((crc << 1) ^ 0x31);			//0x31 hằng số do thuật toán CRC
			}else{
				crc = (uint8_t)(crc << 1);
			}
		}
	}

	return crc;
}

static void dht20_fail(uint32_t now)
{
    data.status = SENSOR_ERROR;
    state = DHT_STATE_ERROR_WAIT;
    since = now;
}

void dht20_init(void)
{
    data.temperature_c = 0.0f;
    data.humidity_pct = 0.0f;
    data.status = SENSOR_UNKNOWN;
    since = HAL_GetTick();
    state = DHT_STATE_INIT;
}

void dht20_service(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t frame[7];													//([0]=status, [1..3]=humidity , [3..5]=temperature, [6]=CRC-8)
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};								//(0xAC: Trigger Measurement, 0x33, 0x00: tham số chuẩn)
    uint32_t rh;
    uint32_t t;
    if (g_i2c_scan.next_address != 0 && !g_i2c_scan.complete) {
        since = now;
        state = DHT_STATE_INIT;
        data.status = SENSOR_UNKNOWN;
        return;
    }

    switch (state) {
    case DHT_STATE_INIT:
        if ((uint32_t)(now - since) < DHT20_POWERON_WAIT_MS) {			// thoát nếu chưa đủ 100ms từ lúc vào INIT
            return;
        }
        if (HAL_I2C_Master_Receive(&hi2c1, DHT20_I2C_ADDR << 1, frame, 1, DHT20_I2C_TIMEOUT_MS) != HAL_OK) {
            dht20_fail(now);
            return;
        }
        if ((frame[0] & DHT20_STATUS_CALIBRATED) != DHT20_STATUS_CALIBRATED) {			//Kiểm tra bit hiệu chuẩn 0x18, nếu cảm biến chưa sẵn sàng exit
            dht20_fail(now);
            return;
        }
        state = DHT_STATE_IDLE;											//để lượt sau bắt đầu kích hoạt phép đo
        since = now - DHT20_MEASURE_PERIOD_MS;
        break;

    case DHT_STATE_IDLE:
        if ((uint32_t)(now - since) < DHT20_MEASURE_PERIOD_MS) {					//chưa đủ 2000 ms từ lần đo trước
            return;
        }
        if (HAL_I2C_Master_Transmit(&hi2c1, DHT20_I2C_ADDR << 1, cmd, 3, DHT20_I2C_TIMEOUT_MS) != HAL_OK) {		//Gửi 3 byte lệnh {0xAC, 0x33, 0x00}
            dht20_fail(now);
            return;
        }
        state = DHT_STATE_WAIT_CONVERSION;								//chuyển sang trạng thái chờ chuyển đổi 80 ms
        since = now;
        break;

    case DHT_STATE_WAIT_CONVERSION:
        if ((uint32_t)(now - since) < DHT20_CONVERSION_WAIT_MS) {
            return;
        }
        if (HAL_I2C_Master_Receive(&hi2c1, DHT20_I2C_ADDR << 1, frame, 7, DHT20_I2C_TIMEOUT_MS) != HAL_OK) {
            dht20_fail(now);
            return;
        }
        if ((frame[0] & DHT20_STATUS_BUSY) != 0) {								//Nếu bit 7 (BUSY) của frame[0] vẫn bằng 1 nghĩa là cảm biến chưa đo xong
            if ((uint32_t)(now - since) >= DHT20_BUSY_TIMEOUT_MS) {
                dht20_fail(now);
            }
            return;
        }
        if ((frame[0] & DHT20_STATUS_CALIBRATED) != DHT20_STATUS_CALIBRATED) {
            dht20_fail(now);
            return;
        }
        if (crc8(frame) != frame[6]) {
            dht20_fail(now);
            return;
        }
        rh = ((uint32_t)frame[1] << 12) | ((uint32_t)frame[2] << 4) | (frame[3] >> 4);		//20 bit độ ẩm: frame[1] là bit [19:12], frame[2] là bit [11:4], 4bit cao frame[3] là bit [3:0].
        t = ((uint32_t)(frame[3] & 0x0F) << 16) | ((uint32_t)frame[4] << 8) | frame[5];		//20 bit nhiệt độ: 4bit low frame[3] là bit [19:16], frame[4] là bit [15:8], frame[5] là bit [7:0].
        data.humidity_pct = (float)rh * 100.0f / 1048576.0f;								// Ma 20 bit: 0..1048575, he so chia la 2^20
        data.temperature_c = (float)t * 200.0f / 1048576.0f - 50.0f;
        data.status = SENSOR_OK;
        state = DHT_STATE_IDLE;
        since = now;
        break;

    case DHT_STATE_ERROR_WAIT:
        if ((uint32_t)(now - since) >= DHT20_ERROR_RETRY_MS) {
            state = DHT_STATE_INIT;
            since = now;
        }
        break;

    default:
        dht20_fail(now);
        break;
    }
}

bool dht20_get_data(dht20_data_t *out)
{
    if (out == NULL) return false;
    *out = data;
    return data.status == SENSOR_OK;
}
