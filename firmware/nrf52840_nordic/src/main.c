#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/gpio.h>
#include "transport.h"
#include "mic.h"
#include "utils.h"
#include "led.h"
#include "config.h"
#include "audio.h"
#include "codec.h"
#include "camera.h"
#include "controls.h"
#include "battery.h"

// State
bool is_recording = false;
bool is_connected = false;
bool is_charging = false;
void refresh_state_indication();

//
// Transport callbacks
//

static void transport_subscribed()
{
	is_connected = true;
#ifndef ENABLE_BUTTON
	is_recording = true;
	mic_resume();
#endif
	refresh_state_indication();
}

static void transport_unsubscribed()
{
	is_connected = false;
#ifndef ENABLE_BUTTON
	is_recording = false;
	mic_pause();
#endif
	refresh_state_indication();
}

static struct transport_cb transport_callbacks = {
	.subscribed = transport_subscribed,
	.unsubscribed = transport_unsubscribed,
};

//
// Button
//

#ifdef ENABLE_BUTTON
static void on_button_pressed()
{
	is_recording = !is_recording;
	set_allowed(is_recording);
	if (is_recording)
	{
		mic_resume();
	}
	else
	{
		mic_pause();
	}
	refresh_state_indication();
}
#endif

//
// Audio Pipeline
//

static void codec_handler(uint8_t *data, size_t len)
{
	broadcast_audio_packets(data, len); // Errors are logged inside
}

static void mic_handler(int16_t *buffer)
{
	codec_receive_pcm(buffer, MIC_BUFFER_SAMPLES); // Errors are logged inside
}

//
// LED indication
//

void refresh_state_indication()
{
	// Recording and connected state - BLUE
	if (is_recording && is_connected)
	{
		set_led_red(false);
		set_led_green(false);
		set_led_blue(true);
		return;
	}

	// Recording but lost connection - RED
	if (is_recording && !is_connected)
	{
		set_led_red(true);
		set_led_green(false);
		set_led_blue(false);
		return;
	}

	// Conencted, but not recording - BLINK BLUE
	// if (is_connected && !is_recording)
	// {
	// 	set_led_red(false);
	// 	set_led_green(false);
	// 	set_led_blue(tick);
	// 	return;
	// }

	// Not recording, but charging - WHITE
	if (is_charging)
	{
		set_led_red(true);
		set_led_green(true);
		set_led_blue(true);
		return;
	}

	// Not recording - OFF
	set_led_red(false);
	set_led_green(false);
	set_led_blue(false);
}

//
// Main
//

int main(void)
{
	// Start watchdog
	int err;
	struct wdt_timeout_cfg wdt_config;
	const struct device *wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt));
	wdt_config.flags = WDT_FLAG_RESET_SOC;
	wdt_config.window.min = 0;
	wdt_config.window.max = WDT_TIMEOUT_MS;
	wdt_config.callback = NULL; // Set to NULL to cause a system reset
	ASSERT_OK(wdt_install_timeout(wdt_dev, &wdt_config));

	// Led start
	ASSERT_OK(led_start());

	// Battery start
	ASSERT_OK(battery_start());

	// Camera start
#ifdef ENABLE_CAMERA
	ASSERT_OK(camera_start());
#endif

	// Transport start
	set_transport_callbacks(&transport_callbacks);
	ASSERT_OK(transport_start());
#ifndef ENABLE_BUTTON
	set_allowed(true);
#endif

	// Controls
#ifdef ENABLE_BUTTON
	set_button_handler(on_button_pressed);
	ASSERT_OK(start_controls());
#endif

	// Codec start
	set_codec_callback(codec_handler);
	ASSERT_OK(codec_start());

	// Mic start
	set_mic_callback(mic_handler);
	ASSERT_OK(mic_start());

	// Set LED
	is_charging = is_battery_charging();
	refresh_state_indication();

	// Main loop
	ASSERT_OK(wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG));
	while (1)
	{
		// Wait wdt
		k_msleep(WDT_FEED_MS);

		// Watchdog
		wdt_feed(wdt_dev, 0);

		// Update battery state
		bool charging = is_battery_charging();
		if (charging != is_charging)
		{
			is_charging = charging;
			refresh_state_indication();
		}
	}

	// Unreachable
	return 0;
}
