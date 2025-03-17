#include "serial_communication.h"

LOG_MODULE_REGISTER(uart, LOG_LEVEL_DBG);

char uart_buffer[UART_BUF_LEN] = {};
void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data);
bool proccess_msg(const struct device *dev, struct uart_event *evt, char data[]);


int uart_setup() {
    int err;

    // get uart device
    struct device* uart0 = DEVICE_DT_GET(DT_CHOSEN(zephyr_shell_uart));
    if (!device_is_ready(uart0)) {
        LOG_ERR("Uart0 device is not ready.");
        return 0;
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
    int err;

    switch (evt->type) {
        case UART_RX_RDY:
            LOG_DBG("UART_RX_RDY");
            proccess_msg(dev, evt, evt->data.rx.buf + evt->data.rx.offset);
            break;

        case UART_RX_BUF_REQUEST:
            for (int i = 0; i < UART_BUF_LEN; i++)
                uart_buffer[i] = 0;
            err = uart_rx_buf_rsp(dev, uart_buffer, UART_BUF_LEN);
            if (err < 0) {
                LOG_ERR("uart_rx_buf_rsp %d", err);
                return;
            }
            break;

        case UART_RX_BUF_RELEASED:
        case UART_RX_DISABLED:
        case UART_RX_STOPPED:
        case UART_TX_DONE:
        case UART_TX_ABORTED:
            LOG_DBG("other uart code %d", evt->type);
            break;
        default:
            LOG_ERR("Wrong UART_CODE");
            break;
    }
    return;
}
bool starts_with(char *data, char *template) {
    int i = 0;
    while (template[i] != '\0' && data[i] != '\0') {
        if (data[i] != template[i]) {
            return false;
        }
        i++;
    }
    return true;
}

bool proccess_msg(const struct device *dev, struct uart_event *evt, char data[]) {
    LOG_INF("|%s|", data);
    // LOG_DBG("same %d", cmp(data, "ping\r\n"));
    // LOG_DBG("same %d", data[cmp(data, "ping\r\n")]);
    // for (int i = 0; i < 6; i++) {
    //     if (data[i] == 0) break;
    //     LOG_DBG("Compare data: %c, ping: %c", data[i], "ping\r\n"[i]);
    // }
    if (starts_with(data, "ping")) {
        uart_tx(dev, "pong\r\n", 7, 100);
    } else if (starts_with(data, "start")) {
        // get the test parameters and "schedule" it
        globals.is_rx = false;
        TestSettings set = {};

        sscanf(data, "start mcs:%d times:%d msg:%d",  &set.mcs, &set.times, &set.msg_to_send);
        globals.test_settings = set;
    } else if (starts_with(data, "stop")) {
        globals.cur_test_status = Ended;
    } else if (starts_with(data, "restart")) {
        sys_reboot(SYS_REBOOT_COLD);
    } else return false;
    return true;
}

