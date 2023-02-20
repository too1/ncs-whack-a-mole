/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <app_bt.h>

void main(void)
{
	int ret;
	ret = app_bt_init();
	if (ret < 0) {
		printk("BT init failed!\n");
		return;
	}
}
