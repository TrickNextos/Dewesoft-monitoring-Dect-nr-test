#include "init.h"
#include "common.h"


LOG_MODULE_REGISTER(init, LOG_LEVEL_DBG);


/* Button and led initialization */
#define SW0_NODE DT_ALIAS(sw0)
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "Unsupported board: sw0 devicetree alias is not defined"
#endif
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET_OR(SW0_NODE, gpios,
															  {0});
static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
													 {0});

int init_led_and_button() {
	int err;
	/* Button and led setup*/
	if (!gpio_is_ready_dt(&button))
	{
		printk("Error: button device is not ready\n");
		return 0;
	}

	err = gpio_pin_configure_dt(&button, GPIO_INPUT);
	if (err != 0)
	{
		printk("Error %d: failed to configure %s pin %d\n",
			   err, button.port->name, button.pin);
		return 0;
	}
	if (led.port && !gpio_is_ready_dt(&led))
	{
		printk("Error %d: LED device %s is not ready; ignoring it\n",
			   err, led.port->name);
		led.port = NULL;
	}
	if (led.port)
	{
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT);
		if (err != 0)
		{
			printk("Error %d: failed to configure LED device %s pin %d\n",
				   err, led.port->name, led.pin);
			led.port = NULL;
		}
		else
		{
			printk("Set up LED at %s pin %d\n", led.port->name, led.pin);
		}
	}
    return -1;
}

int modem_init(struct nrf_modem_dect_phy_callbacks *dect_phy_callbacks, struct nrf_modem_dect_phy_init_params *dect_phy_init_params, long device_id, struct k_sem *operation_sem) {
	int err;

	err = nrf_modem_lib_init();
	if (err)
	{
		LOG_ERR("modem init failed, err %d", err);
		return err;
	}
	
	err = nrf_modem_dect_phy_callback_set(dect_phy_callbacks);
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_callback_set failed, err %d", err);
		return err;
	}
	
	err = nrf_modem_dect_phy_init(dect_phy_init_params);
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_init failed, err %d", err);
		return err;
	}


	k_sem_take(operation_sem, K_FOREVER);
	// if (exit)
	// {
	// 	return -EIO;
	// }
	hwinfo_get_device_id((void *)&device_id, sizeof(device_id));

	LOG_INF("Dect NR+ PHY initialized, device ID: %d", device_id);

	err = nrf_modem_dect_phy_capability_get();
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_capability_get failed, err %d", err);
	}

    return -1;

}

int modem_deinit(struct k_sem *operation_sem){
    int err;
	err = nrf_modem_dect_phy_deinit();
	if (err)
	{
		LOG_ERR("nrf_modem_dect_phy_deinit() failed, err %d", err);
		return err;
	}

	k_sem_take(operation_sem, K_FOREVER);

	err = nrf_modem_lib_shutdown();
	if (err)
	{
		LOG_ERR("nrf_modem_lib_shutdown() failed, err %d", err);
		return err;
	}
}


void set_led(int state) {
	gpio_pin_set_dt(&led, state);
}

int read_button() {
    return gpio_pin_get_dt(&button);
}
