/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <app_sensors.h>
#include <app_bt.h>
#include <app_led.h>
#include <string.h>
#include <stdio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

#define BUTTON0_NODE DT_NODELABEL(button0)

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);

static struct {
	bool trial_started;
	uint32_t start_time;
	uint32_t time; 
} m_trial_data = {0};

void trial_done_func(struct k_work *work)
{
	static uint8_t rsp_string[32];
	app_led_set(LED_COLOR_BLACK);
	printk("Trial completed in %i milliseconds\n", m_trial_data.time);
	sprintf(rsp_string, "TD:%.6i", m_trial_data.time);
	app_bt_send(rsp_string, strlen(rsp_string));
}
K_WORK_DEFINE(m_work_trial_done, trial_done_func);

void on_button_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	if(m_trial_data.trial_started) {
		m_trial_data.time = k_uptime_get_32() - m_trial_data.start_time;
		m_trial_data.trial_started = false;		
		k_work_submit(&m_work_trial_done);
	}
}

static int button_led_init(void)
{
	int ret;

	ret = app_led_init();
	if (ret < 0) {
		printk("LED init error (err %i)", ret);
		return ret;
	}

	if (!device_is_ready(button.port)) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret < 0) {
		return -1;
	}
	
	ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret != 0) {
		printk("Error %d: failed to configure interrupt on %s pin %d\n",
			ret, button.port->name, button.pin);
		return ret;
	}
	
	static struct gpio_callback button_callback;
	gpio_init_callback(&button_callback, on_button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_callback);

	return 0;
}

void sensors_callback(app_sensors_event_t *event)
{

}

void bluetooth_callback(app_bt_event_t *event)
{
	switch(event->type) {
		case APP_BT_EVT_CONNECTED:
			printk("Bluetooth connected\n");
			app_led_set(LED_COLOR_BLACK);
			break;
		case APP_BT_EVT_DISCONNECTED:
			printk("Bluetooth disconnected\n");
			app_led_blink(LED_COLOR_BLUE, LED_COLOR_BLACK, LED_SPEED_NORMAL);
			break;
		case APP_BT_EVT_RX:
			printk("Data received!!\n");
			if(!m_trial_data.trial_started) {
				app_led_set(LED_COLOR_PURPLE);
				m_trial_data.trial_started = true;
				m_trial_data.start_time = k_uptime_get_32();
			}
			break;
	}
}

void main(void)
{
	int ret;

	ret = button_led_init();
	if (ret < 0) {
		printk("Button/LED init failed\n");
		return;
	}

#if defined(CONFIG_BOARD_THINGY52_NRF52832)
	ret = app_sensors_init(sensors_callback);
	if (ret < 0) {
		printk("Sensors init failed (err %d)\n", ret);
		return;
	}
#endif

	ret = app_bt_init(bluetooth_callback);
	if (ret < 0) {
		printk("Bluetooth init failed (err %i)", ret);
		return;
	}
	
	printk("Basic Thingy52 sensor sample\n");

	app_led_blink(LED_COLOR_BLUE, LED_COLOR_BLACK, LED_SPEED_NORMAL);

	while (1) {
		k_msleep(SLEEP_TIME_MS);
	}
}
