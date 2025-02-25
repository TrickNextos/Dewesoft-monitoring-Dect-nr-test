#ifndef _INIT_H
#define _INIT_H

#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

int init_led_and_button();
int modem_init(struct nrf_modem_dect_phy_callbacks *dect_phy_callbacks,
               struct nrf_modem_dect_phy_init_params *dect_phy_init_params,
               long device_id,
               struct k_sem *operation_sem
            );
int modem_deinit();

void set_led(int state);
int read_button();

#endif