#ifndef _TRANSMIT_H
#define _TRANSMIT_H

#include <string.h>
#include <inttypes.h>

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


#include <inttypes.h>
#include "common.h"


int start_test_tx(TestSettings* settings, char tx_buf[]);
void handle_tx_pdc(const uint64_t *time, const struct nrf_modem_dect_phy_rx_pdc_status *status, const void *data_void, uint32_t len);

#endif