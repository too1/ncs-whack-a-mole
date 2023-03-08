/* main.c - Application main entry point */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <app_bt.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus.h>
#include <bluetooth/services/nus_client.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_bt, LOG_LEVEL_DBG);

#define SCAN_INTERVAL 0x00A0 /* 160 ms */
#define SCAN_WINDOW   0x0030 /* 30 ms */
#define INIT_INTERVAL 0x0010 /* 10 ms */
#define INIT_WINDOW   0x0010 /* 10 ms */
#define CONN_INTERVAL 40     /* 50 ms */
#define CONN_LATENCY  0
#define CONN_TIMEOUT  MIN(MAX((CONN_INTERVAL * 125 * \
			       MAX(CONFIG_BT_MAX_CONN, 6) / 1000), 10), 3200)

//#define LOG_DISABLE 1

static const char adv_target_name[] = "Whack-A-Mole Button";
static bool 	  adv_target_name_found;

static void start_scan(void);
static void gatt_discover(struct bt_conn *conn, struct bt_nus_client *nus_client);

static app_bt_callback_t m_callback;

static struct bt_conn *conn_connecting;
static struct bt_conn_info conn_info;
static uint8_t volatile conn_count;
static bool volatile is_disconnecting;

static struct per_context_t {
	bool used;
	bool ready;
	struct bt_conn *conn;
	struct bt_nus_client nus_client;
	uint32_t index;
} per_context[CONFIG_BT_MAX_CONN] = {0};

static struct per_context_t *get_free_per_context(void)
{
	for(int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if(!per_context[i].used) {
			per_context[i].used = true;
			return &per_context[i];
		}
	}
	return NULL;
}

static struct per_context_t *get_per_context_from_conn(struct bt_conn *conn)
{
	for(int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if(per_context[i].used && per_context[i].conn == conn) {
			return &per_context[i];
		}
	}
	return NULL;
}

static struct per_context_t *get_per_context_from_client(struct bt_nus_client *client)
{
	for(int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		if(per_context[i].used && &per_context[i].nus_client == client) {
			return &per_context[i];
		}
	}
	return NULL;
}

static void fwd_event_con_num_change(uint32_t con_num)
{
	static struct app_bt_evt_t evt = {.type = APP_BT_EVT_CON_NUM_CHANGE};
	evt.num_connected = con_num;
	m_callback(&evt);
}

static void fwd_event_rx_data(uint32_t con_index, const uint8_t *data, uint16_t len)
{
	static struct app_bt_evt_t rx_evt = {.type = APP_BT_EVT_RX_DATA};
	rx_evt.con_index = con_index;
	rx_evt.data = data;
	rx_evt.data_len = len;
	m_callback(&rx_evt);
}

static void fwd_event_per_link_con_discon(struct bt_conn *conn, bool connected)
{
	static struct app_bt_evt_t per_con_discon_evt;
	per_con_discon_evt.type = (connected ? APP_BT_EVT_PER_CONNECTED : APP_BT_EVT_PER_DISCONNECTED);
	per_con_discon_evt.per_conn = conn;
	m_callback(&per_con_discon_evt);
}

static bool target_adv_name_found(struct bt_data *data, void *user_data)
{
	if (data->type == BT_DATA_NAME_COMPLETE) {
		//LOG_INF("Dev with name found: %.*s", data->data_len, data->data);
		if(memcmp(data->data, adv_target_name, strlen(adv_target_name)) == 0) {
			adv_target_name_found = true;
		}
		return false;
	}
	return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	struct bt_conn_le_create_param create_param = {
		.options = BT_CONN_LE_OPT_NONE,
		.interval = INIT_INTERVAL,
		.window = INIT_WINDOW,
		.interval_coded = 0,
		.window_coded = 0,
		.timeout = 0,
	};
	struct bt_le_conn_param conn_param = {
		.interval_min = CONN_INTERVAL,
		.interval_max = CONN_INTERVAL,
		.latency = CONN_LATENCY,
		.timeout = CONN_TIMEOUT,
	};
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;

	if (conn_connecting) {
		return;
	}

	/* We're only interested in connectable events */
	if (type != BT_GAP_ADV_TYPE_ADV_IND &&
	    type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND &&
	    type != BT_GAP_ADV_TYPE_EXT_ADV) {
		return;
	}

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
	//LOG_INF("Device found: %s (RSSI %d)", addr_str, rssi);

	// Look for the target device name in the advertise payload, and return if it is not found
	adv_target_name_found = false;
	bt_data_parse(ad, target_adv_name_found, 0);
	if (!adv_target_name_found) {
		return;
	}

	if (bt_le_scan_stop()) {
		LOG_INF("Scanning successfully stopped");
		return;
	}

	err = bt_conn_le_create(addr, &create_param, &conn_param,
				&conn_connecting);
	if (err) {
		LOG_WRN("Create conn to %s failed (%d)", addr_str, err);
		start_scan();
	}
}

