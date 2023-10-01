/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */
#include <zephyr/types.h>
#include <zephyr/kernel.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include <bluetooth/services/nus.h>

#include <zephyr/settings/settings.h>

#include <stdio.h>

#include <zephyr/logging/log.h>

#include <SEGGER_RTT.h>

#include "simplest_interpreter.h"


#define LOG_MODULE_NAME app
LOG_MODULE_REGISTER(LOG_MODULE_NAME, CONFIG_LOG_MAX_LEVEL);

#define RTT_IN_BUFSIZE 10 // number of characters to receive at a time in RTT console
char rtt_in_buf[RTT_IN_BUFSIZE]; //console input

// TODO: Create NUS write thread as was earlier
#define STACKSIZE CONFIG_BT_NUS_THREAD_STACK_SIZE
#define PRIORITY 7

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN	(sizeof(DEVICE_NAME) - 1)

static struct bt_conn *current_conn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

struct fifo_program_data_t {
	void *fifo_reserved;
	uint8_t data_byte;
};

static K_FIFO_DEFINE(fifo_program);

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed (err %u)", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s", addr);

	current_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected    = connected,
	.disconnected = disconnected,
};

static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data,
			  uint16_t len)
{
	//int err;
	char addr[BT_ADDR_LE_STR_LEN] = {0};
    uint8_t count = 0;
	struct fifo_program_data_t *buf;
	static char prompt[2];
	prompt[0] = 10; prompt[1] = '>'; prompt[2] = ' ';
	
	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	LOG_INF("Received data from: %s, now will be put in FIFO buffer", addr);
    LOG_HEXDUMP_DBG(data, len, "hexdump of data:");
	
    while (count < len) {
        buf = k_malloc(sizeof(*buf));
		buf->data_byte = data[count];
        k_fifo_put(&fifo_program, buf);
        count++;

        LOG_DBG("data[count-1] = %u", data[count-1]);
		if (data[count-1] == 10) {
			LOG_DBG("Sending prompt in bt_nus_send()");
			LOG_HEXDUMP_DBG(prompt, 3, "promt static variable:");
			if (bt_nus_send(NULL, prompt, 3)) {
				LOG_WRN("Failed to send data over BLE connection");
		    }
		}
    }
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

void error(void)
{
	while (true) {
		/* Spin for ever */
		k_sleep(K_MSEC(1000));
	}
}

int main(void)
{
	int err = 0;
    char c;
    unsigned int n = 0;
	struct fifo_program_data_t *buf;


	printk("Entering main().\n");

    SEGGER_RTT_ConfigDownBuffer(0, "console", rtt_in_buf, sizeof(rtt_in_buf), SEGGER_RTT_MODE_NO_BLOCK_TRIM);
    
	err = bt_enable(NULL);
	if (err) {
		error();
	}

	LOG_INF("Bluetooth initialized");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_nus_init(&nus_cb);
	if (err) {
		LOG_ERR("Failed to initialize UART service (err: %d)", err);
		return 0;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd,
			      ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return 0;
	}

	for (;;) {
    
		n = SEGGER_RTT_Read(0, &c, 1); // read one character from RTT
		if (n) {
			n = 0;
			if (bt_nus_send(NULL, &c, 1)) {
				LOG_WRN("Failed to send data over BLE connection");
		    }
		}

		buf = k_fifo_get(&fifo_program, K_FOREVER);
		if (buf) LOG_DBG("Received in FIFO buffer: %c\n", buf->data_byte);
		if (buf) k_free(buf);
		
		k_sleep(K_MSEC(200));
	}
}
