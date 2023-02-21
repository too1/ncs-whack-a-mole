#include "app_sensors.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>

#if defined(CONFIG_BOARD_THINGY52_NRF52832)
#include <zephyr/drivers/sensor.h>
static const struct device *dev_mpu9250;

static app_sensors_callback_t m_callback;

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

static int process_mpu6050()
{
	struct sensor_value temperature;
	struct sensor_value accel[3];
	struct sensor_value gyro[3];
	int rc = sensor_sample_fetch(dev_mpu9250);

	if (rc == 0) {
		rc = sensor_channel_get(dev_mpu9250, SENSOR_CHAN_ACCEL_XYZ, accel);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev_mpu9250, SENSOR_CHAN_GYRO_XYZ, gyro);
	}
	if (rc == 0) {
		rc = sensor_channel_get(dev_mpu9250, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
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

int app_sensors_init(app_sensors_callback_t callback)
{
	m_callback = callback;

	const char *mpu9250_label = "MPU9250";
	dev_mpu9250 = device_get_binding(mpu9250_label);
	if (!dev_mpu9250) {
		printk("Failed to find sensor %s\n", mpu9250_label);
		return -ENODEV;
	}

	return 0;
}

int app_sensors_read_mpu(void)
{
	process_mpu6050();

	return 0;
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

#endif // defined(CONFIG_BOARD_THINGY52_NRF52832)