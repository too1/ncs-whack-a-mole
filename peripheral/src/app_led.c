#include <app_led.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <string.h>

#define RED_LED DT_GPIO_PIN(DT_NODELABEL(led0), gpios)
#define GREEN_LED DT_GPIO_PIN(DT_NODELABEL(led1), gpios)
#define BLUE_LED DT_GPIO_PIN(DT_NODELABEL(led2), gpios)

#if defined(CONFIG_BOARD_THINGY52_NRF52832)	
#include <zephyr/drivers/gpio/gpio_sx1509b.h>
const struct device *dev_sx1509b;
static const gpio_pin_t rgb_pins[] = {
	RED_LED,
	GREEN_LED,
	BLUE_LED,
};
#else
#include <zephyr/drivers/pwm.h>
static const struct pwm_dt_spec pwm_leds[3] = {PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led0)), PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led1)), PWM_DT_SPEC_GET(DT_NODELABEL(pwm_led2))};
#endif

#define NUMBER_OF_LEDS 3

struct {
	led_effect_cfg_t config;
	uint32_t runtime;
	uint32_t repeats_left;
	bool 	infinite;
	bool 	active;
} m_led_effect_status = {0};

static void led_blink_work_handler(struct k_work *work);

static led_color_t blink_colors[2];
static uint32_t blink_speed_ms;
static struct k_work_delayable work_led_blink;
static bool led_blink_active = false;

int app_led_init(void)
{
	int ret = 0;

#if defined(CONFIG_BOARD_THINGY52_NRF52832)	
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
#else
	for (int i = 0; i < NUMBER_OF_LEDS; i++) {
		if (!device_is_ready(pwm_leds[i].dev)) {
			printk("PWM device %i is not ready!\n", i);
			return -ENODEV;
		}
	}
#endif 

	k_work_init_delayable(&work_led_blink, led_blink_work_handler);
	
	return ret;
}

static int led_set_color(led_color_t color)
{
	int ret = 0;
#if defined(CONFIG_BOARD_THINGY52_NRF52832)
	ret = sx1509b_led_intensity_pin_set(dev_sx1509b, RED_LED, COLOR_CH_RED(color));
	if (ret == 0) {
		ret = sx1509b_led_intensity_pin_set(dev_sx1509b, GREEN_LED, COLOR_CH_GREEN(color));
	}
	if (ret == 0) {
		ret = sx1509b_led_intensity_pin_set(dev_sx1509b, BLUE_LED, COLOR_CH_BLUE(color));	
	}
#else 
	ret = pwm_set_dt(&pwm_leds[0], 255, COLOR_CH_RED(color));
	if (ret == 0) {
		ret = pwm_set_dt(&pwm_leds[1], 255, COLOR_CH_GREEN(color));
	}
	if (ret == 0) {
		ret = pwm_set_dt(&pwm_leds[2], 255, COLOR_CH_BLUE(color));
	}
#endif
	return ret;
}

int app_led_set(led_color_t color)
{
	if(led_blink_active || m_led_effect_status.active) {
		led_blink_active = false;
		m_led_effect_status.active = false;
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

int app_led_set_effect(led_effect_cfg_t *cfg)
{
	memcpy(&m_led_effect_status.config, cfg, sizeof(led_effect_cfg_t));
	m_led_effect_status.runtime = 0;
	m_led_effect_status.active = true;
	if(cfg->num_repeats == LED_REPEAT_INFINITE) {
		m_led_effect_status.infinite = true;
	}
	blink_speed_ms = 20;
	return k_work_schedule(&work_led_blink, K_NO_WAIT);
}

static void led_blink_work_handler(struct k_work *work)
{
	if(m_led_effect_status.active) {
		uint32_t new_color;
		uint32_t mix_factor = m_led_effect_status.runtime % 512 + 1;
		if(mix_factor <= 256) {
			new_color = COLOR_MIX(m_led_effect_status.config.color1, m_led_effect_status.config.color2, mix_factor);
		}
		else {
			new_color = COLOR_MIX(m_led_effect_status.config.color1, m_led_effect_status.config.color2, 512 - mix_factor);
		}
		led_set_color(new_color);
		m_led_effect_status.runtime += m_led_effect_status.config.speed;
		
		// Check if the effect should expire
		if(!m_led_effect_status.infinite && (m_led_effect_status.runtime / 512) >= m_led_effect_status.config.num_repeats) {
			led_set_color(m_led_effect_status.config.color_end);
			m_led_effect_status.active = false;
			return;
		}
	}
	else {
		static bool color_select;
		color_select = !color_select;
		led_set_color(blink_colors[color_select ? 0 : 1]);
	}
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
	if(led_blink_active || m_led_effect_status.active) {
		led_blink_active = false;
		m_led_effect_status.active = false;
		k_work_cancel_delayable(&work_led_blink);
	}
	return led_set_color(LED_COLOR_BLACK);
}
