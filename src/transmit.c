#include <inttypes.h>
#include "transmit.h"
#include "common.h"
#include "common.h"

LOG_MODULE_REGISTER(transmit, LOG_LEVEL_DBG);

// extern k_sem operation_sem;

int setup_tx_test(TestSettings* settings, char tx_buf[]);
void finish_tx_test(TestSettings* settings, char tx_buf[]);

int start_test_tx(TestSettings* settings, char tx_buf[])
{
	int err;
	if (settings->mcs > MAX_MCS)
	{
		LOG_ERR("MCS value set too high, dont have data for it");
		return -1;
	}
	// int max_data_len = mcs_subslots_size[mcs][SUBSLOTS_USED];
	int max_data_len = mcs_subslots_size[settings->mcs][SUBSLOTS_USED] / 8;

	tx_buf[0] = ScheduleTest;
	tx_buf[1] = settings->mcs;

	/* Signal to start test */
	err = setup_tx_test(settings, tx_buf);
	LOG_ERR("myb");
	if (err < 0) return err;

	globals.cur_test_status = Running;
	tx_buf[0] = Testing;
	int msg_send = 0;
	
	// int64_t current_time = k_uptime_get();
	LOG_INF("max data len %d", max_data_len);
	// while (globals.cur_test_status == Running && k_uptime_get() - current_time <= settings->seconds * MSEC_PER_SEC)
	// while (current_test_status == Running && msg_send < 1500)
	TestHeader h = {
		.type = Running,
		.mcs = settings->mcs,
		.msg_num = 0,
	};

	int64_t start_time = k_uptime_get();
	for (int i = 0; i < settings->msg_to_send; i++)
	{
		h.msg_num = i;
		memcpy(tx_buf, &h, sizeof(h));

		// err = transmit(TX_HANDLE, (void *)tx_buf, max_data_len, settings->mcs);
		err = transmit(TX_HANDLE, (void *)tx_buf, 100, settings->mcs);

		if (err != 0)
		{
			LOG_ERR("Error during transmition %d", err);
			continue;
		}
		if (msg_send % 100 == 0)
			LOG_DBG("Sent test %d", msg_send);

		msg_send++;
		set_led(msg_send % 50 > 25 ? 1 : 0);

		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);

		// k_msleep(5);
	}
	int64_t end_time = k_uptime_get();
	LOG_INF("Time needed: %d ms", end_time - start_time);
	set_led(0);

	LOG_INF("msg sent: %d", msg_send);
	LOG_INF("msg size: %dB = %db", max_data_len, max_data_len * 8);
	LOG_INF("Data sent: %d b", max_data_len * 8 * msg_send);
	
	finish_tx_test(settings, tx_buf);
	return 0;
}

int setup_tx_test(TestSettings* settings, char tx_buf[]) {
	int err;
	globals.tx.devices_in_test = 0;
	globals.cur_test_status = Scheduled;
	for (int i = 0; i < 10; i++)
	{
		tx_buf[2] = i;
		err = transmit(TX_HANDLE, (void *)tx_buf, 100, settings->mcs);
		// err = transmit(TX_HANDLE, (void *)tx_buf, mcs_subslots_size[0][1], 0);
		if (err != 0)
		{
			LOG_ERR("Error during transmition %d", err);
			return -1;
		}
		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
		// LOG_INF("Send invitation");
		err = receive(RX_HANDLE, 250);
		if (err != 0)
		{
			LOG_ERR("Error during recieving %d", err);
			return -1;
		}
		/* Wait for TX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
	}
	if (globals.tx.devices_in_test == 0)
	{
		LOG_ERR("No devices to take tests with");
		return -2;
	}

	return 0;
}

void finish_tx_test(TestSettings* settings, char tx_buf[]) {
	int err;
	globals.tx.end_responses_recieved = 0;
	tx_buf[0] = EndTest;
	globals.cur_test_status = NotRunning;

	for (int i = 0; i < 10; i++)
	{
		if (globals.tx.end_responses_recieved >= globals.tx.devices_in_test)
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

void handle_tx_pdc(const uint64_t *time, const struct nrf_modem_dect_phy_rx_pdc_status *status, const void *data_void, uint32_t len) {
	LOG_DBG("handle_tx_pdc");

	TestHeader h = {};
	LOG_HEXDUMP_DBG(data_void, sizeof(h) * 10, "Header: ");
	memcpy(&h, data_void + 4, sizeof(h)); // idk why +4, but it works // update, works only for StartTest, not for TestResults ;(((((
	LOG_DBG("type %d", h.type);
	LOG_ILOG_DBGNF("mc %d", h.mcs);
	LOG_DBG("msg_num %d", h.msg_num);

	switch (globals.cur_test_status)
	{
	case NotRunning:
	case Scheduled:
		if (h.type == StartTest)
		{
			LOG_INF("Device ready to take test");
			globals.tx.devices_in_test++;
		}
	case Ended:
		if (h.type == TestResults)
		{
			// LOG_HEXDUMP_INF(data, 50U, "Results hexdump:");
			// int msg_recieved_inner;
			LOG_INF("WAAAAAAY");

			// memcpy(&msg_recieved_inner, data+1, sizeof(msg_recieved_inner));
			LOG_INF("WAAAAAAY");
			// LOG_HEXDUMP_INF(msg_recieved_inner, sizeof(msg_recieved_inner), "msg_recieved_inner content:");
			LOG_INF("Messages recieved: %d", h.msg_num);
		}
	}
}