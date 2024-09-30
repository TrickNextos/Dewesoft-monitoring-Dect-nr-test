/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <string.h>
#include <inttypes.h>
#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dr_test);

#define TX_HANDLE 0
#define RX_HANDLE 1
#define MAX_MCS 4
#define MAX_NUM_OF_SUBSLOTS 6
#define MAX_DATA_LEN 4832
#define SUBSLOTS_USED 6

const int mcs_subslots_size[MAX_MCS + 1][MAX_NUM_OF_SUBSLOTS + 1] = {
	{0,
	 136,
	 264,
	 400,
	 536,
	 664,
	 792},
	{32,
	 296,
	 552,
	 824,
	 1096,
	 1352,
	 1608},
	{56,
	 456,
	 856,
	 1256,
	 1640,
	 2024,
	 2360},
	{88,
	 616,
	 1128,
	 1672,
	 2168,
	 2680,
	 3192},
	{144,
	 936,
	 1736,
	 2488,
	 3256,
	 4024,
	 4832}};

// if this is the error, include overlay-eu.proj/overlay-us.proj into build
BUILD_ASSERT(CONFIG_CARRIER, "Carrier must be configured according to local regulations");

static bool exit;
static uint16_t device_id;

enum TestStatus
{
	NotRunning,
	Scheduled,
	Running,
	Ended,
};

// is only recieving
static bool is_rx = false;
// has sent data at least once (if yes, then it will never recieve)
static bool has_sent = false;
static enum TestStatus current_test_status = NotRunning;
static int num_of_tx_recieved = 0;
static int current_mcs = 0;
int tx_buf[MAX_DATA_LEN];

bool respond_to_test_start_as_rx = false;
bool send_statistics_back = false;

static int devices_in_test = 0;
static int responses_received = 0;

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

enum TestHeaderType
{
	// send by tx node to start test
	ScheduleTest,
	// rx answers to ScheduleTest so tx knows how many participants
	StartTest,
	Testing,
	// tx sends to signal end of test
	EndTest,
	// rx responds with results
	TestResults,
};

