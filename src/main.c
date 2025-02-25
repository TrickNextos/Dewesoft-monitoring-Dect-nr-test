/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "main.h"
#include "init.h"
#include "common.h"

#include "transmit.h"
#include "recieve.h"

LOG_MODULE_REGISTER(main);

// static Globals globals = {};

// if this is the error, include overlay-eu.proj/overlay-us.proj into build
BUILD_ASSERT(CONFIG_CARRIER, "Carrier must be configured according to local regulations");

static bool exit;
// static uint16_t device_id;

// char tx_buf[MAX_DATA_LEN] = "Eos harum dolorum in iure aspernatur aut consectetur dolores. In quae itaque et dolorem natus quo voluptate molestias non commodi officia est laborum molestiae est vitae aliquam. Non autem ipsa ex consequuntur quae et natus tempore! Sed debitis pariatur non optio inventore sit aspernatur mollitia ut ipsa sunt a impedit nihil eum asperiores possimus est ullam quis. 33 architecto expedita eos autem quisquam ea sapiente saepe et delectus facilis et odio excepturi ad numquam iure. Aut sunt quia in nihil architecto cum explicabo sint et galisum perspiciatis. Non possimus mollitia non Quis labore eos dignissimos galisum. Qui excepturi accusamus eum tempora quibusdam et deserunt omnis qui consequuntur pariatur sit dignissimos vero est nisi distinctio est quaerat doloremque.Aut galisum vero id rerum molestiae sed dolores nihil. Ex dolor quia ut provident modi qui voluptatem iusto vel quia explicabo ut expedita quam sed quia reprehenderit aut repellat perspiciatis. Et delectus aliquid quo aspernatur quidem sit reiciendis sint. Ut molestiae consequatur ea eaque consequatur et fugiat consequatur vel suscipit dolores ad nesciunt dolorem. Non quibusdam maiores et blanditiis laborum sed vitae nemo et distinctio sunt aut dolor repudiandae. Non praesentium rerum aut voluptas error vel eius pariatur aut reprehenderit voluptatum? Et vero nemo ad amet enim et vitae veniam in optio consequatur non autem itaque eos animi expedita. Sit quis suscipit cum ipsum enim rem omnis nobis eum debitis autem. Aut similique voluptatem non recusandae quod ad reprehenderit blanditiis eos voluptate nostrum. Et distinctio doloremque non molestiae doloribus ut placeat accusamus et magnam doloremque At aliquid quia. Est quam sint non assumenda dolorem hic rerum facilis a reprehenderit voluptatibus in assumenda commodi ut provident corporis et numquam recusandae. Et dolorem architecto et deleniti autem vel Quis earum. Ut nisi maxime id beatae nostrum id magnam accusantium ad nesciunt libero! 33 incidunt rerum eum voluptatem esse qui iure odit et cupiditate sunt.Sit rerum necessitatibus rem eveniet repellat ut error sunt rem sunt tempora eum cupiditate amet non consectetur tempore. Ut amet accusantium rem sequi molestias sed inventore ipsa in ipsa voluptatem. In aliquam corrupti et ducimus dolores rem voluptatem repellat aut odit temporibus et soluta maxime. Et recusandae quia in porro laborum aut culpa tenetur. Ut tempora expedita a quia eveniet et galisum culpa et nisi fugit. Non aliquid saepe eos sequi eaque rem maiores saepe hic porro omnis ea voluptas dolores sit deserunt similique non illum rerum. Non nostrum veniam et impedit nisi vel dolor eveniet ab blanditiis nesciunt non eveniet numquam. In quos omnis non obcaecati voluptatum ea unde earum qui dolor adipisci et odio dolor! Ut galisum totam ut sint porro ab illum dolore et dolorem nostrum. Et numquam culpa aut enim labore ut quis rerum qui autem fugit a harum sapiente. Cum repellat nisi hic impedit autem aut quis similique. Et molestiae doloremque non voluptatem tenetur est quisquam totam eum fuga temporibus cum laborum obcaecati sed eveniet doloribus a voluptatibus labore. In deserunt aperiam et ducimus quisquam quo autem error. Et praesentium ipsum sit adipisci quia et sunt reprehenderit.Eos harum dolorum in iure aspernatur aut consectetur dolores. In quae itaque et dolorem natus quo voluptate molestias non commodi officia est laborum molestiae est vitae aliquam. Non autem ipsa ex consequuntur quae et natus tempore! Sed debitis pariatur non optio inventore sit aspernatur mollitia ut ipsa sunt a impedit nihil eum asperiores possimus est ullam quis. 33 architecto expedita eos autem quisquam ea sapiente saepe et delectus facilis et odio excepturi ad numquam iure. Aut sunt quia in nihil architecto cum explicabo sint et galisum perspiciatis. Non possimus mollitia non Quis labore eos dignissimos galisum. Qui excepturi accusamus eum tempora quibusdam et deserunt omnis qui consequuntur pariatur sit dignissimos vero est nisi distinctio est quaerat doloremque.Aut galisum vero id rerum molestiae sed dolores nihil. Ex dolor quia ut provident modi qui voluptatem iusto vel quia explicabo ut expedita quam sed quia reprehenderit aut repellat perspiciatis. Et delectus aliquid quo aspernatur quidem sit reiciendis sint. Ut molestiae consequatur ea eaque consequatur et fugiat consequatur vel suscipit dolores ad nesciunt dolorem. Non quibusdam maiores et blanditiis laborum sed vitae nemo et distinctio sunt aut dolor repudiandae. Non praesentium rerum aut voluptas error vel eius pariatur aut reprehenderit voluptatum? Et vero nemo ad amet enim et vitae veniam in optio consequatur non autem itaque eos animi expedita. Sit quis suscipit cum ipsum enim rem omnis nobis eum debitis autem. Aut similique voluptatem non recusandae quod ad reprehenderit.";
char tx_buf[MAX_DATA_LEN] = {};



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

