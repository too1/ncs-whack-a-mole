#ifndef __APP_BT_H
#define __APP_BT_H

#include <zephyr/kernel.h>

typedef enum {APP_BT_EVT_CONNECTED, APP_BT_EVT_DISCONNECTED, APP_BT_EVT_RX} app_bt_evt_type_t;

typedef struct {
	app_bt_evt_type_t type;
	const uint8_t * buf;
	uint32_t length;
} app_bt_event_t;

typedef void (*app_bt_callback_t)(app_bt_event_t *event);

int app_bt_init(app_bt_callback_t callback);

#endif
