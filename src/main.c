/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const char *now_str(void)
{
	static char buf[16]; /* ...HH:MM:SS.MMM */
	uint32_t now = k_uptime_get_32();
	unsigned int ms = now % MSEC_PER_SEC;
	unsigned int s;
	unsigned int min;
	unsigned int h;

	now /= MSEC_PER_SEC;
	s = now % 60U;
	now /= 60U;
	min = now % 60U;
	now /= 60U;
	h = now;

	snprintf(buf, sizeof(buf), "%u:%02u:%02u.%03u",
		 h, min, s, ms);
	return buf;
}

static int process_mpu6050(const struct device *dev)
{
	struct sensor_value temperature;
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
	int rc = sensor_sample_fetch(dev);

	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_GYRO_XYZ,
					gyro);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP,
					&temperature);
	}
	if (rc == 0) {
		printk("[%s]:%i Cel\n"
		       "  accel %i %i %i m/s/s\n"
		       "  gyro  %i %i %i rad/s\n",
		       now_str(),
		       (int)(100*sensor_value_to_double(&temperature)),
		       (int)(100*sensor_value_to_double(&accel[0])),
		       (int)(100*sensor_value_to_double(&accel[1])),
		       (int)(100*sensor_value_to_double(&accel[2])),
		       (int)(100*sensor_value_to_double(&gyro[0])),
		       (int)(100*sensor_value_to_double(&gyro[1])),
		       (int)(100*sensor_value_to_double(&gyro[2])));
	} else {
		printk("sample fetch/get failed: %d\n", rc);
	}

	return rc;
}

void main(void)
{
	int ret;

	if (!device_is_ready(led.port)) {
		return;
	}

	const char *const label = "MPU9250";
	const struct device *dev_mpu9250 = device_get_binding(label);

	if (!dev_mpu9250) {
		printk("Failed to find sensor %s\n", label);
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}

	printk("Basic Thingy52 sensor sample\n");

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return;
		}

		process_mpu6050(dev_mpu9250);

		k_msleep(SLEEP_TIME_MS);
	}
}

static const struct gpio_dt_spec mpu_pwr = 
	GPIO_DT_SPEC_GET(DT_NODELABEL(mpu_pwr), enable_gpios);

static int mpu_pwr_ctrl_init(const struct device *dev)
{
	if (!device_is_ready(mpu_pwr.port)) {
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&mpu_pwr, GPIO_OUTPUT_HIGH);
	if (ret < 0) {
		return ret;
	}

	k_sleep(K_MSEC(100)); /* Wait for the rail to come up and stabilize */
	
	return 0;
}

SYS_INIT(mpu_pwr_ctrl_init, POST_KERNEL,
	 CONFIG_BOARD_CCS_VDD_PWR_CTRL_INIT_PRIORITY);
