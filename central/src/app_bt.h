#ifndef __APP_BT_H
#define __APP_BT_H

#include <zephyr/kernel.h>

int app_bt_init(void);

int app_bt_send_str(uint32_t con_index, const uint8_t *string, uint16_t len);

#endif
