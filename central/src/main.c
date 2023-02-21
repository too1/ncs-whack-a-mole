/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <app_bt.h>
#include <dk_buttons_and_leds.h>

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	uint32_t changed_and_pressed = button_state & has_changed;
	
	// If button 1 pressed
	if(changed_and_pressed & DK_BTN1_MSK) {
		app_bt_send_str(0, "test", 4);
	}
}

void main(void)
{
	int ret;

	ret = dk_buttons_init(button_changed);
	if (ret < 0) {
		printk("Cannot init buttons (err: %d)\n", ret);
		return;
	}

	ret = app_bt_init();
	if (ret < 0) {
		printk("BT init failed!\n");
		return;
	}

	while (1) {
		k_msleep(10000);
		ret = app_bt_send_str(0, "timer", 5);
		if (ret == 0) printk("Send BT success\n");
		else printk("Send BT error: %i\n", ret);
	}
}
