/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <app_sensors.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#define BUTTON0_NODE DT_NODELABEL(button0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);

void on_button_pressed(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	
}

static int button_led_init(void)
{
	int ret;

	if (!device_is_ready(led.port)) {
		return -1;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return -1;
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
		return;
	}
	static struct gpio_callback button_callback;
	gpio_init_callback(&button_callback, on_button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_callback);

	return 0;
}

void sensors_callback(app_sensors_event_t *event)
{

}

void main(void)
{
	int ret;

	ret = button_led_init();
	if (ret < 0) {
		printk("Button/LED init failed\n");
		return;
	}

	ret = app_sensors_init(sensors_callback);
	if (ret < 0) {
		printk("Sensors init failed\n");
		return;
	}
	
	printk("Basic Thingy52 sensor sample\n");

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}

		app_sensors_read_mpu();

		k_msleep(SLEEP_TIME_MS);
	}
}
