#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>
#include "transport.h"
#include "mic.h"
#include "utils.h"
#include "led.h"

// Main loop
int main(void)
{
	// Transport start
	ASSERT_OK(transport_start());

	// Led start
	ASSERT_OK(led_start());
	set_led_blue(true);
	k_msleep(1000);

	// Mic start
	ASSERT_OK(mic_start());
	
	// Blink LED
	bool is_on = false;
	set_led_red(is_on);
	set_led_blue(false);
	while (1) {
		is_on = !is_on;
		set_led_red(is_on);
		k_msleep(1000);
	}

	// Unreachable
	return 0;
}
