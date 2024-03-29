#include "transport.h"

//
// Hidden state 
//

void (*transport_connected)(struct bt_conn *conn, uint8_t err) = NULL;
void (*transport_disconnected)(struct bt_conn *conn, uint8_t err) = NULL;
struct bt_conn *current_connection = NULL;

void _transport_connected(struct bt_conn *conn, uint8_t err) {
    current_connection = conn;
    if (transport_connected) {
        transport_connected(conn, err);
    }
}

void _transport_disconnected(struct bt_conn *conn, uint8_t err) {
    current_connection = NULL;
    if (transport_disconnected) {
        transport_disconnected(conn, err);
    }
}

static struct bt_conn_cb _callback_references = {
	.connected = _transport_connected,
	.disconnected = _transport_disconnected,
};

//
// Public functions
//

int transport_start() {

    // Configure callbacks
    bt_conn_cb_register(&_callback_references);

    // Enable Bluetooth
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return err;
    }
    printk("Bluetooth initialized\n");

	// Start advertising
	bt_gatt_service_register(&audio_service);
    err = bt_le_adv_start(BT_LE_ADV_CONN, bt_ad, ARRAY_SIZE(bt_ad), bt_sd, ARRAY_SIZE(bt_sd));
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return err;
    } else {
    	printk("Advertising successfully started\n");
	}

    return 0;
}

struct bt_conn * get_current_connection() {
    return current_connection;
}