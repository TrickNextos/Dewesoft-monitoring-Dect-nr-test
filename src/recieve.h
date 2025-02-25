#ifndef _RECIEVE_H
#define _RECIEVE_H

#include "common.h"

void handle_rx_pdc(const uint64_t *time,
				const struct nrf_modem_dect_phy_rx_pdc_status *status,
				const void *data_void, uint32_t len);
void start_rx_test();
void end_rx_test(char tx_buf[]);

#endif // _RECIEVE_H