// #ifndef _SERIAL_COMMUNICATION_H
// #define _SERIAL_COMMUNICATION_H

#include "common.h"
#include <zephyr/drivers/uart.h>

#define UART_BUF_LEN 256
#define UART_RX_TIMEOUT 100

int uart_setup();

// #endif // _SERIAL_COMMUNICATION_H