static void start_scan(void)
{
	struct bt_le_scan_param scan_param = {
		.type       = BT_HCI_LE_SCAN_PASSIVE,
		.options    = BT_LE_SCAN_OPT_NONE,
		.interval   = SCAN_INTERVAL,
		.window     = SCAN_WINDOW,
	};
	int err;

	err = bt_le_scan_start(&scan_param, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return;
	}

	LOG_INF("Scanning successfully started");
}

#if defined(CONFIG_BT_GATT_CLIENT)
static void mtu_exchange_cb(struct bt_conn *conn, uint8_t err,
			    struct bt_gatt_exchange_params *params)
{
	LOG_INF("MTU exchange %u %s (%u)", bt_conn_index(conn),
	       err == 0U ? "successful" : "failed", bt_gatt_get_mtu(conn));
	struct per_context_t *peripheral = get_per_context_from_conn(conn);
	gatt_discover(conn, &peripheral->nus_client);
}

static struct bt_gatt_exchange_params mtu_exchange_params[CONFIG_BT_MAX_CONN];

static int mtu_exchange(struct bt_conn *conn)
{
	uint8_t conn_index;
	int err;

	conn_index = bt_conn_index(conn);

	LOG_INF("MTU (%u): %u", conn_index, bt_gatt_get_mtu(conn));

	mtu_exchange_params[conn_index].func = mtu_exchange_cb;

	err = bt_gatt_exchange_mtu(conn, &mtu_exchange_params[conn_index]);
	if (err) {
		LOG_ERR("MTU exchange failed (err %d)", err);
	} else {
		LOG_DBG("Exchange pending...");
	}

	return err;
}
#endif /* CONFIG_BT_GATT_CLIENT */

static void discovery_complete(struct bt_gatt_dm *dm,
			       void *context)
{
	struct bt_nus_client *nus = context;

	LOG_INF("Service discovery completed");

	bt_gatt_dm_data_print(dm);

	bt_nus_handles_assign(dm, nus);
	bt_nus_subscribe_receive(nus);

	bt_gatt_dm_data_release(dm);

	start_scan();

	struct per_context_t *peripheral = get_per_context_from_client(nus);
	if (peripheral) {
		peripheral->ready = true;
		fwd_event_con_num_change(conn_count);
	}
}

static void discovery_service_not_found(struct bt_conn *conn,
					void *context)
{
	LOG_WRN("Service not found!");
}

static void discovery_error(struct bt_conn *conn,
			    int err,
			    void *context)
{
	LOG_ERR("Error while discovering GATT database: (%d)", err);
}

struct bt_gatt_dm_cb discovery_cb = {
	.completed         = discovery_complete,
	.service_not_found = discovery_service_not_found,
	.error_found       = discovery_error,
};

static void gatt_discover(struct bt_conn *conn, struct bt_nus_client *nus_client)
{
	int err;

	/*if (conn != default_conn) {
		return;
	}*/

	err = bt_gatt_dm_start(conn,
			       BT_UUID_NUS_SERVICE,
			       &discovery_cb,
			       nus_client);
	if (err) {
		LOG_ERR("could not start the discovery procedure, error code: %d", err);
		start_scan();
	}
}

static void connected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_conn_get_info(conn, &conn_info);
	// One of the peripherals/thingy's connected
	if (conn_info.role == BT_CONN_ROLE_CENTRAL) {
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

		if (reason) {
			LOG_ERR("Failed to connect to %s (%u)", addr, reason);

			bt_conn_unref(conn_connecting);
			conn_connecting = NULL;

			start_scan();
			return;
		}

		conn_connecting = NULL;

		conn_count++;
		
		if (conn_count < CONFIG_BT_MAX_CONN) {
			//start_scan(); // Try to skip this, in order to speed up the connection procedure
		}

		struct per_context_t *peripheral = get_free_per_context();
		if (peripheral) {
			peripheral->conn = conn;
			peripheral->ready = false;
		}

		LOG_DBG("Connected (%u): %s", conn_count, addr);

#if defined(CONFIG_BT_SMP)
		int err = bt_conn_set_security(conn, BT_SECURITY_L2);

		if (err) {
			LOG_ERR("Failed to set security (%d).", err);
		}
#endif

#if defined(CONFIG_BT_GATT_CLIENT)
		mtu_exchange(conn);
#endif
	}
	// The central/phone connected
	else {
		fwd_event_per_link_con_discon(conn, true);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_conn_get_info(conn, &conn_info);

	// One of the peripherals/thingy's disconnected
	if (conn_info.role == BT_CONN_ROLE_CENTRAL) {
		bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

		bt_conn_unref(conn);

		struct per_context_t *peripheral = get_per_context_from_conn(conn);
		if (peripheral) {
			peripheral->used = false;
		} 

		if ((conn_count == 1U) && is_disconnecting) {
			is_disconnecting = false;
			start_scan();
		}
		conn_count--;

		LOG_INF("Disconnected (count %i): %s (reason 0x%02x)", conn_count, addr, reason);

		fwd_event_con_num_change(conn_count);
	}
	// The central (phone) disconnected
	else {
		fwd_event_per_link_con_discon(conn, false);
	}
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_DBG("LE conn param req: %s int (0x%04x, 0x%04x) lat %d to %d",
	       addr, param->interval_min, param->interval_max, param->latency,
	       param->timeout);

	return true;
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_DBG("LE conn param updated: %s int 0x%04x lat %d to %d",
	       addr, interval, latency, timeout);
}

