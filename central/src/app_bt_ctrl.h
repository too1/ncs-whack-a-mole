#ifndef __APP_BT_CTRL_H
#define __APP_BT_CTRL_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/conn.h>

enum {APP_BT_CTRL_EVT_RX_DATA};

struct app_bt_ctrl_evt_t {
    uint32_t type;
    const uint8_t *data;
    uint16_t data_len;
};

typedef void (*app_bt_ctrl_callback_t)(struct app_bt_ctrl_evt_t *event);

int app_bt_ctrl_connected(struct bt_conn *conn);

int app_bt_ctrl_disconnected(struct bt_conn *conn);

int app_bt_ctrl_init(app_bt_ctrl_callback_t callback);

int app_bt_ctrl_send_str(const uint8_t *string, uint16_t len);

#endif
