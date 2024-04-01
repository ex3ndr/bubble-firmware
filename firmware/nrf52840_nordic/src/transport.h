#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/l2cap.h>
#include <zephyr/drivers/gpio.h>

// Internal
static struct bt_conn_cb _callback_references;
static struct bt_l2cap_chan_ops _callback_l2cap_references;
static ssize_t audio_characteristic_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t audio_characteristic_format_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t audio_channel_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len);
static void ccc_config_changed_handler(const struct bt_gatt_attr *attr, uint16_t value);

// Callbacks
int transport_start();
int broadcast_audio_packets(uint8_t *buffer, size_t size);
