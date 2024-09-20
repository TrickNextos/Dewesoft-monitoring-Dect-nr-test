/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/drivers/hwinfo.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <inttypes.h>

LOG_MODULE_REGISTER(app);

#define NUM_OF_TX 2000
int num_of_tx_recieved = 0;
int time_since_last_tx_recieved = 0;

uint32_t tx_handle = 0;
uint32_t rx_handle = 1;

BUILD_ASSERT(CONFIG_CARRIER, "Carrier must be configured according to local regulations");

#define DATA_LEN_MAX 32

static bool exit;
static uint16_t device_id;

// is only recieving
bool is_rx = false;
// has sent data at least once (if yes, then it will never recieve)
bool is_sending = false;
// got back data from reciever, so can send again
bool can_send_tx = true;
// if recieving, send statistics to sender
bool send_data = false;

/* Semaphore to synchronize modem calls. */
K_SEM_DEFINE(operation_sem, 0, 1);

/* Header type 1, due to endianness the order is different than in the specification. */
struct phy_ctrl_field_common
{
	uint32_t packet_length : 4;
	uint32_t packet_length_type : 1;
	uint32_t header_format : 3;
	uint32_t short_network_id : 8;
	uint32_t transmitter_id_hi : 8;
	uint32_t transmitter_id_lo : 8;
	uint32_t df_mcs : 3;
	uint32_t reserved : 1;
	uint32_t transmit_power : 4;
	uint32_t pad : 24;
};

/* Send operation. */
static int transmit(uint32_t handle, void *data, size_t data_len)
{
	int err;

	struct phy_ctrl_field_common header = {
		.header_format = 0x0,
		.packet_length_type = 0x0,
		.packet_length = 0x01,
		.short_network_id = (CONFIG_NETWORK_ID & 0xff),
		.transmitter_id_hi = (device_id >> 8),
		.transmitter_id_lo = (device_id & 0xff),
		.transmit_power = CONFIG_TX_POWER,
		.reserved = 0,
		.df_mcs = CONFIG_MCS,
	};

	struct nrf_modem_dect_phy_tx_params tx_op_params = {
		.start_time = 0,
		.handle = handle,
		.network_id = CONFIG_NETWORK_ID,
		.phy_type = 0,
		.lbt_rssi_threshold_max = 0,
		.carrier = CONFIG_CARRIER,
		.lbt_period = NRF_MODEM_DECT_LBT_PERIOD_MAX,
		.phy_header = (union nrf_modem_dect_phy_hdr *)&header,
		.data = data,
		.data_size = data_len,
	};
	err = nrf_modem_dect_phy_tx(&tx_op_params);
	if (err != 0)
	{
		return err;
	}

	return 0;
}

/* Receive operation. */
static int receive(uint32_t handle, uint32_t duration_ms)
{
	int err;

	struct nrf_modem_dect_phy_rx_params rx_op_params = {
		.start_time = 0,
		.handle = handle,
		.network_id = CONFIG_NETWORK_ID,
		.mode = NRF_MODEM_DECT_PHY_RX_MODE_CONTINUOUS,
		.rssi_interval = NRF_MODEM_DECT_PHY_RSSI_INTERVAL_OFF,
		.link_id = NRF_MODEM_DECT_PHY_LINK_UNSPECIFIED,
		.rssi_level = -60,
		.carrier = CONFIG_CARRIER,
		.duration = duration_ms *
					NRF_MODEM_DECT_MODEM_TIME_TICK_RATE_KHZ,
		.filter.short_network_id = CONFIG_NETWORK_ID & 0xff,
		.filter.is_short_network_id_used = 1,
		/* listen for everything (broadcast mode used) */
		.filter.receiver_identity = 0,
	};
	err = nrf_modem_dect_phy_rx(&rx_op_params);
	if (err != 0)
	{
		return err;
	}

	return 0;
}

/* Timers*/
extern void set_rx_or_tx(struct k_timer *timer_id)
{
	is_rx = !is_sending;
}

extern void send_end_data(struct k_timer *timer_id)
{
	send_data = num_of_tx_recieved != 0;
}

K_TIMER_DEFINE(rxtx_timer, set_rx_or_tx, NULL);
K_TIMER_DEFINE(last_msg_timer, send_end_data, NULL);

/* Button and led initialization */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
															  {0});
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
													 {0});

/* Callback after init operation. */
static void init(const uint64_t *time, int16_t temp, enum nrf_modem_dect_phy_err err,
				 const struct nrf_modem_dect_phy_modem_cfg *cfg)
{
	if (err)
	{
		LOG_ERR("Init failed, err %d", err);
		exit = true;
		return;
	}

	k_sem_give(&operation_sem);
}

