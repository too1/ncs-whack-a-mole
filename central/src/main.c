/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <app_bt.h>
#include <app_bt_per.h>
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

void on_app_bt_per_event(struct app_bt_per_evt_t *event)
{

}

void on_app_bt_event(struct app_bt_evt_t *event)
{
	switch(event->type) {
		case APP_BT_EVT_CON_NUM_CHANGE:
			mygame.bt_rx(&mygame, event);
			break; 
		case APP_BT_EVT_RX_DATA:
			mygame.bt_rx(&mygame, event);
			break;
		case APP_BT_EVT_PER_CONNECTED:
			app_bt_per_connected(event->per_conn);
			break;
		case APP_BT_EVT_PER_DISCONNECTED:
			app_bt_per_disconnected(event->per_conn);
			break; 			
	}
}

void on_game_bt_per_send(const uint8_t *data, uint16_t len)
{
	app_bt_per_send_str(data, len);
}

void on_game_bt_send(uint32_t con_index, const uint8_t *data, uint16_t len)
{
#if 0
	printk("BT TX (cond index %i):", con_index);
	for(int i = 0; i < len; i++) {
		if(data[i] > 32 && data[i] < 128) printk("0x%.2X '%c' ", data[i], data[i]);
		else printk("0x%.2X ", data[i]);
	} 
	printk("\n");
#endif
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

	ret = app_bt_per_init(on_app_bt_per_event);
	if (ret < 0) {
		printk("BT Peripheral init failed!\n");
		return;
	}

	mygame.bt_send = on_game_bt_send;
	mygame.bt_per_send = on_game_bt_per_send;
	whackamole_init(&mygame);

	mygame.play(&mygame);
}
