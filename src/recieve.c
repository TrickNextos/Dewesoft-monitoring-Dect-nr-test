#include "recieve.h"

LOG_MODULE_REGISTER(recieve, LOG_LEVEL_DBG);

void handle_rx_pdc(const uint64_t *time, const struct nrf_modem_dect_phy_rx_pdc_status *status, const void *data_void, uint32_t len) {
    // LOG_INF("handle_rx_pdc");

	char* data = (char *)data_void;

	// int64_t current_time = k_uptime_get();
	TestHeader h = {};
	memcpy(&h, data_void, sizeof(h));
	// LOG_INF("msg num: %d", h.msg_num);

    switch (globals.cur_test_status)
	{
	case NotRunning:
		if (h.type != ScheduleTest)
			return;
		globals.cur_test_status = Running;
		globals.rx.respond_to_test_start_as_rx = true;

		break;
	case Scheduled:
	case Running:
		if (h.type == Testing)
		{
			if (globals.rx.last_msg_number == 0) {
				LOG_INF("Single msg size %d B", len);
			}
			while (++globals.rx.last_msg_number < h.msg_num) {
				LOG_ERR("Missed package %d", globals.rx.last_msg_number-1);
			}
			globals.rx.num_recv++;
		}
		else if (h.type == EndTest)
		{
			globals.cur_test_status = NotRunning;
			globals.rx.send_statistics_back = true;
		}
		break;
	case Ended: break;
	}

	// int64_t end_time = k_uptime_get();
	// LOG_INF("Time needed %d in ms", end_time - current_time);
}

void start_rx_test(char tx_buf[]){
	LOG_INF("Participating in test");
	TestHeader h = {
		.mcs = 0,
		.msg_num = 0,
		.type = StartTest,
	};

	LOG_DBG("type %d", h.type);
	LOG_DBG("mc %d", h.mcs);
	LOG_DBG("msg_num %d", h.msg_num);

	memcpy(tx_buf, &h, sizeof(h));

	LOG_HEXDUMP_DBG(tx_buf, sizeof(h) * 2, "Header: ");
	TestHeader h1 = {};
	memcpy(&h1, tx_buf, sizeof(h1));

	LOG_DBG("type %d", h1.type);
	LOG_DBG("mc %d", h1.mcs);
	LOG_DBG("msg_num %d", h1.msg_num);

	transmit(TX_HANDLE, &tx_buf, 100, 2);
	/* Wait for TX operation to complete. */
	k_sem_take(&operation_sem, K_FOREVER);
}

void end_rx_test(char tx_buf[]){
	LOG_INF("Sending statistics");
	// struct StatisticsResults {
	// 	int num_of_tx_recieved;
	// 	uint16_t device_id;
	// };
	// tx_buf[0] = TestResults;
	// struct StatisticsResults res = {
	// 	.num_of_tx_recieved = globals.rx.num_recv,
	// 	.device_id = globals.device_id,
	// };

	// // tx_buf[1] = num_of_tx_recieved;
	// // tx_buf[2] = device_id >> 8;
	// // tx_buf[3] = device_id & 0xff;
	// memcpy(tx_buf+1, &res, sizeof(res));
	LOG_DBG("msg rec: %d", globals.rx.num_recv);

	TestHeader h = {
		.type = TestResults,
		.mcs = 0,
		.msg_num = globals.rx.num_recv,
	};
	memcpy(tx_buf, &h, sizeof(h));
	transmit(TX_HANDLE, (void *)tx_buf, 100, 2);
	LOG_INF("Sending statistics");

	globals.rx.num_recv = 0;
	globals.rx.last_msg_number = 0;

	/* Wait for TX operation to complete. */
	k_sem_take(&operation_sem, K_FOREVER);
}