/*
	PHY modem functions
*/

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
// static void pdc(const uint64_t *time,
// 				const struct nrf_modem_dect_phy_rx_pdc_status *status,
// 				const void *data_void, uint32_t len)
// {
// 	set_led(0);
// 	char* data = (char *)data_void;
// 	enum TestHeaderType header_type = (enum TestHeaderType)(data[0]);
// 	// LOG_INF("L: %d", len);
// 	// LOG_INF("Header type %d", header_type);
// 	if (is_rx)
// 	{
// 		switch (current_test_status)
// 		{
// 		case NotRunning:
// 			if (header_type != ScheduleTest)
// 				return;
// 			current_test_status = Running;
// 			respond_to_test_start_as_rx = true;

// 			break;
// 		case Scheduled:
// 		case Running:
// 			if (header_type == Testing)
// 			{
// 				num_of_tx_recieved++;
//     			// LOG_HEXDUMP_INF(data, 480U, "Sample Data!"); 
// 				// data[len-1] = 0;
// 				// if (MAX_DATA_LEN < len) {
// 				// 	data[MAX_DATA_LEN - 1] = 0;
// 				// }
// 				if (num_of_tx_recieved % 100 == 0)
// 					printk("Data recieved (len %d)\n", len);
// 				if (num_of_tx_recieved == 154)
//     				LOG_HEXDUMP_INF(data, len, "Sample Data!"); 
// 				// for(int i = 10; i < len && i < MAX_DATA_LEN; i++) {
// 				// 	if (data[i] != tx_buf[i]) {
// 				// 		LOG_ERR("Wrong data at position %d: %d %d", i, data[i], tx_buf[i]);
// 				// 	}
// 				// }
// 			}
// 			else if (header_type == EndTest)
// 			{
// 				current_test_status = NotRunning;
// 				send_statistics_back = true;
// 			}
// 		}
// 	}
// 	else
// 	{
// 		switch (current_test_status)
// 		{
// 		case NotRunning:
// 		case Scheduled:
// 			if (header_type == StartTest)
// 			{
// 				LOG_INF("Device ready to take test");
// 				devices_in_test++;
// 			}
// 		case Ended:
// 			if (header_type == TestResults)
// 			{
// 				LOG_HEXDUMP_INF(data, 50U, "Results hexdump:");
// 				int msg_recieved_inner;
// 				LOG_INF("WAAAAAAY");