#if defined(CONFIG_BT_SMP)
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_ERR("Security failed: %s level %u err %d", addr, level,
		       err);
	}
}
#endif

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static void le_phy_updated(struct bt_conn *conn,
			   struct bt_conn_le_phy_info *param)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("LE PHY Updated: %s Tx 0x%x, Rx 0x%x", addr, param->tx_phy,
	       param->rx_phy);
}
#endif /* CONFIG_BT_USER_PHY_UPDATE */

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
static void le_data_len_updated(struct bt_conn *conn,
				struct bt_conn_le_data_len_info *info)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Data length updated: %s max tx %u (%u us) max rx %u (%u us)",
	       addr, info->tx_max_len, info->tx_max_time, info->rx_max_len,
	       info->rx_max_time);
}
#endif /* CONFIG_BT_USER_DATA_LEN_UPDATE */

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_req = le_param_req,
	.le_param_updated = le_param_updated,

#if defined(CONFIG_BT_SMP)
	.security_changed = security_changed,
#endif

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	.le_phy_updated = le_phy_updated,
#endif /* CONFIG_BT_USER_PHY_UPDATE */

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	.le_data_len_updated = le_data_len_updated,
#endif /* CONFIG_BT_USER_DATA_LEN_UPDATE */
};

static void disconnect(struct bt_conn *conn, void *data)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		LOG_ERR("Failed disconnection %s.", addr);
	}
	else {
		LOG_DBG("Disconnecting %s... success.", addr);
	}
}

static uint8_t nus_data_received(struct bt_nus_client *nus, const uint8_t *data, uint16_t len)
{
	struct per_context_t *peripheral = get_per_context_from_client(nus);
	LOG_INF("BT RX (con ind %i): %.*s", peripheral->index, len, data);
	fwd_event_rx_data(peripheral->index, data, len);
	return BT_GATT_ITER_CONTINUE;
}
static volatile int packets_queued = 0;
static void nus_data_sent(struct bt_nus_client *nus, uint8_t err, const uint8_t *const data, uint16_t len)
{
	packets_queued--;
}

int app_bt_init(app_bt_callback_t callback)
{
	int err;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	m_callback = callback;

	LOG_INF("Bluetooth initialized");

	struct bt_nus_client_init_param init = {
		.cb = {
			.received = nus_data_received,
			.sent = nus_data_sent,
		}
	};

	for(int i = 0; i < CONFIG_BT_MAX_CONN; i++) {
		per_context[i].index = i;
		err = bt_nus_client_init(&(per_context[i].nus_client), &init);
		if (err) {
			LOG_ERR("NUS Client initialization failed (err %d)", err);
			return err;
		}
	}

	bt_conn_cb_register(&conn_callbacks);

	start_scan();

	return 0;
}

int app_bt_send_str(uint32_t con_index, const uint8_t *string, uint16_t len)
{
	int ret;
	if (con_index >= CONFIG_BT_MAX_CONN) {
		return -EINVAL;
	}
	if (per_context[con_index].ready) {
		ret = bt_nus_client_send(&(per_context[con_index].nus_client), string, len);
		if (ret < 0) {
			LOG_ERR("ERROR sending to client %i: %i. PQ %i", con_index, ret, packets_queued);
			return ret;
		}
		packets_queued++;
	}
	else return -EBUSY;
	return 0;
}

void app_bt_disconnect_all(void)
{
	LOG_DBG("Disconnecting all...");
	is_disconnecting = true;
	bt_conn_foreach(BT_CONN_TYPE_LE, disconnect, NULL);

	while (is_disconnecting) {
		k_sleep(K_MSEC(10));
	}
	LOG_DBG("All disconnected.");
}

