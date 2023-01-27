#ifndef __APP_LED_H
#define __APP_LED_H

typedef enum {LED_COLOR_BLACK, LED_COLOR_RED} app_led_color_t;

int app_led_init(void);

int app_led_toggle(app_led_color_t color1);

#endif
