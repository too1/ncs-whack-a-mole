#ifndef __APP_BT_H
#define __APP_BT_H

typedef struct {

} app_bt_event_t;

typedef void (*app_bt_callback_t)(app_bt_event_t *event);

int app_bt_init(app_bt_callback_t callback);

#endif
