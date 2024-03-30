#include <zephyr/kernel.h>
#include "transport.h"
#include "mic.h"
#include "utils.h"
#include "led.h"
#include "config.h"
#include "audio.h"

// Mic callback
uint8_t mic_encoded[AUDIO_BUFFER_SAMPLES];
#define DIVIDER 4
static void mic_callback(int16_t *buffer)
{
	for (int i = 0; i < MIC_BUFFER_SAMPLES / DIVIDER; i++)
	{
		mic_encoded[i] = linear2ulaw(buffer[i * DIVIDER]);
	}
	broadcast_audio_packets(mic_encoded, MIC_BUFFER_SAMPLES);
}

// Main loop
int main(void)
{
	// Transport start
	ASSERT_OK(transport_start());

	// Led start
	ASSERT_OK(led_start());

	// Mic start
	set_mic_callback(mic_callback);
	ASSERT_OK(mic_start());

	// Blink LED
	bool is_on = false;
	set_led_red(is_on);
	while (1)
	{
		is_on = !is_on;
		set_led_red(is_on);
		k_msleep(1000);
	}

	// Unreachable
	return 0;
}
