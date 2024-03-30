#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>

// Internal
static ssize_t audio_characteristic_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);

// Callbacks
int transport_start();
struct bt_conn *get_current_connection();
int broadcast_audio_packets(uint8_t *buffer, size_t size);