/* Callback after deinit operation. */
static void deinit(const uint64_t *time, enum nrf_modem_dect_phy_err err)
{
	if (err)
	{
		LOG_ERR("Deinit failed, err %d", err);
		return;
	}

	k_sem_give(&operation_sem);
}

/* Operation complete notification. */
static void op_complete(const uint64_t *time, int16_t temperature,
						enum nrf_modem_dect_phy_err err, uint32_t handle)
{
	LOG_DBG("op_complete cb time %" PRIu64 " status %d", *time, err);
	k_sem_give(&operation_sem);
}

/* Callback after receive stop operation. */
static void rx_stop(const uint64_t *time, enum nrf_modem_dect_phy_err err, uint32_t handle)
{
	LOG_DBG("rx_stop cb time %" PRIu64 " status %d handle %d", *time, err, handle);
	k_sem_give(&operation_sem);
}

/* Physical Control Channel reception notification. */
static void pcc(
	const uint64_t *time,
	const struct nrf_modem_dect_phy_rx_pcc_status *status,
	const union nrf_modem_dect_phy_hdr *hdr)
{
	struct phy_ctrl_field_common *header = (struct phy_ctrl_field_common *)hdr->type_1;
}

/* Physical Control Channel CRC error notification. */
static void pcc_crc_err(const uint64_t *time,
						const struct nrf_modem_dect_phy_rx_pcc_crc_failure *crc_failure)
{
	LOG_DBG("pcc_crc_err cb time %" PRIu64 "", *time);
}

/* Physical Data Channel reception notification. */
static void pdc(const uint64_t *time,
				const struct nrf_modem_dect_phy_rx_pdc_status *status,
				const void *data, uint32_t len)
{
	// if msg is statistics (starts with M)
	if (((char *)data)[0] == 'M')
	{
		if (is_rx)
			return;
		LOG_INF("%s", (char *)data);
		// turn of the led if you are sending, if you are reading the led should still be on
		gpio_pin_set_dt(&led, is_rx);
		can_send_tx = true;
		return;
	}
	if (is_rx)
	{
		num_of_tx_recieved++;
		k_timer_start(&last_msg_timer, K_MSEC(500), K_NO_WAIT);
	}
	// LOG_INF("Received data (RSSI: %d.%d): %d %s",
	// 		(status->rssi_2 / 2), (status->rssi_2 & 0b1) * 5, num_of_tx_recieved, (char *)data);
	// uint8_t *state = (uint8_t *)data;
	// if (len > 0 && (*state == '0' || *state == '1'))
	// {
	// 	gpio_pin_set_dt(&led, *state - '0');
	// }
}

/* Physical Data Channel CRC error notification. */
static void pdc_crc_err(
	const uint64_t *time, const struct nrf_modem_dect_phy_rx_pdc_crc_failure *crc_failure)
{
	LOG_DBG("pdc_crc_err cb time %" PRIu64 "", *time);
}

/* RSSI measurement result notification. */
static void rssi(const uint64_t *time, const struct nrf_modem_dect_phy_rssi_meas *status)
{
	LOG_DBG("rssi cb time %" PRIu64 " carrier %d", *time, status->carrier);
}

/* Callback after link configuration operation. */
static void link_config(const uint64_t *time, enum nrf_modem_dect_phy_err err)
{
	LOG_DBG("link_config cb time %" PRIu64 " status %d", *time, err);
}

/* Callback after time query operation. */
static void time_get(const uint64_t *time, enum nrf_modem_dect_phy_err err)
{
	LOG_DBG("time_get cb time %" PRIu64 " status %d", *time, err);
}

/* Callback after capability get operation. */
static void capability_get(const uint64_t *time, enum nrf_modem_dect_phy_err err,
						   const struct nrf_modem_dect_phy_capability *capability)
{
	LOG_DBG("capability_get cb time %" PRIu64 " status %d", *time, err);
}

/* Dect PHY callbacks. */
static struct nrf_modem_dect_phy_callbacks dect_phy_callbacks = {
	.init = init,
	.deinit = deinit,
	.op_complete = op_complete,
	.rx_stop = rx_stop,
	.pcc = pcc,
	.pcc_crc_err = pcc_crc_err,
	.pdc = pdc,
	.pdc_crc_err = pdc_crc_err,
	.rssi = rssi,
	.link_config = link_config,
	.time_get = time_get,
	.capability_get = capability_get,
};

/* Dect PHY init parameters. */
static struct nrf_modem_dect_phy_init_params dect_phy_init_params = {
	.harq_rx_expiry_time_us = 5000000,
	.harq_rx_process_count = 4,
};

