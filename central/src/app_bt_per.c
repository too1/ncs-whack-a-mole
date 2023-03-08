#include <app_bt_per.h>
#include <zephyr/types.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <bluetooth/services/nus.h>

#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME app_bt_per
LOG_MODULE_REGISTER(LOG_MODULE_NAME, LOG_LEVEL_DBG);

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static struct bt_conn *current_conn = 0;

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	LOG_DBG("Data received, %i bytes", len);
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

int app_bt_per_connected(struct bt_conn *conn)
{
	LOG_INF("Connected!");
	current_conn = conn;
	return 0;
}

int app_bt_per_disconnected(struct bt_conn *conn)
{
	LOG_INF("Disconnected!");
	current_conn = 0;
	return 0;
}

int app_bt_per_init(app_bt_per_callback_t callback)
{
	int err = bt_nus_init(&nus_cb);
	if (err) {
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return err;
	}

	LOG_DBG("App BT Peripheral initialized");

	return 0;
}

int app_bt_per_send_str(const uint8_t *string, uint16_t len)
{	
	int err;
	if (current_conn == 0) {
		LOG_WRN("No peripheral connected. Ignoring peripheral TX");
		return -ENOTCONN;
	}
	err = bt_nus_send(current_conn, string, len); 
	if (err < 0) {
		LOG_WRN("Failed to send data over BLE connection");
		return err;
	}
	return 0;
}
