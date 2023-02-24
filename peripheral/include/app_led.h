#ifndef __APP_LED_H
#define __APP_LED_H

#include <zephyr/kernel.h>
#include <color.h>


int app_led_init(void);

int app_led_set_effect(led_effect_cfg_t *cfg);

int app_led_set(led_color_t color);

int app_led_toggle(led_color_t color);

int app_led_blink(led_color_t c1, led_color_t c2, led_speed_t speed);

int app_led_off(void);

#endif
