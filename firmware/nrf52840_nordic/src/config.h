#include <haly/nrfy_gpio.h>

// #define SAMPLE_RATE 16000
#define MIC_GAIN 20
#define MIC_IRC_PRIORITY 7
#define MIC_BUFFER_SAMPLES 1600 // 100ms
#define AUDIO_BUFFER_SAMPLES 16000 // 1s
#define NETWORK_RING_BUF_SIZE 4 * 1024

// PIN definitions
// https://github.com/Seeed-Studio/Adafruit_nRF52_Arduino/blob/5aa3573913449410fd60f76b75673c53855ff2ec/variants/Seeed_XIAO_nRF52840_Sense/variant.cpp#L34
#define PDM_DIN_PIN NRF_GPIO_PIN_MAP(0, 16)
#define PDM_CLK_PIN NRF_GPIO_PIN_MAP(1, 0)
#define PDM_PWR_PIN NRF_GPIO_PIN_MAP(1, 10)