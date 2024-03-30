#include <zephyr/sys/ring_buffer.h>
#include "transport.h"
#include "config.h"

//
// Service and Characteristic
//

static struct bt_uuid_128 audio_service_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10000, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
static struct bt_uuid_128 audio_characteristic_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10001, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&audio_service_uuid),
    BT_GATT_CHARACTERISTIC(&audio_characteristic_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, audio_characteristic_read, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)};
static struct bt_gatt_service audio_service = BT_GATT_SERVICE(attrs);

// Advertisement data
static const struct bt_data bt_ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, "Super", sizeof("Super") - 1),
};

// Scan response data
static const struct bt_data bt_sd[] = {
    BT_DATA(BT_DATA_UUID128_ALL, audio_service_uuid.val, sizeof(audio_service_uuid.val)),
};

static ssize_t audio_characteristic_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

//
// Connection Callbacks
//

void (*transport_connected)(struct bt_conn *conn, uint8_t err) = NULL;
void (*transport_disconnected)(struct bt_conn *conn, uint8_t err) = NULL;
struct bt_conn *current_connection = NULL;
uint16_t current_package_index = 0;

void _transport_connected(struct bt_conn *conn, uint8_t err)
{
    printk("err: %d\n", err);
    current_connection = conn;
    printk("Connected\n");
    if (transport_connected)
    {
        transport_connected(conn, err);
    }
}

void _transport_disconnected(struct bt_conn *conn, uint8_t err)
{
    printk("err: %d\n", err);
    current_connection = NULL;
    printk("Disconnected\n");
    if (transport_disconnected)
    {
        transport_disconnected(conn, err);
    }
}

static struct bt_conn_cb _callback_references = {
    .connected = _transport_connected,
    .disconnected = _transport_disconnected,
};

//
// Pusher
//

uint8_t ring_buffer_data[NETWORK_RING_BUF_SIZE];
struct ring_buf ring_buf;
K_THREAD_STACK_DEFINE(pusher_stack, 1024);
static struct k_thread pusher_thread;

void pusher(void)
{
    uint8_t data[180];
    size_t read_size;
    while (1)
    {
        // Read data from ring buffer
        read_size = ring_buf_get(&ring_buf, data, sizeof(data));
        // printk("Read size: %d\n", read_size);

        // Push data to audio characteristic
        if (read_size > 0)
        {
            // Notify
            while (true)
            {

                // Load current connection
                struct bt_conn *conn = get_current_connection();

                // Skip if no connection
                if (!conn)
                {
                    break;
                }

                // Send notification
                int err = bt_gatt_notify(conn, &audio_service.attrs[1], data, read_size);
                if (err == -EAGAIN || err == -ENOMEM)
                {
                    k_sleep(K_MSEC(1));
                    continue;
                }
                if (err)
                {
                    printk("bt_gatt_notify failed (err %d)\n", err);
                }
                break;
            }
        }
        else
        {
            k_sleep(K_MSEC(1));
        }
    }
}

//
// Public functions
//

int transport_start()
{

    // Configure callbacks
    bt_conn_cb_register(&_callback_references);

    // Enable Bluetooth
    int err = bt_enable(NULL);
    if (err)
    {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }
    printk("Bluetooth initialized\n");

    // Start advertising
    bt_gatt_service_register(&audio_service);
    err = bt_le_adv_start(BT_LE_ADV_CONN, bt_ad, ARRAY_SIZE(bt_ad), bt_sd, ARRAY_SIZE(bt_sd));
    if (err)
    {
        printk("Advertising failed to start (err %d)\n", err);
        return err;
    }
    else
    {
        printk("Advertising successfully started\n");
    }

    // Start pusher
    ring_buf_init(&ring_buf, sizeof(ring_buffer_data), ring_buffer_data);
    k_thread_create(&pusher_thread, pusher_stack, K_THREAD_STACK_SIZEOF(pusher_stack), (k_thread_entry_t)pusher, NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);

    return 0;
}

struct bt_conn *get_current_connection()
{
    return current_connection;
}

// https://github.com/zephyrproject-rtos/zephyr/issues/52101
int broadcast_audio_packets(uint8_t *buffer, size_t size)
{
    int written = ring_buf_put(&ring_buf, buffer, size);
    if (written != size)
    {
        printk("Failed to write to ring buffer, written: %d, size: %d\n", written, size);
        return -1;
    }
    return 0;
}