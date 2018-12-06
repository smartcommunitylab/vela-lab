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
#include <stdio.h>
#include <string.h>
#include "vela_uart.h"
#include "vela_sender.h"
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
#include "max-17260-sensor.h"
#endif
#endif

#define FUEL_GAUGE_POLLING_INTERVAL CLOCK_SECOND * 60

PROCESS(vela_node_process, "main starter process for non-sink");

AUTOSTART_PROCESSES (&vela_node_process);

static struct ctimer polling_timer;
static uint16_t t1 = 1;
static uint16_t t2 = 2;
static uint16_t t3 = 3;
static uint16_t t4 = 4;
static uint16_t t5 = 5;
uint8_t bat_data[10] = {0};

static void poll_fuel_gauge(void *not_used)
{
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
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
	bat_data[0] = (uint8_t)(REP_CAP_mAh >> 8);
	bat_data[1] = (uint8_t)REP_CAP_mAh;
	bat_data[2] = (uint8_t)(REP_SOC_permillis >> 8);
	bat_data[3] = (uint8_t)REP_SOC_permillis;
	bat_data[4] = (uint8_t)(TTE_minutes >> 8);
	bat_data[5] = (uint8_t)TTE_minutes;
	bat_data[6] = (uint8_t)(AVG_current_10uA >> 8);
	bat_data[7] = (uint8_t)AVG_current_10uA;
	bat_data[8] = (uint8_t)(AVG_voltage_mV >> 8);
	bat_data[9] = (uint8_t)AVG_voltage_mV;
#endif
#else
	t1++;
	t2++;
	t3++;
	t4++;
	t5++;
	bat_data[0] = (uint8_t)(t1 >> 8);
	bat_data[1] = (uint8_t)t1;
	bat_data[2] = (uint8_t)(t2 >> 8);
	bat_data[3] = (uint8_t)t2;
	bat_data[4] = (uint8_t)(t3 >> 8);
	bat_data[5] = (uint8_t)t3;
	bat_data[6] = (uint8_t)(t4 >> 8);
	bat_data[7] = (uint8_t)t4;
	bat_data[8] = (uint8_t)(t5 >> 8);
	bat_data[9] = (uint8_t)t5;
#endif
	ctimer_set(&polling_timer, FUEL_GAUGE_POLLING_INTERVAL, poll_fuel_gauge, NULL);
	process_post(&vela_sender_process, event_bat_data_ready, bat_data);
}

/*---------------------------------------------------------------------------*/
static struct etimer et;

PROCESS_THREAD(vela_node_process, ev, data)
{
	PROCESS_BEGIN();
	printf("main: started\n");
	etimer_set(&et, 0.2 * CLOCK_SECOND);
	PROCESS_WAIT_UNTIL(etimer_expired(&et));
	// initialize and start the other threads
	vela_uart_init();

	printf("Uart initialized\n");
	vela_sender_init();
	event_bat_data_ready = process_alloc_event();
	poll_fuel_gauge(0);
	// do something, probably related to the watchdog

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/


