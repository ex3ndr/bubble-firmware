#pragma once
#include <zephyr/kernel.h>

void codec_receive_pcm(int16_t *data, size_t len);

int codec_start();