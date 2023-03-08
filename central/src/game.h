#ifndef __GAME_H
#define __GAME_H

#include <zephyr/kernel.h>
#include <app_bt.h>

struct game_t;

typedef void (*game_func_bt_send_t)(uint32_t con_index, const uint8_t *data, uint16_t len);
typedef void (*game_func_bt_per_send_t)(const uint8_t *data, uint16_t len);
typedef void (*game_func_play_t)(struct game_t *game);
typedef void (*game_func_bt_evt_t)(struct game_t *game, struct app_bt_evt_t *bt_evt);

struct game_t {
    game_func_play_t play;
    game_func_bt_evt_t bt_rx;
    game_func_bt_send_t bt_send;
	game_func_bt_per_send_t bt_per_send;
};

#endif