/* Send operation. */
static int transmit(uint32_t handle, void *data, size_t data_len, int mcs)
{
	int err;

	struct phy_ctrl_field_common header = {
		.header_format = 0x0,
		.packet_length_type = 0x0,
		.packet_length = SUBSLOTS_USED,
		.short_network_id = (CONFIG_NETWORK_ID & 0xff),
		.transmitter_id_hi = (device_id >> 8),
		.transmitter_id_lo = (device_id & 0xff),
		.transmit_power = CONFIG_TX_POWER,
		.reserved = 0,
		.df_mcs = mcs,
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
	is_rx = !has_sent;
}

K_TIMER_DEFINE(rxtx_timer, set_rx_or_tx, NULL);

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
	gpio_pin_set_dt(&led, 0);
	enum TestHeaderType header_type = (enum TestHeaderType)((int *)data)[0];
	// LOG_INF("L: %d", len);
	if (len == 600 || num_of_tx_recieved == 600)
	{

		for (int j = 0; j < len - 8; j += 8)
		{
			int i = len - j;
			printk("%d: %#10.8x%#10.8x%#10.8x%#10.8x%#10.8x%#10.8x%#10.8x%#10.8x\n", i, ((int *)data)[i], ((int *)data)[i + 1], ((int *)data)[i + 2], ((int *)data)[i + 3], ((int *)data)[i + 4], ((int *)data)[i + 5], ((int *)data)[i + 6], ((int *)data)[i + 7]);
		}
	}
	// LOG_INF("Header type %d", header_type);
	if (is_rx)
	{
		switch (current_test_status)
		{
		case NotRunning:
			if (header_type != ScheduleTest)
				return;
			current_test_status = Running;
			respond_to_test_start_as_rx = true;

			break;
		case Scheduled:
		case Running:
			if (header_type == Testing)
			{
				num_of_tx_recieved++;
				if (num_of_tx_recieved >= len)
				{
				}
				else if (((int *)data)[num_of_tx_recieved] != 0xAB)
				{
				}
				// LOG_INF("NOOOOO %d", num_of_tx_recieved);
				else if (((int *)data)[num_of_tx_recieved] == 0xAB)
					LOG_INF("Yesss %d", num_of_tx_recieved);
			}
			else if (header_type == EndTest)
			{
				current_test_status = NotRunning;
				send_statistics_back = true;
			}
		}
	}
	else
	{
		switch (current_test_status)
		{
		case Scheduled:
			if (header_type == StartTest)
			{
				LOG_INF("Device ready to take test");
				devices_in_test++;
			}
		case Ended:
			if (header_type == TestResults)
			{
				LOG_INF("WAAAAAAY");
				LOG_INF("Messages recieved: %d", ((int *)data)[1]);
			}
		}
	}
	return;
	// // if msg is statistics (starts with M)
	// if (((char *)data)[0] == 'M')
	// {
	// 	if (is_rx)
	// 		return;
	// 	LOG_INF("%s", (char *)data);
	// 	// turn of the led if you are sending, if you are reading the led should still be on
	// 	gpio_pin_set_dt(&led, is_rx);
	// 	// can_send_tx = true;
	// 	return;
	// }
	// if (is_rx)
	// {
	// 	num_of_tx_recieved++;
	// 	// k_timer_start(&last_msg_timer, K_MSEC(500), K_NO_WAIT);
	// }
	// // LOG_INF("Received data (RSSI: %d.%d): %d %s",
	// // 		(status->rssi_2 / 2), (status->rssi_2 & 0b1) * 5, num_of_tx_recieved, (char *)data);
	// // uint8_t *state = (uint8_t *)data;
	// // if (len > 0 && (*state == '0' || *state == '1'))
	// // {
	// // 	gpio_pin_set_dt(&led, *state - '0');
	// // }
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

void start_test_tx(uint8_t mcs, int duration_ms)
{
	int err;
	if (mcs > MAX_MCS)
	{
		LOG_ERR("MCS value set too high, dont have data for it");
		return;
	}
	// int max_data_len = mcs_subslots_size[mcs][MAX_NUM_OF_SUBSLOTS];
	int max_data_len = mcs_subslots_size[mcs][MAX_NUM_OF_SUBSLOTS] / 8;

	tx_buf[0] = ScheduleTest;
	tx_buf[1] = mcs;

	/* Signal to start test */
	devices_in_test = 0;
	current_test_status = Scheduled;
	for (int i = 0; i < 10; i++)
	{
		err = transmit(TX_HANDLE, (void *)tx_buf, 100, 0);
		// err = transmit(TX_HANDLE, (void *)tx_buf, mcs_subslots_size[0][1], 0);
		if (err != 0)
		{
			LOG_ERR("Error during transmition %d", err);
			return;
		}
		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
		// LOG_INF("Send invitation");
		err = receive(RX_HANDLE, 250);
		if (err != 0)
		{
			LOG_ERR("Error during recieving %d", err);
			return;
		}
		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
	}
	if (devices_in_test == 0)
	{
		LOG_ERR("No devices to take tests with");
		return;
	}

	current_test_status = Running;
	tx_buf[0] = Testing;
	int msg_send = 0;
	LOG_INF("here");
	int64_t current_time = k_uptime_get();
	while (current_test_status == Running && k_uptime_get() - current_time <= duration_ms)
	{
		err = transmit(TX_HANDLE, (void *)tx_buf, max_data_len, mcs);
		// err = transmit(TX_HANDLE, (void *)tx_buf, msg_send, mcs);
		if (err != 0)
		{
			LOG_ERR("Error during transmition %d", err);
			continue;
		}
		if (msg_send % 100 == 0)
			LOG_INF("Sent test %d", msg_send);
		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
		msg_send++;
	}
	LOG_INF("msg sent: %d", msg_send);
	LOG_INF("msg size: %dB = %db", max_data_len, max_data_len * 8);
	LOG_INF("Data rate: %d kb/s", ((max_data_len / 1024) * msg_send * 1000) / duration_ms);
	responses_received = 0;
	tx_buf[0] = EndTest;
	current_test_status = Ended;
	for (int i = 0; i < 10; i++)
	{
		if (responses_received >= devices_in_test)
			break;
		err = transmit(TX_HANDLE, (void *)tx_buf, 10, 0);
		if (err != 0)
		{
			LOG_ERR("Error during transmition %d", err);
			continue;
		}
		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
		err = receive(RX_HANDLE, 100);
		if (err != 0)
		{
			LOG_ERR("Error during transmition %d", err);
			continue;
		}
		/* Wait for RX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
	}
}

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

	for (int i = 0; i < 4832; i++)
	{
		tx_buf[i] = 0xABABABAB;
	}

	/* End of setup*/

	k_timer_start(&rxtx_timer, K_MSEC(1000), K_NO_WAIT);
	while (!is_rx)
	{
		int button_state;
		int prev_button_state = 0;
		button_state = gpio_pin_get_dt(&button);
		if (prev_button_state != button_state && current_test_status == NotRunning)
		{
			prev_button_state = button_state;
			if (button_state != 1)
			{
				continue;
			}
			has_sent = true;

			LOG_INF("Starting TX test");
			// gpio_pin_set_dt(&led, 1);
			start_test_tx(4, 60 * MSEC_PER_SEC);
			// gpio_pin_set_dt(&led, 0);
		}
	}
	LOG_INF("Recieving msg!");
	gpio_pin_set_dt(&led, 1);
	while (1)
	{
		if (respond_to_test_start_as_rx)
		{
			respond_to_test_start_as_rx = false;
			const int tx_len = mcs_subslots_size[0][1];
			tx_buf[0] = StartTest;
			transmit(TX_HANDLE, &tx_buf, 100, 0);
			/* Wait for TX operation to complete. */
			k_sem_take(&operation_sem, K_FOREVER);
			// }
			LOG_INF("Send responses");
		}
		else if (send_statistics_back)
		{
			LOG_INF("Sending statistics");
			send_statistics_back = false;
			tx_buf[0] = TestResults;
			tx_buf[1] = num_of_tx_recieved;
			tx_buf[2] = device_id >> 8;
			tx_buf[3] = device_id & 0xff;
			LOG_INF("msg rec: %d", num_of_tx_recieved);

			transmit(TX_HANDLE, (void *)tx_buf, 100, 0);
			/* Wait for TX operation to complete. */
			k_sem_take(&operation_sem, K_FOREVER);
			LOG_INF("Sending statistics");
			num_of_tx_recieved = 0;
		}
		err = receive(RX_HANDLE, 100);
		if (err != 0)
		{
			LOG_ERR("Error during receiving %d", err);
			continue;
		}
		/* Wait for RX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
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
