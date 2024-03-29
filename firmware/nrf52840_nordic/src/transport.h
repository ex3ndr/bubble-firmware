#pragma once

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/gpio.h>

static struct bt_uuid_128 audio_service_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10000, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
static struct bt_uuid_128 audio_characteristic_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10001, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&audio_service_uuid),
    BT_GATT_CHARACTERISTIC(&audio_characteristic_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, NULL, NULL),
};
static struct bt_gatt_service audio_service = BT_GATT_SERVICE(attrs);

// Advertisement data
static const struct bt_data bt_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, "Super", sizeof("Super") - 1),
};

// Scan response data
static const struct bt_data bt_sd[] = {
// BT_DATA_BYTES(BT_DATA_UUID128_ALL, audio_service_uuid.uuid),
};

// Callbacks
int transport_start();
struct bt_conn* get_current_connection();