#include "serial_communication.h"
#include "init.h"

LOG_MODULE_REGISTER(uart, LOG_LEVEL_DBG);

char uart_buffer[UART_BUF_LEN] = {};
void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data);


int uart_setup() {
    int err;

    // get uart device
    struct device* uart0 = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(uart0)) {
        LOG_ERR("Uart0 device is not ready.");
        return;
    }

    // use uart_callback funcion without any specific data, to be send to it, as there are globals already
    err = uart_callback_set(uart0, uart_callback, NULL);
    if (err < 0) {
        LOG_ERR("Error with 'uart_callback_set': %d", err);
        return err;
    }
    err = uart_rx_enable(uart0, uart_buffer, UART_BUF_LEN, UART_RX_TIMEOUT);
    if (err < 0) {
        LOG_ERR("Error with 'uart_rx_enable': %d", err);
        return err;
    }
    LOG_DBG("Setup uart");

    return 0;
}

// callback for uart communication
void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data) {
    LOG_DBG("Data from uart! ");
    switch (evt->type) {
        case UART_TX_DONE:
            // do something
            break;

        case UART_TX_ABORTED:
            // do something
            break;

        case UART_RX_RDY:
            LOG_INF("Rx data: %s", evt->data.rx.buf + evt->data.rx.offset);
            LOG_INF("Rx offset: %d", evt->data.rx.offset);
            break;

        case UART_RX_BUF_REQUEST:
            // do something
            break;

        case UART_RX_BUF_RELEASED:
            // do somethingq
            break;

        case UART_RX_DISABLED:
            // do something
            break;

        case UART_RX_STOPPED:
            // do something
            break;

        default:
            break;
    }
    return;
}
