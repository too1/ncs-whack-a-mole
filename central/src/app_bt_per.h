#ifndef __APP_BT_PER_H
#define __APP_BT_PER_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>

enum {APP_BT_PER_EVT_RX_DATA};

struct app_bt_per_evt_t {
    uint32_t type;
    const uint8_t *data;
    uint16_t data_len;
};

typedef void (*app_bt_per_callback_t)(struct app_bt_per_evt_t *event);

int app_bt_per_connected(struct bt_conn *conn);

int app_bt_per_disconnected(struct bt_conn *conn);

int app_bt_per_init(app_bt_per_callback_t callback);

int app_bt_per_send_str(const uint8_t *string, uint16_t len);

#endif
