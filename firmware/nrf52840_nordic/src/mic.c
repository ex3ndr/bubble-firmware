#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "nrfx_clock.h"
#include "nrfx_pdm.h"
#include "config.h"
#include "mic.h"
#include "utils.h"
#include "led.h"

//
// Port of this code: https://github.com/Seeed-Studio/Seeed_Arduino_Mic/blob/master/src/hardware/nrf52840_adc.cpp
//

static int16_t m_buffer_0[1024];
static int16_t m_buffer_1[1024];
static volatile uint8_t m_buffer_count;

static void pdm_irq_handler(nrfx_pdm_evt_t const *event)
{

    // // Ignore error (how to handle?)
    // if (event->error)
    // {

    //     return;
    // }

    set_led_green(m_buffer_count == 0);
    m_buffer_count = (m_buffer_count + 1) % 2;
}

int mic_start()
{

    // Start the high frequency clock
    if (!nrf_clock_hf_is_running(NRF_CLOCK, NRF_CLOCK_HFCLK_HIGH_ACCURACY))
    {
        nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
    }

    // Configure PDM
    nrfx_pdm_config_t pdm_config = NRFX_PDM_DEFAULT_CONFIG(PDM_CLK_PIN, PDM_DIN_PIN);
    pdm_config.gain_l = MIC_GAIN;
    pdm_config.gain_r = MIC_GAIN;
    pdm_config.interrupt_priority = MIC_IRC_PRIORITY;
    pdm_config.clock_freq = NRF_PDM_FREQ_1280K;
    pdm_config.mode = NRF_PDM_MODE_MONO;
    pdm_config.edge = NRF_PDM_EDGE_LEFTFALLING;
    pdm_config.ratio = NRF_PDM_RATIO_64X;
    IRQ_DIRECT_CONNECT(PDM_IRQn, 5, nrfx_pdm_irq_handler, 0); // IMPORTANT!
    if (nrfx_pdm_init(&pdm_config, pdm_irq_handler) != NRFX_SUCCESS)
    {
        printk("Unable to initialize PDM\n");
        return -1;
    }
    // ASSERT_OK(nrf_nvic_ClearPendingIRQ(PDM_IRQn)); // Handled automatically
    // ASSERT_OK(nrf_nvic_EnableIRQ(PDM_IRQn));       // Handled automatically

    // Power on Mic
    // ASSERT_OK(nrf_gpio_cfg_output(PDM_PWR_PIN));
    // ASSERT_OK(nrf_gpio_pin_set(PDM_PWR_PIN));

    // Start PDM
    if (nrfx_pdm_start() != NRFX_SUCCESS)
    {
        printk("Unable to start PDM\n");
        return -1;
    }
    // nrfx_pdm_event_clear(NRF_PDM_EVENT_STARTED);   // Handled automatically
    // nrfx_pdm_task_trigger(NRF_PDM_TASK_START);     // Handled automatically

    printk("Microphone started\n");

    return 0;
}