#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/l2cap.h>
#include "transport.h"
#include "config.h"
#include "utils.h"

//
// Internal
//

static struct bt_conn_cb _callback_references;
#ifdef ENABLE_L2CAP
static struct bt_l2cap_chan_ops _callback_l2cap_references;
#endif
static ssize_t audio_characteristic_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t audio_characteristic_format_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
#ifdef ENABLE_L2CAP
static ssize_t audio_channel_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
#endif
static void ccc_config_changed_handler(const struct bt_gatt_attr *attr, uint16_t value);
#ifdef ENABLE_L2CAP
static int _l2cap_accept(struct bt_conn *conn, struct bt_l2cap_server *server, struct bt_l2cap_chan **chan);
#endif

//
// Service and Characteristic
//

static struct bt_uuid_128 audio_service_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10000, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
static struct bt_uuid_128 audio_characteristic_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10001, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
static struct bt_uuid_128 audio_characteristic_format_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10002, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
#ifdef ENABLE_L2CAP
static struct bt_uuid_128 audio_stream_uuid = BT_UUID_INIT_128(BT_UUID_128_ENCODE(0x19B10003, 0xE8F2, 0x537E, 0x4F6C, 0xD104768A1214));
#endif
static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&audio_service_uuid),
    BT_GATT_CHARACTERISTIC(&audio_characteristic_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, audio_characteristic_read, NULL, NULL),
    BT_GATT_CCC(ccc_config_changed_handler, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&audio_characteristic_format_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, audio_characteristic_format_read, NULL, NULL),
#ifdef ENABLE_L2CAP
    BT_GATT_CHARACTERISTIC(&audio_stream_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, audio_channel_read, NULL, NULL),
#endif
};
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

#ifdef ENABLE_L2CAP
static struct bt_l2cap_chan_ops _callback_l2cap_references;
static struct bt_l2cap_server _l2cap_server = {
    .psm = 0,
    .sec_level = BT_SECURITY_L1,
    .accept = _l2cap_accept};
#endif
//
// State and Characteristics
//

struct bt_conn *current_connection = NULL;
uint16_t current_mtu = 0;
uint16_t current_package_index = 0;
#ifdef ENABLE_L2CAP
struct bt_l2cap_chan current_l2cap_channel = {};
bool current_l2cap_ready = false;
bool current_l2cap_used = false;
uint16_t current_l2cap_psm = 0;
#endif

static ssize_t audio_characteristic_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    printk("audio_characteristic_read\n");
    return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t audio_characteristic_format_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    printk("audio_characteristic_format_read\n");
    uint8_t value[1] = {CODEC_ID};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(value));
}

#ifdef ENABLE_L2CAP
static ssize_t audio_channel_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset)
{
    printk("audio_channel_read %d\n", _l2cap_server.psm);
    uint16_t value[1] = {_l2cap_server.psm};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(value));
}
#endif

static void ccc_config_changed_handler(const struct bt_gatt_attr *attr, uint16_t value)
{
    if (value == BT_GATT_CCC_NOTIFY)
    {
        printk("Client subscribed for notifications\n");
    }
    else if (value == 0)
    {
        printk("Client unsubscribed from notifications\n");
    }
    else
    {
        printk("Invalid CCC value: %u\n", value);
    }
}

//
// Connection Callbacks
//

static void _transport_connected(struct bt_conn *conn, uint8_t err)
{
    struct bt_conn_info info = {0};
    err = bt_conn_get_info(conn, &info);
    if (err)
    {
        printk("Failed to get connection info (err %d)\n", err);
        return;
    }

    current_connection = bt_conn_ref(conn);
    current_mtu = info.le.data_len->tx_max_len;
#ifdef ENABLE_L2CAP
    current_l2cap_ready = false;
    current_l2cap_psm = 0;
#endif
    printk("Connected\n");
    printk("Interval: %d, latency: %d, timeout: %d\n", info.le.interval, info.le.latency, info.le.timeout);
    printk("TX PHY %s, RX PHY %s\n", phy2str(info.le.phy->tx_phy), phy2str(info.le.phy->rx_phy));
    printk("LE data len updated: TX (len: %d time: %d) RX (len: %d time: %d)\n", info.le.data_len->tx_max_len, info.le.data_len->tx_max_time, info.le.data_len->rx_max_len, info.le.data_len->rx_max_time);
}

