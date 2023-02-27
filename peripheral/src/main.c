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

void challenge_timeout_func(struct k_timer *timer_id); 
K_TIMER_DEFINE(m_timer_challenge_timeout, challenge_timeout_func, NULL);

void trial_done_func(struct k_work *work)
{
	static uint8_t rsp_string[32];
	app_led_set(LED_COLOR_BLACK);
	printk("Trial completed in %i milliseconds\n", m_trial_data.time);
	sprintf(rsp_string, "TD:%.6i", m_trial_data.time);
	app_bt_send(rsp_string, strlen(rsp_string));
}

void trial_timeout_func(struct k_work *work)
{
	app_led_set(LED_COLOR_BLACK);
	app_bt_send("TO", 2);
}

void send_ping_func(struct k_work *work)
{
	app_bt_send("PING", 4);
}

K_WORK_DEFINE(m_work_trial_done, trial_done_func);
K_WORK_DEFINE(m_work_trial_timeout, trial_timeout_func);
K_WORK_DEFINE(m_work_send_ping, send_ping_func);

void on_button_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	if(m_trial_data.trial_started) {
		m_trial_data.time = k_uptime_get_32() - m_trial_data.start_time;
		m_trial_data.trial_started = false;		
		k_work_submit(&m_work_trial_done);
	}
	else {
		k_work_submit(&m_work_send_ping);
	}
}

void challenge_timeout_func(struct k_timer *timer_id)
{
	if (m_trial_data.trial_started) {
		m_trial_data.trial_started = false;
		k_work_submit(&m_work_trial_timeout);
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
			m_trial_data.trial_started = false;
			break;
		case APP_BT_EVT_DISCONNECTED:
			printk("Bluetooth disconnected\n");
			app_led_blink(LED_COLOR_BLUE, LED_COLOR_BLACK, 250);
			break;
		case APP_BT_EVT_RX:
#if 0
			printk("BT RX:");
			for(int i = 0; i < event->length; i++) {
				if(event->buf[i] > 32 && event->buf[i] < 128) printk("0x%.2X '%c' ", event->buf[i], event->buf[i]);
				else printk("0x%.2X ", event->buf[i]);
			} 
			printk("\n");
#endif
			// LED command 
			// L - Start trial (0/1) - Led mode (P/B) - Color 1 RGB - Color 2 RGB - Color End RGB - Speed - Num repeats (255 - infinite)
			// 0 - 1                 - 2              - 3 4 5       -  6 7 8      - 9 10 11       - 12    - 13      
			if(event->buf[0] == 'L' && event->length >= 14) {
				led_effect_cfg_t led_effect;
				// Check if a new challenge/trial should be started
				if(event->buf[1] == '1' && !m_trial_data.trial_started) {
					m_trial_data.trial_started = true;
					m_trial_data.start_time = k_uptime_get_32();
				}
				// Check if a new challenge/trial with timeout should be started
				else if(event->buf[1] == '2' && !m_trial_data.trial_started && event->length >= 16) {
					m_trial_data.trial_started = true;
					m_trial_data.start_time = k_uptime_get_32();

					uint16_t timeout_ms = (uint16_t)event->buf[14] << 8 | (uint16_t)event->buf[15];
					k_timer_start(&m_timer_challenge_timeout, K_MSEC(timeout_ms), K_MSEC(0));
				}
				// Check if we should blink the LED
				if(event->buf[2] == 'P') {
					led_effect.type = LED_EFFECT_PULSE;
					led_effect.color1 = COLOR_FROM_BUF(event->buf, 3);
					led_effect.color2 = COLOR_FROM_BUF(event->buf, 6);
					led_effect.color_end = COLOR_FROM_BUF(event->buf, 9);
					led_effect.speed = event->buf[12];
					led_effect.num_repeats = event->buf[13];
					app_led_set_effect(&led_effect);
				}
			}
			// Reset command
			else if(memcmp(event->buf, "RST", 3) == 0) {
				app_led_off();
				m_trial_data.trial_started = false;
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

	app_led_blink(LED_COLOR_BLUE, LED_COLOR_BLACK, 250);

	while (1) {
		k_msleep(SLEEP_TIME_MS);
	}
}
