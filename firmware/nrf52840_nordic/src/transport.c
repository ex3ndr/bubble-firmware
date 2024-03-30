#include <zephyr/sys/ring_buffer.h>
#include "transport.h"
#include "config.h"
#include "utils.h"

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
uint16_t current_mtu = 0;
uint16_t current_package_index = 0;

void _transport_connected(struct bt_conn *conn, uint8_t err)
{
    printk("err: %d\n", err);
    struct bt_conn_info info = {0};
    err = bt_conn_get_info(conn, &info);
    if (err)
    {
        printk("Failed to get connection info (err %d)\n", err);
        return;
    }

    current_connection = bt_conn_ref(conn);
    current_mtu = info.le.data_len->tx_max_len;
    printk("Connected\n");
    printk("Interval: %d, latency: %d, timeout: %d\n", info.le.interval, info.le.latency, info.le.timeout);
    printk("TX PHY %s, RX PHY %s\n", phy2str(info.le.phy->tx_phy), phy2str(info.le.phy->rx_phy));
    printk("LE data len updated: TX (len: %d time: %d) RX (len: %d time: %d)\n", info.le.data_len->tx_max_len, info.le.data_len->tx_max_time, info.le.data_len->rx_max_len, info.le.data_len->rx_max_time);

    if (transport_connected)
    {
        transport_connected(conn, err);
    }
}

void _transport_disconnected(struct bt_conn *conn, uint8_t err)
{
    printk("err: %d\n", err);
    printk("Disconnected\n");
    if (current_connection)
    {
        bt_conn_unref(current_connection);
        transport_disconnected(current_connection, err);
    }
}

static bool _le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
    printk("Connection parameters update request received.\n");
    printk("Minimum interval: %d, Maximum interval: %d\n",
           param->interval_min, param->interval_max);
    printk("Latency: %d, Timeout: %d\n", param->latency, param->timeout);

    return true;
}

static void _le_param_updated(struct bt_conn *conn, uint16_t interval,
                              uint16_t latency, uint16_t timeout)
{
    printk("Connection parameters updated.\n"
           " interval: %d, latency: %d, timeout: %d\n",
           interval, latency, timeout);
}

static void _le_phy_updated(struct bt_conn *conn,
                            struct bt_conn_le_phy_info *param)
{
    printk("LE PHY updated: TX PHY %s, RX PHY %s\n",
           phy2str(param->tx_phy), phy2str(param->rx_phy));
}

static void _le_data_length_updated(struct bt_conn *conn,
                                    struct bt_conn_le_data_len_info *info)
{
    printk("LE data len updated: TX (len: %d time: %d)"
           " RX (len: %d time: %d)\n",
           info->tx_max_len,
           info->tx_max_time, info->rx_max_len, info->rx_max_time);
    current_mtu = info->tx_max_len;
}

static struct bt_conn_cb _callback_references = {
    .connected = _transport_connected,
    .disconnected = _transport_disconnected,
    .le_param_req = _le_param_req,
    .le_param_updated = _le_param_updated,
    .le_phy_updated = _le_phy_updated,
    .le_data_len_updated = _le_data_length_updated,
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
    uint8_t pusher_temp_data[251];
    size_t read_size;
    while (1)
    {

        // Load current connection
        struct bt_conn *conn = current_connection;
        if (conn)
        {
            conn = bt_conn_ref(conn);
        }
        bool valid = true;
        if (current_mtu < MINIMAL_PACKET_SIZE)
        {
            valid = false;
        }
        else if (!conn)
        {
            valid = false;
        }
        else
        {
            valid = bt_gatt_is_subscribed(conn, &audio_service.attrs[1], BT_GATT_CCC_NOTIFY); // Check if subscribed
        }

        if (!valid)
        {

#ifdef LOG_DISCARDED
            printk("Discarded %d, current MTU: %d\n", ring_buf_size_get(&ring_buf), current_mtu);
#endif

            // Just discard data
            ring_buf_reset(&ring_buf);
            k_sleep(K_MSEC(10));
            if (conn)
            {
                bt_conn_unref(conn);
            }
            continue;
        }

        // Read data from ring buffer
        read_size = ring_buf_get(&ring_buf, pusher_temp_data, current_mtu);
        if (read_size <= 0) // Should not happen, but anyway
        {
            k_sleep(K_MSEC(50));
            if (conn)
            {
                bt_conn_unref(conn);
            }
            continue;
        }

        // Notify
        while (true)
        {

            // Try send notification
            // printk("Sending %d bytes, wq: %d\n", read_size, sys_dlist_len(&k_sys_work_q.notifyq.waitq));
            int err = bt_gatt_notify(conn, &audio_service.attrs[1], pusher_temp_data, read_size);

            // Log failure
            if (err)
            {
                printk("bt_gatt_notify failed (err %d)\n", err);
                printk("MTU: %d, read_size: %d\n", current_mtu, read_size);
                k_sleep(K_MSEC(50));
            }
            else
            {
                // printk("Sent %d bytes\n", read_size);
            }

            // Try to send more data if possible
            if (err == -EAGAIN || err == -ENOMEM)
            {
                continue;
            }

            // Break if success
            bt_conn_unref(conn);
            break;
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
    k_thread_create(&pusher_thread, pusher_stack, K_THREAD_STACK_SIZEOF(pusher_stack), (k_thread_entry_t)pusher, NULL, NULL, NULL, K_PRIO_PREEMPT(7), 0, K_NO_WAIT);

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
        printk("Failed to write to network ring buffer, written: %d, size: %d\n", written, size);
        return -1;
    }
    return 0;
}