static void _transport_disconnected(struct bt_conn *conn, uint8_t err)
{
    printk("Disconnected\n");
    bt_conn_unref(conn);
    current_connection = NULL;
#ifdef ENABLE_L2CAP
    current_l2cap_ready = false;
    current_l2cap_psm = 0;
#endif
    current_mtu = 0;
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

#ifdef ENABLE_L2CAP

static int _l2cap_accept(struct bt_conn *conn, struct bt_l2cap_server *server, struct bt_l2cap_chan **chan)
{
    if (current_l2cap_used)
    {
        printk("L2CAP channel already in use\n");
        return -ENOMEM;
    }

    printk("Incoming L2CAP channel connection\n");
    memset(&current_l2cap_channel, 0, sizeof(current_l2cap_channel));
    current_l2cap_channel.ops = &_callback_l2cap_references;
    *chan = &current_l2cap_channel;
    current_l2cap_ready = false;
    current_l2cap_used = true;

    return 0;
}

static void _l2cap_connected(struct bt_l2cap_chan *chan)
{
    printk("L2CAP channel connected\n");
    current_l2cap_ready = true;
}

static void _l2cap_disconnected(struct bt_l2cap_chan *chan)
{
    printk("L2CAP channel disconnected\n");
    current_l2cap_ready = false;
    current_l2cap_used = false;
}

static int _l2cap_received(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
    printk("Received data: %d\n", buf->len);
    net_buf_unref(buf);
    return 0;
}

static void _l2cap_status(struct bt_l2cap_chan *chan, atomic_t *status)
{
    printk("L2CAP status: %ld\n", atomic_get(status));
}

#endif

static struct bt_conn_cb _callback_references = {
    .connected = _transport_connected,
    .disconnected = _transport_disconnected,
    .le_param_req = _le_param_req,
    .le_param_updated = _le_param_updated,
    .le_phy_updated = _le_phy_updated,
    .le_data_len_updated = _le_data_length_updated,
};

#ifdef ENABLE_L2CAP
static struct bt_l2cap_chan_ops _callback_l2cap_references = {
    .connected = _l2cap_connected,
    .disconnected = _l2cap_disconnected,
    .recv = _l2cap_received,
    .status = _l2cap_status,
};
#endif

//
// Pusher
//

static uint8_t ring_buffer_data[NETWORK_RING_BUF_SIZE];
static struct ring_buf ring_buf;
K_THREAD_STACK_DEFINE(pusher_stack, 1024);
static struct k_thread pusher_thread;
NET_BUF_POOL_DEFINE(pool, 10, CONFIG_BT_CTLR_DATA_LENGTH_MAX, 0, NULL);
static uint8_t pusher_temp_data[CONFIG_BT_CTLR_DATA_LENGTH_MAX];
static size_t read_size;

static bool push_to_gatt(struct bt_conn *conn)
{
    // Read data from ring buffer
    read_size = ring_buf_get(&ring_buf, pusher_temp_data, current_mtu);
    if (read_size <= 0) // Should not happen, but anyway
    {
        return false;
    }

    // Notify
    while (true)
    {

        // Try send notification
        int err = bt_gatt_notify(conn, &audio_service.attrs[1], pusher_temp_data, read_size);

        // Log failure
        if (err)
        {
            printk("bt_gatt_notify failed (err %d)\n", err);
            printk("MTU: %d, read_size: %d\n", current_mtu, read_size);
            k_sleep(K_MSEC(50));
        }

        // Try to send more data if possible
        if (err == -EAGAIN || err == -ENOMEM)
        {
            continue;
        }

        // Break if success
        break;
    }

    return true;
}

#ifdef ENABLE_L2CAP
bool push_to_l2cap()
{
    printk("Pushing to L2CAP@0\n");

    // Read data from ring buffer
    read_size = ring_buf_get(&ring_buf, pusher_temp_data, current_mtu);
    if (read_size <= 0) // Should not happen, but anyway
    {
        return false;
    }

    // Allocate buffer
    printk("Pushing to L2CAP@1\n");
    struct net_buf *buf = net_buf_alloc(&pool, K_FOREVER);
    if (!buf)
    {
        printk("Failed to allocate buffer\n");
        return false;
    }
    net_buf_add_mem(buf, pusher_temp_data, read_size);

    // Send data
    printk("Pushing to L2CAP@2\n");
    int err = bt_l2cap_chan_send(&current_l2cap_channel, buf);
    if (err)
    {
        printk("Failed to send data (err %d)\n", err);
    }

    return true;
}
#endif

void pusher(void)
{
    while (1)
    {

        //
        // Load current connection
        //

        struct bt_conn *conn = current_connection;
        bool use_gatt = true;
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
#ifdef ENABLE_L2CAP
        else if (!current_l2cap_ready)
        {
            valid = false;
        }
#endif
        else
        {
            valid = bt_gatt_is_subscribed(conn, &audio_service.attrs[1], BT_GATT_CCC_NOTIFY); // Check if subscribed
        }

        // If no valid mode exists - discard whole buffer
        if (!valid)
        {
            ring_buf_reset(&ring_buf);
            k_sleep(K_MSEC(10));
        }

        // Handle GATT
        if (use_gatt && valid)
        {
            bool sent = push_to_gatt(conn);
            if (!sent)
            {
                k_sleep(K_MSEC(50));
            }
        }

// Handle L2CAP
#ifdef ENABLE_L2CAP
        if (!use_gatt && valid)
        {
            bool sent = push_to_l2cap(current_l2cap_channel);
            if (!sent)
            {
                k_sleep(K_MSEC(50));
            }
        }
#endif

        if (conn)
        {
            bt_conn_unref(conn);
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

#ifdef ENABLE_L2CAP
    // Register L2CAP server
    err = bt_l2cap_server_register(&_l2cap_server);
    if (err)
    {
        printk("L2CAP server registration failed (err %d)\n", err);
        return err;
    }
    else
    {
        printk("L2CAP server registered\n");
    }
#endif

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

int broadcast_audio_packets(uint8_t *buffer, size_t size)
{
    int written = ring_buf_put(&ring_buf, buffer, size);
    if (written != size)
    {
        return -1;
    }
    return 0;
}