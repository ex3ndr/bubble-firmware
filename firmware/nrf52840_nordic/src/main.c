#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "transport.h"
#include "mic.h"
#include "utils.h"
#include "led.h"
#include "config.h"
#include "audio.h"
#include "codec.h"
#include "camera.h"

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(DT_ALIAS(sw0), gpios, {0});

static void codec_handler(uint8_t *data, size_t len)
{
	broadcast_audio_packets(data, len); // Errors are logged inside
}

static void mic_handler(int16_t *buffer)
{
	codec_receive_pcm(buffer, MIC_BUFFER_SAMPLES); // Errors are logged inside
}

void bt_ctlr_assert_handle(char *name, int type)
{
	if (name != NULL)
	{
		printk("Bt assert-> %s", name);
	}
}

// Main loop
int main(void)
{
	// Button
	if (!gpio_is_ready_dt(&button))
	{
		printk("Error: button device %s is not ready\n",
			   button.port->name);
		return 0;
	}

	int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (ret != 0)
	{
		printk("Error %d: failed to configure %s pin %d\n",
			   ret, button.port->name, button.pin);
		return 0;
	}

	// Led start
	ASSERT_OK(led_start());
	set_led_blue(true);

	// Camera start
#ifdef ENABLE_CAMERA
	ASSERT_OK(camera_start());
#endif

	// Transport start
	ASSERT_OK(transport_start());

	// Codec start
	set_codec_callback(codec_handler);
	ASSERT_OK(codec_start());

	// Mic start
	set_mic_callback(mic_handler);
	ASSERT_OK(mic_start());

	// Blink LED
	bool is_on = true;
	set_led_blue(false);
	set_led_red(is_on);
	while (1)
	{
		is_on = !is_on;
		printk("Button: %d\n", gpio_pin_get_dt(&button));
		// if (gpio_pin_get(gpio0_port, 3) == 0)
		// {
		// 	set_led_red(is_on);
		// 	set_led_green(false);
		// } else {
		// 	set_led_red(false);
		// 	set_led_green(is_on);
		// }
		set_led_red(is_on);

		k_msleep(500);

#ifdef ENABLE_CAMERA
		take_photo();
#endif
	}

	// Unreachable
	return 0;
}
