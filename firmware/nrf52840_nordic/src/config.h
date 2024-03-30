#include <haly/nrfy_gpio.h>

// #define SAMPLE_RATE 16000
#define MIC_GAIN 20
#define MIC_IRC_PRIORITY 7
#define MIC_BUFFER_SAMPLES 1600    // 100ms
#define AUDIO_BUFFER_SAMPLES 16000 // 1s
#define NETWORK_RING_BUF_SIZE 32 * 1024 // 32KB
#define MINIMAL_PACKET_SIZE 100 // Less than that doesn't make sence to send anything at all

// PIN definitions
// https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino/blob/5aa3573913449410fd60f76b75673c53855ff2ec/variants/Seeed_XIAO_nRF52840_Sense/variant.cpp#L34
#define PDM_DIN_PIN NRF_GPIO_PIN_MAP(0, 16)
#define PDM_CLK_PIN NRF_GPIO_PIN_MAP(1, 0)
#define PDM_PWR_PIN NRF_GPIO_PIN_MAP(1, 10)

// Codecs
#define CODEC_PCM 1
// #define CODEC_MU_LAW 1

// Codec packages
#if CODEC_PCM | CODEC_MU_LAW
#define CODEC_DIVIDER 1
#define CODEC_PACKAGE_SAMPLES 160 // 10ms
#if CODEC_PCM
#define CODEC_OUTPUT_MAX_BYTES CODEC_PACKAGE_SAMPLES * 2
#else
#define CODEC_OUTPUT_MAX_BYTES CODEC_PACKAGE_SAMPLES
#endif
#endif

// Logs
// #define LOG_DISCARDED