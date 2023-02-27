#ifndef __COLOR_H
#define __COLOR_H

#include <zephyr/kernel.h>

#define LED_COLOR_BLACK 	0x000000
#define LED_COLOR_RED		0x0000FF
#define LED_COLOR_ORANGE    0x0088FF
#define LED_COLOR_YELLOW	0x00FFFF
#define LED_COLOR_GREEN		0x00FF00
#define LED_COLOR_BLUE		0xFF0000
#define LED_COLOR_PURPLE	0xFF00FF
#define LED_COLOR_TEAL		0xFFFF00
#define LED_COLOR_WHITE		0xFFFFFF

#define COLOR_CH_RED(a) ((a) & 0xFF)
#define COLOR_CH_GREEN(a) (((a) >> 8) & 0xFF)
#define COLOR_CH_BLUE(a) (((a) >> 16) & 0xFF)

#define COLOR_FROM_BUF(a, offset) ((uint32_t)a[offset] << 16 | (uint32_t)a[offset + 1] << 8 | (uint32_t)a[offset + 2])
#define COLOR_TO_BUF(c, buf, offset) do{buf[offset+2]=((c)&0xFF);buf[offset+1]=((c>>8)&0xFF);buf[offset+0]=((c>>16)&0xFF);}while(0)

#define COLOR_MIX(c1, c2, mix) ( ((((COLOR_CH_RED(c1)   * (mix) + COLOR_CH_RED(c2)   * (256 - (mix))) >> 8) & 0xFF) << 0) | \
                                 ((((COLOR_CH_GREEN(c1) * (mix) + COLOR_CH_GREEN(c2) * (256 - (mix))) >> 8) & 0xFF) << 8) | \
                                 ((((COLOR_CH_BLUE(c1)  * (mix) + COLOR_CH_BLUE(c2)  * (256 - (mix))) >> 8) & 0xFF) << 16) )

typedef enum {LED_SPEED_SLOW = 2, LED_SPEED_NORMAL = 10, LED_SPEED_FAST = 50} led_speed_t;
typedef enum {LED_EFFECT_PULSE, LED_EFFECT_BLINK} led_effect_type_t;

typedef uint32_t led_color_t;

#define LED_REPEAT_INFINITE 0
#define LED_EFFECT_CMD_SIZE 16

typedef struct {
    led_effect_type_t type;
    led_color_t color1;
    led_color_t color2;
    led_color_t color_end;
    uint8_t     speed;
    uint8_t     num_repeats;
} led_effect_cfg_t;

void led_effect_to_cmd(const led_effect_cfg_t *cfg, uint8_t sub_cmd, uint8_t *cmd_buf);

void led_effect_from_cmd(led_effect_cfg_t *cfg, uint8_t *cmd_buf);

void led_effect_to_cmd_to(const led_effect_cfg_t *cfg, uint8_t sub_cmd, uint8_t *cmd_buf, uint16_t timeout);

#endif