#include "common.h"
#include <nrf_modem_dect_phy.h>

K_SEM_DEFINE(operation_sem, 0, 1);
Globals globals = {};

LOG_MODULE_REGISTER(common, LOG_LEVEL_DBG);

/* Send operation. */
int transmit(uint32_t handle, void *data, size_t data_len, int mcs)
{
	int err;
	// LOG_INF("data len: %d", data_len);

	struct phy_ctrl_field_common header = {
		.header_format = 0x0,
		.packet_length_type = 0x0,
		.packet_length = SUBSLOTS_USED,
		.short_network_id = (CONFIG_NETWORK_ID & 0xff),
		.transmitter_id_hi = (globals.device_id >> 8),
		.transmitter_id_lo = (globals.device_id & 0xff),
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
		// .data_size = mcs_subslots_size[mcs][SUBSLOTS_USED],
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
int receive(uint32_t handle, uint32_t duration_ms)
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


const int mcs_subslots_size[MAX_MCS + 1][MAX_NUM_OF_SUBSLOTS + 1] = {
	{0,
	 136,
	 264,
	 400,
	 536,
	 664,
	 792,
	 920},
	{32,
	 296,
	 552,
	 824,
	 1096,
	 1352,
	 1608,
	 1864},
	{56,
	 456,
	 856,
	 1256,
	 1640,
	 2024,
	 2360,
	 2774},
	{88,
	 616,
	 1128,
	 1672,
	 2168,
	 2680,
	 3192,
	 3704},
	{144,
	 936,
	 1736,
	 2488,
	 3256,
	 4024,
	 4832,
	 5600}};
