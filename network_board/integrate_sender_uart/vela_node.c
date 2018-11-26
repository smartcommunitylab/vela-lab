/**
 * vela_node.c - this will start all the processes needed at a non-sink node
 1. start uart
 2. start unicast sender
 3. start trickle receiver

 channel check interval
 *
 **/

#include "contiki.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include <stdio.h>
#include <string.h>
#include "vela_uart.h"
#include "vela_sender.h"
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
#include "max-17260-sensor.h"
#endif
#endif

#define FUEL_GAUGE_POLLING_INTERVAL CLOCK_SECOND*7

PROCESS(vela_node_process, "main starter process for non-sink");

AUTOSTART_PROCESSES(&vela_node_process);

static struct ctimer polling_timer;

static void poll_fuel_gauge(void *not_used) {
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
	ctimer_set(&polling_timer, FUEL_GAUGE_POLLING_INTERVAL, poll_fuel_gauge, NULL);

	uint16_t REP_CAP_mAh, REP_SOC_permillis, TTE_minutes, AVG_voltage_mV;
	int16_t AVG_current_10uA;

	REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
	REP_SOC_permillis = max_17260_sensor.value(
	MAX_17260_SENSOR_TYPE_REP_SOC);
	TTE_minutes = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_TTE);
	AVG_current_10uA = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_I);
	AVG_voltage_mV = max_17260_sensor.value(
	MAX_17260_SENSOR_TYPE_AVG_V);

	printf("POLLING: ");
	printf("Remaining battery capacity = %d mAh ", REP_CAP_mAh);
	printf("or %d.%d %%. ", (uint16_t) (REP_SOC_permillis / 10), REP_SOC_permillis % 10);
	printf("Remaining runtime with this consumption: %d hours and %d minutes. ", TTE_minutes / 60, TTE_minutes % 60);
	if (AVG_current_10uA > 0) {
		printf("Avg current %d.%d mA ", AVG_current_10uA / 100, AVG_current_10uA % 100);
	} else {
		printf("Avg current -%d.%d mA ", -AVG_current_10uA / 100, -AVG_current_10uA % 100);
	}
	printf("Avg voltage = %d mV ", AVG_voltage_mV);
	printf("\n");
#endif
#endif
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(vela_node_process, ev, data) {
	PROCESS_BEGIN()
	;
	//printf("main: started\n");

	// initialize and start the other threads
	vela_uart_init();
	vela_sender_init();

	poll_fuel_gauge(0);
	// do something, probably related to the watchdog

PROCESS_END();
}
/*---------------------------------------------------------------------------*/