int main(void)
{
	int err;
	/* Button and led setup*/
	if (!gpio_is_ready_dt(&button))
	{
		printk("Error: button device is not ready\n");
		return 0;
	}

	err = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (err != 0)
	{
		printk("Error %d: failed to configure %s pin %d\n",
			   err, button.port->name, button.pin);
		return 0;
	}
	if (led.port && !gpio_is_ready_dt(&led))
	{
		printk("Error %d: LED device %s is not ready; ignoring it\n",
			   err, led.port->name);
		led.port = NULL;
	}
	if (led.port)
	{
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (err != 0)
		{
			printk("Error %d: failed to configure LED device %s pin %d\n",
				   err, led.port->name, led.pin);
			led.port = NULL;
		}
		else
		{
			printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}

	/* DECT NR+ modem initialization */
	uint8_t tx_buf[DATA_LEN_MAX];
	size_t tx_len;

	LOG_INF("Dect NR+ PHY Hello sample started");

	err = nrf_modem_lib_init();
	if (err)
	{
		LOG_ERR("modem init failed, err %d", err);
		return err;
	}

	err = nrf_modem_dect_phy_callback_set(&dect_phy_callbacks);
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_callback_set failed, err %d", err);
		return err;
	}

	err = nrf_modem_dect_phy_init(&dect_phy_init_params);
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_init failed, err %d", err);
		return err;
	}

	k_sem_take(&operation_sem, K_FOREVER);
	if (exit)
	{
		return -EIO;
	}

	hwinfo_get_device_id((void *)&device_id, sizeof(device_id));

	LOG_INF("Dect NR+ PHY initialized, device ID: %d", device_id);

	err = nrf_modem_dect_phy_capability_get();
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_capability_get failed, err %d", err);
	}
	k_timer_start(&rxtx_timer, K_MSEC(10000), K_NO_WAIT);
	while (!is_rx)
	{
		int button_state;
		int prev_button_state = 0;
		button_state = gpio_pin_get_dt(&button);
		if (prev_button_state != button_state && can_send_tx)
		{
			can_send_tx = false;
			prev_button_state = button_state;
			if (button_state != 1)
			{
				continue;
			}
			is_sending = true;

			LOG_INF("Transmitting 0..%d", NUM_OF_TX);
			for (int i = 0; i < NUM_OF_TX; i++)
			{
				gpio_pin_set_dt(&led, (i % 50) < 25);
				tx_len = sprintf(tx_buf, "%d", i);

				err = transmit(tx_handle, tx_buf, tx_len);
				if (err)
				{
					LOG_ERR("Transmisstion failed, err %d", err);
					return err;
				}
				/* Wait for TX operation to complete. */
				k_sem_take(&operation_sem, K_FOREVER);
			}
			gpio_pin_set_dt(&led, 1);
		}

		/** Receiving messages for CONFIG_RX_PERIOD_MS seconds. */
		err = receive(rx_handle, 50);
		if (err)
		{
			LOG_ERR("Reception failed, err %d", err);
			return err;
		}

		/* Wait for RX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
	}
	LOG_INF("Recieving msg!");
	while (1)
	{
		gpio_pin_set_dt(&led, 1);
		/** Receiving messages for CONFIG_RX_PERIOD_MS seconds. */
		err = receive(rx_handle, 5 * MSEC_PER_SEC);
		if (err)
		{
			LOG_ERR("Reception failed, err %d", err);
			return err;
		}

		/* Wait for RX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);

		if (send_data)
		{
			LOG_INF("Messages recieved: %d / %d", num_of_tx_recieved, NUM_OF_TX);
			tx_len = sprintf(&tx_buf, "Messages recieved: %d / %d", num_of_tx_recieved, NUM_OF_TX);
			err = transmit(tx_handle, (void *)tx_buf, tx_len);
			if (err)
			{
				LOG_ERR("Transmition failed, err %d", err);
				return err;
			}

			/* Wait for RX operation to complete. */
			k_sem_take(&operation_sem, K_FOREVER);
			num_of_tx_recieved = 0;
			send_data = false;
		}
	}

	LOG_INF("Shutting down");

	err = nrf_modem_dect_phy_deinit();
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_deinit() failed, err %d", err);
		return err;
	}

	k_sem_take(&operation_sem, K_FOREVER);

	err = nrf_modem_lib_shutdown();
	if (err)
	{
		LOG_ERR("nrf_modem_lib_shutdown() failed, err %d", err);
		return err;
	}

	LOG_INF("Bye!");

	return 0;
}
