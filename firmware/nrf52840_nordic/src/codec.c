#include <zephyr/sys/ring_buffer.h>
#include "codec.h"
#include "audio.h"
#include "config.h"

//
// Output
//

static volatile codec_callback _callback = NULL;

void set_codec_callback(codec_callback callback)
{
    _callback = callback;
}

//
// Input
//

uint8_t codec_ring_buffer_data[AUDIO_BUFFER_SAMPLES * 2]; // 2 bytes per sample
struct ring_buf codec_ring_buf;

int codec_receive_pcm(int16_t *data, size_t len)
{
    int written = ring_buf_put(&codec_ring_buf, (uint8_t *)data, len * 2);
    if (written != len * 2)
    {
        printk("Failed to write %d bytes to codec ring buffer\n", len * 2);
        return -1;
    }
    return 0;
}

//
// Thread
//

int16_t codec_input_samples[CODEC_PACKAGE_SAMPLES];
uint8_t codec_output_bytes[CODEC_OUTPUT_MAX_BYTES];
K_THREAD_STACK_DEFINE(codec_stack, 1024);
static struct k_thread codec_thread;
uint16_t execute_codec();
void codec_entry()
{
    uint16_t output_size;
    while (1)
    {

        // Check if we have enough data
        if (ring_buf_size_get(&codec_ring_buf) < CODEC_PACKAGE_SAMPLES * 2)
        {
            k_sleep(K_MSEC(10));
            continue;
        }

        // Read package
        ring_buf_get(&codec_ring_buf, (uint8_t *)codec_input_samples, CODEC_PACKAGE_SAMPLES * 2);

        // Run Codec
        output_size = execute_codec();

        // Notify
        if (_callback)
        {
            _callback(codec_output_bytes, output_size);
        }
    }
}

int codec_start()
{
    ring_buf_init(&codec_ring_buf, sizeof(codec_ring_buffer_data), codec_ring_buffer_data);
    k_thread_create(&codec_thread, codec_stack, K_THREAD_STACK_SIZEOF(codec_stack), (k_thread_entry_t)codec_entry, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
    return 0;
}

//
// MU-Law codec
//

#if CODEC_MU_LAW

uint16_t execute_codec()
{
    for (int i = 0; i < CODEC_PACKAGE_SAMPLES / CODEC_DIVIDER; i++)
    {
        codec_output_bytes[i] = linear2ulaw(codec_input_samples[i * CODEC_DIVIDER]);
    }
    return CODEC_PACKAGE_SAMPLES / CODEC_DIVIDER;
}

#endif

//
// PCM codec
//

#if CODEC_PCM

uint16_t execute_codec()
{
    for (int i = 0; i < CODEC_PACKAGE_SAMPLES / CODEC_DIVIDER; i++)
    {
        codec_output_bytes[i * 2] = codec_input_samples[i * CODEC_DIVIDER] & 0xFF;
        codec_output_bytes[i * 2 + 1] = (codec_input_samples[i * CODEC_DIVIDER] >> 8) & 0xFF;
    }
    return (CODEC_PACKAGE_SAMPLES / CODEC_DIVIDER) * 2;
}
#endif