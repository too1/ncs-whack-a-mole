#include <app_bt.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

static app_bt_callback_t m_callback;

static app_bt_event_t m_event;

static struct bt_conn *current_conn;

static void trigger_app_callback(app_bt_evt_type_t type)
{
	m_event.type = type;
	m_callback(&m_event);
}

static void bt_connected(struct bt_conn *conn, uint8_t err)
{
	current_conn = conn;
	trigger_app_callback(APP_BT_EVT_CONNECTED);
}

static void bt_disconnected(struct bt_conn *conn, uint8_t reason)
{
	current_conn = NULL;
	trigger_app_callback(APP_BT_EVT_DISCONNECTED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = bt_connected,
	.disconnected = bt_disconnected,
};

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	m_event.type = APP_BT_EVT_RX;
	m_event.buf = data;
	m_event.length = (uint32_t)len;
	m_callback(&m_event);
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

int app_bt_init(app_bt_callback_t callback)
{
	int ret;

	m_callback = callback;

	ret = bt_enable(NULL);
	if (ret < 0) {
		return ret;
	}
	

	ret = bt_nus_init(&nus_cb);
	if (ret < 0) {
		return ret;
	}

	ret = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int app_bt_send(const uint8_t *data, uint16_t len)
{
	return bt_nus_send(current_conn, data, len);
}
