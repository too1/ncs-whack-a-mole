#include <app_led.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_sx1509b.h>
#include <zephyr/device.h>

#define NUMBER_OF_LEDS 3
#define RED_LED DT_GPIO_PIN(DT_NODELABEL(led0), gpios)
#define GREEN_LED DT_GPIO_PIN(DT_NODELABEL(led1), gpios)
#define BLUE_LED DT_GPIO_PIN(DT_NODELABEL(led2), gpios)

const struct device *dev_sx1509b;

static const gpio_pin_t rgb_pins[] = {
	RED_LED,
	GREEN_LED,
	BLUE_LED,
};

int app_led_init(void)
{
	int ret;
	
	dev_sx1509b = DEVICE_DT_GET(DT_NODELABEL(sx1509b));

	if (!device_is_ready(dev_sx1509b)) {
		printk("sx1509b: device not ready.\n");
		return -1;
	}

	for (int i = 0; i < NUMBER_OF_LEDS; i++) {
		ret = sx1509b_led_intensity_pin_configure(dev_sx1509b, rgb_pins[i]);

		if (ret < 0) {
			printk("Error configuring pin for LED intensity\n");
			return ret;
		}
	}

	return 0;
}

int app_led_toggle(led_color_t color1)
{
	static bool led_on = true;
	if(led_on) {
		sx1509b_led_intensity_pin_set(dev_sx1509b, RED_LED, COLOR_CH_RED(color1));
		sx1509b_led_intensity_pin_set(dev_sx1509b, GREEN_LED, COLOR_CH_GREEN(color1));
		sx1509b_led_intensity_pin_set(dev_sx1509b, BLUE_LED, COLOR_CH_BLUE(color1));
	} else {
		sx1509b_led_intensity_pin_set(dev_sx1509b, RED_LED, 0);
		sx1509b_led_intensity_pin_set(dev_sx1509b, GREEN_LED, 0);
		sx1509b_led_intensity_pin_set(dev_sx1509b, BLUE_LED, 0);
	}
	led_on = !led_on;
	return 0;
}
