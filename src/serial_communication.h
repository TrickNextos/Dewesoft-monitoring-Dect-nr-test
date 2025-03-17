#ifndef _SERIAL_COMMUNICATION_H
#define _SERIAL_COMMUNICATION_H

#include "common.h"
#include "init.h"

#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/toolchain.h>

#define UART_BUF_LEN 1024
#define UART_RX_TIMEOUT 1000

int uart_setup();

#endif // _SERIAL_COMMUNICATION_H