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

static void led_blink_work_handler(struct k_work *work);

static led_color_t blink_colors[2];
static uint32_t blink_speed_ms;
static struct k_work_delayable work_led_blink;
static bool led_blink_active = false;

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

	k_work_init_delayable(&work_led_blink, led_blink_work_handler);
	
	return 0;
}

static int led_set_color(led_color_t color)
{
	int ret = sx1509b_led_intensity_pin_set(dev_sx1509b, RED_LED, COLOR_CH_RED(color));
	if (ret == 0) {
		ret = sx1509b_led_intensity_pin_set(dev_sx1509b, GREEN_LED, COLOR_CH_GREEN(color));
	}
	if (ret == 0) {
		ret = sx1509b_led_intensity_pin_set(dev_sx1509b, BLUE_LED, COLOR_CH_BLUE(color));	
	}
	return ret;
}

int app_led_set(led_color_t color)
{
	if(led_blink_active) {
		led_blink_active = false;
		k_work_cancel_delayable(&work_led_blink);
	}

	return led_set_color(color);
}

int app_led_toggle(led_color_t color1)
{
	static bool led_on = true;
	led_on = !led_on;
	return led_set_color(led_on ? color1 : LED_COLOR_BLACK);
}

static void led_blink_work_handler(struct k_work *work)
{
	static bool color_select;
	color_select = !color_select;
	led_set_color(blink_colors[color_select ? 0 : 1]);
	k_work_schedule(k_work_delayable_from_work(work), K_MSEC(blink_speed_ms));
}

int app_led_blink(led_color_t c1, led_color_t c2, led_speed_t speed)
{
	if(led_blink_active) {
		k_work_cancel_delayable(&work_led_blink);
	}

	blink_colors[0] = c1;
	blink_colors[1] = c2;
	blink_speed_ms = (uint32_t)speed;

	led_blink_active = true;	
	return k_work_schedule(&work_led_blink, K_NO_WAIT);
}

int app_led_off(void)
{
	if(led_blink_active) {
		led_blink_active = false;
		k_work_cancel_delayable(&work_led_blink);
	}
	return led_set_color(LED_COLOR_BLACK);
}
