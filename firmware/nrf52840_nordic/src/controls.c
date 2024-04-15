#include "controls.h"
#include "utils.h"

static button_handler button_cb = NULL;
static struct gpio_callback button_cb_data;
void button_pressed(const struct device *dev, struct gpio_callback *cb,
                    uint32_t pins)
{
    if (button_cb)
    {
        button_cb();
    }
}

int start_controls()
{
    ASSERT_OK(gpio_is_ready_dt(&button));
    ASSERT_OK(gpio_pin_configure_dt(&button, GPIO_INPUT));
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    ASSERT_OK(gpio_add_callback(button.port, &button_cb_data));
    ASSERT_OK(gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE));

    return 0;
}

void set_button_handler(button_handler _handler)
{
    button_cb = _handler;
}