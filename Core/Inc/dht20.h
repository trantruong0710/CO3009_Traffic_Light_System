#ifndef DHT20_H
#define DHT20_H

#include <stdbool.h>

typedef enum { SENSOR_UNKNOWN = 0, SENSOR_OK, SENSOR_ERROR } sensor_status_t;
typedef struct {
    float temperature_c;
    float humidity_pct;
    sensor_status_t status;
} dht20_data_t;

void dht20_init(void);
void dht20_service(void);
bool dht20_get_data(dht20_data_t *out);

#endif
