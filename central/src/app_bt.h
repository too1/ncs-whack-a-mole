#ifndef __APP_BT_H
#define __APP_BT_H

#include <zephyr/kernel.h>
#include <color.h>

enum {APP_BT_EVT_CON_NUM_CHANGE, APP_BT_EVT_RX_DATA};

struct app_bt_evt_t {
    uint32_t type;
    uint32_t num_connected;
    const uint8_t *data;
    uint16_t data_len;
    uint32_t con_index;
};

typedef void (*app_bt_callback_t)(struct app_bt_evt_t *event);

int app_bt_init(app_bt_callback_t callback);

int app_bt_send_str(uint32_t con_index, const uint8_t *string, uint16_t len);

#endif
