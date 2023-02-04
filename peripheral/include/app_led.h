#ifndef __APP_LED_H
#define __APP_LED_H

#include <zephyr/kernel.h>

#define LED_COLOR_BLACK 	0x000000
#define LED_COLOR_RED		0x0000FF
#define LED_COLOR_YELLOW	0x00FFFF
#define LED_COLOR_GREEN		0x00FF00
#define LED_COLOR_BLUE		0xFF0000
#define LED_COLOR_PURPLE	0xFF00FF
#define LED_COLOR_TEAL		0xFFFF00
#define LED_COLOR_WHITE		0xFFFFFF

#define COLOR_CH_RED(a) ((a) & 0xFF)
#define COLOR_CH_GREEN(a) (((a) >> 8) & 0xFF)
#define COLOR_CH_BLUE(a) (((a) >> 16) & 0xFF)

typedef uint32_t led_color_t;

typedef enum {LED_SPEED_FAST = 100, LED_SPEED_NORMAL = 250, LED_SPEED_SLOW = 1000} led_speed_t;

int app_led_init(void);

int app_led_toggle(led_color_t color);

int app_led_blink(led_color_t c1, led_color_t c2, led_speed_t speed);

int app_led_off(void);

#endif
