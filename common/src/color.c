#include <color.h>

void led_effect_to_cmd(const led_effect_cfg_t *cfg, uint8_t sub_cmd, uint8_t *cmd_buf)
{
    cmd_buf[0] = 'L';
    cmd_buf[1] = sub_cmd;
    cmd_buf[2] = 'P';
    COLOR_TO_BUF(cfg->color1, cmd_buf, 3);
    COLOR_TO_BUF(cfg->color2, cmd_buf, 6);
    COLOR_TO_BUF(cfg->color_end, cmd_buf, 9);
    cmd_buf[12] = cfg->speed;
    cmd_buf[13] = cfg->num_repeats;
}