// 				memcpy(&msg_recieved_inner, data+1, sizeof(msg_recieved_inner));
// 				LOG_INF("WAAAAAAY");
// 				// LOG_HEXDUMP_INF(msg_recieved_inner, sizeof(msg_recieved_inner), "msg_recieved_inner content:");
// 				LOG_INF("Messages recieved: %d", msg_recieved_inner);
// 			}
// 		}
// 	}
// 	return;
// 	// // if msg is statistics (starts with M)
// 	// if (((char *)data)[0] == 'M')
// 	// {
// 	// 	if (is_rx)
// 	// 		return;
// 	// 	LOG_INF("%s", (char *)data);
// 	// 	// turn of the led if you are sending, if you are reading the led should still be on
// 	// 	gpio_pin_set_dt(&led, is_rx);
// 	// 	// can_send_tx = true;
// 	// 	return;
// 	// }
// 	// if (is_rx)
// 	// {
// 	// 	num_of_tx_recieved++;
// 	// 	// k_timer_start(&last_msg_timer, K_MSEC(500), K_NO_WAIT);
// 	// }
// 	// // LOG_INF("Received data (RSSI: %d.%d): %d %s",
// 	// // 		(status->rssi_2 / 2), (status->rssi_2 & 0b1) * 5, num_of_tx_recieved, (char *)data);
// 	// // uint8_t *state = (uint8_t *)data;
// 	// // if (len > 0 && (*state == '0' || *state == '1'))
// 	// // {
// 	// // 	gpio_pin_set_dt(&led, *state - '0');
// 	// // }
// }

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
	.pdc = handle_rx_pdc,
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

/* Timers*/
// wait a fraction of a second at start to see if the button is pressed
// at the end of this the device is set into rx or tx mode
extern void set_rx_or_tx(struct k_timer *timer_id)
{
	// if the device is in TX mode, 
	if (globals.has_sent) {
		dect_phy_callbacks.pdc = handle_tx_pdc;
	}
	globals.is_rx = !globals.has_sent;
}

K_TIMER_DEFINE(rxtx_timer, set_rx_or_tx, NULL);



int main(void)
{
	int err;

	if (init_led_and_button() >= 0) return 0;
	if (modem_init(&dect_phy_callbacks, &dect_phy_init_params, globals.device_id, &operation_sem) >= 0) return 0;

	LOG_INF("Dect NR+ PHY Hello sample started");


	// if the button is pressed in 1 second, the device will go into tx mode
	k_timer_start(&rxtx_timer, K_MSEC(1000), K_NO_WAIT);
	// memset(tx_buf, ':', MAX_DATA_LEN*sizeof(tx_buf[0]));
	// tx_buf[MAX_DATA_LEN-1] = '0';
	while (!globals.is_rx)
	{
		int button_state;
		int prev_button_state = 0;
		button_state = read_button();
		if (prev_button_state != button_state && globals.cur_test_status == NotRunning)
		{
			prev_button_state = button_state;
			if (button_state != 1)
			{
				continue;
			}

			dect_phy_callbacks.pdc = handle_tx_pdc;
			err = nrf_modem_dect_phy_callback_set(&dect_phy_callbacks);
			if (err)
			{
				LOG_ERR("nrf_modem_dect_phy_init failed, err %d", err);
				return err;
			}
			// err = nrf_modem_dect_phy_init(dect_phy_init_params);

			globals.has_sent = true;

			LOG_INF("Starting TX test");
			// gpio_pin_set_dt(&led, 1);
			// start_test_tx(4, 10 * MSEC_PER_SEC);
			TestSettings set = {
				.mcs = 4,
				.seconds = 15,
				.msg_to_send = 10000,
			};
			err = start_test_tx(&set, tx_buf);
			if (err < 0) {
				LOG_ERR("start_test_tx err %d", err);
			}
			// gpio_pin_set_dt(&led, 0);
		}
	}


	LOG_INF("Recieving msg!");
	set_led(1);
	while (1)
	{
		if (globals.rx.respond_to_test_start_as_rx)
		{
			globals.rx.respond_to_test_start_as_rx = false;
			start_rx_test(tx_buf);
		}
		else if (globals.rx.send_statistics_back)
		{
			globals.rx.send_statistics_back = false;
			end_rx_test(tx_buf);
		}

		err = receive(RX_HANDLE, 1000);
		if (err != 0)
		{
			LOG_ERR("Error during receiving %d", err);
			continue;
		}
		/* Wait for RX operation to complete. */
		k_sem_take(&operation_sem, K_FOREVER);
	}

	LOG_INF("Shutting down");
	modem_deinit(&operation_sem);

	LOG_INF("Bye!");

	return 0;
}
