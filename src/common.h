#ifndef _COMMON_H
#define _COMMON_H

#define TX_HANDLE 0
#define RX_HANDLE 1
#define MAX_MCS 4
#define MAX_NUM_OF_SUBSLOTS 7
// #define MAX_DATA_LEN 15
#define MAX_DATA_LEN 700
#define SUBSLOTS_USED MAX_NUM_OF_SUBSLOTS

#include <inttypes.h>
// #include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

#include <nrf_modem_dect_phy.h>
#include <modem/nrf_modem_lib.h>

// this semaphere is used all throught the program
// it ensures that the modem system call are syncronised
extern struct k_sem operation_sem;


typedef struct {
	uint16_t num_sent;
	uint8_t end_responses_recieved;
	uint8_t devices_in_test;
} TransmitGlobals;

typedef struct {
	uint16_t num_recv;
	bool respond_to_test_start_as_rx;
	bool send_statistics_back;
	uint16_t last_msg_number;
} RecieveGlobals;


typedef struct {
    uint16_t seconds;
    uint8_t mcs;
	uint16_t msg_to_send;
} TestSettings;

enum TestStatus
{
	NotRunning,
	Scheduled,
	Running,
	Ended,
};

/* Header type 1, due to endianness the order is different than in the specification. */
struct phy_ctrl_field_common
{
	uint32_t packet_length : 4;
	uint32_t packet_length_type : 1;
	uint32_t header_format : 3;
	uint32_t short_network_id : 8;
	uint32_t transmitter_id_hi : 8;
	uint32_t transmitter_id_lo : 8;
	uint32_t df_mcs : 3;
	uint32_t reserved : 1;
	uint32_t transmit_power : 4;
	uint32_t pad : 24;
};

enum TestHeaderType
{
	// send by tx node to start test
	ScheduleTest,
	// rx answers to ScheduleTest so tx knows how many participants
	StartTest,
	Testing,
	// tx sends to signal end of test
	EndTest,
	// rx responds with results
	TestResults,
};

typedef struct {
	enum TestHeaderType type;
	char mcs; // 1B velik prostor
	int msg_num;
} TestHeader;


typedef struct {
	RecieveGlobals rx;
	TransmitGlobals tx;
	uint16_t device_id;
	enum TestStatus cur_test_status;
	bool is_rx;
	bool has_sent;
} Globals;
extern Globals globals;


int transmit(uint32_t handle, void *data, size_t data_len, int mcs);
int receive(uint32_t handle, uint32_t duration_ms);

extern const int mcs_subslots_size[MAX_MCS + 1][MAX_NUM_OF_SUBSLOTS + 1];


#endif