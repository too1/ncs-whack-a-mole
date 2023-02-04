#ifndef __APP_SENSORS_H
#define __APP_SENSORS_H

#include <zephyr/kernel.h>
typedef struct {
	uint32_t type;
} app_sensors_event_t;

typedef void (*app_sensors_callback_t)(app_sensors_event_t *event);

int app_sensors_init(app_sensors_callback_t callback);

int app_sensors_read_mpu(void);

#endif
