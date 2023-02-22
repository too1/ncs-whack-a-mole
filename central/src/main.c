/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <app_bt.h>
#include <dk_buttons_and_leds.h>
#include <game_whackamole.h>

static struct game_t mygame;

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	uint32_t changed_and_pressed = button_state & has_changed;
	
	// If button 1 pressed
	if(changed_and_pressed & DK_BTN1_MSK) {
		app_bt_send_str(0, "test", 4);
	}
}

void on_app_bt_event(struct app_bt_evt_t *event)
{
	mygame.bt_rx(&mygame, event);
}

void on_game_bt_send(uint32_t con_index, const uint8_t *data, uint16_t len)
{
	printk("BT TX (cond index %i): %.*s\n", con_index, len, data);
	app_bt_send_str(con_index, data, len);
}

void main(void)
{
	int ret;

	ret = dk_buttons_init(button_changed);
	if (ret < 0) {
		printk("Cannot init buttons (err: %d)\n", ret);
		return;
	}

	ret = app_bt_init(on_app_bt_event);
	if (ret < 0) {
		printk("BT init failed!\n");
		return;
	}

	mygame.bt_send = on_game_bt_send;
	whackamole_init(&mygame);

	mygame.play(&mygame);
}
