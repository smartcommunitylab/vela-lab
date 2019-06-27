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
#include "vela_node.h"
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
#include "max-17260-sensor.h"
#endif
#endif

#define FUEL_GAUGE_POLLING_INTERVAL_DEF 60

#include "sys/log.h"
#define LOG_MODULE "vela_node"
//#define LOG_LEVEL LOG_LEVEL_DBG
#define LOG_LEVEL LOG_LEVEL_NONE

PROCESS(vela_node_process, "main starter process for non-sink");

AUTOSTART_PROCESSES (&vela_node_process);

static struct ctimer polling_timer;
static uint8_t bat_data[12];
static uint8_t fuel_gauge_polling_interval_s = FUEL_GAUGE_POLLING_INTERVAL_DEF;

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
static uint16_t REP_CAP_mAh, REP_SOC_permillis, TTE_minutes, AVG_voltage_mV;
static int16_t AVG_current_100uA, AVG_temp_10mDEG;
#endif
#else
static uint16_t t1 = 1;
static uint16_t t2 = 2;
static uint16_t t3 = 3;
static uint16_t t4 = 4;
static uint16_t t5 = 5;
static uint16_t t6 = 6;
#endif

static void poll_fuel_gauge(void *not_used)
{
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
  REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
  REP_SOC_permillis = max_17260_sensor.value(
					     MAX_17260_SENSOR_TYPE_REP_SOC);
  TTE_minutes = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_TTE);
  AVG_current_100uA = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_I);
  AVG_voltage_mV = max_17260_sensor.value(
					  MAX_17260_SENSOR_TYPE_AVG_V);
  AVG_temp_10mDEG = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_TEMP);
  LOG_DBG("Remaining battery capacity = %d mAh ", REP_CAP_mAh);
  LOG_DBG_("or %d.%d %%. ", (uint16_t) (REP_SOC_permillis / 10), REP_SOC_permillis % 10);
  LOG_DBG_("Remaining runtime with this consumption: %d hours and %d minutes. ", TTE_minutes / 60, TTE_minutes % 60);
  if (AVG_current_100uA > 0) {
    LOG_DBG_("Avg current %d.%d mA ", AVG_current_100uA / 10, AVG_current_100uA % 10);
  } else {
    LOG_DBG_("Avg current -%d.%d mA ", -AVG_current_100uA / 10, -AVG_current_100uA % 10);
  }
  LOG_DBG_("Avg voltage = %d mV ", AVG_voltage_mV);
  if (AVG_temp_10mDEG > 0)
    {
      LOG_DBG_("Avg temp %d.%d °C ", AVG_temp_10mDEG / 100,
	       AVG_temp_10mDEG % 100);
    }
  else
    {
      LOG_DBG_("Avg temp %d.%d °C ", -AVG_temp_10mDEG / 100,
	       -AVG_temp_10mDEG % 100);
    }
  LOG_DBG_("\n");

  bat_data[0] = (uint8_t)(REP_CAP_mAh >> 8);
  bat_data[1] = (uint8_t)REP_CAP_mAh;
  bat_data[2] = (uint8_t)(REP_SOC_permillis >> 8);
  bat_data[3] = (uint8_t)REP_SOC_permillis;
  bat_data[4] = (uint8_t)(TTE_minutes >> 8);
  bat_data[5] = (uint8_t)TTE_minutes;
  bat_data[6] = (uint8_t)(AVG_current_100uA >> 8);
  bat_data[7] = (uint8_t)AVG_current_100uA;
  bat_data[8] = (uint8_t)(AVG_voltage_mV >> 8);
  bat_data[9] = (uint8_t)AVG_voltage_mV;
  bat_data[10] = (uint8_t)(AVG_temp_10mDEG >> 8);
  bat_data[11] = (uint8_t)AVG_temp_10mDEG;
#endif
#else
  t1++;
  t2++;
  t3++;
  t4++;
  t5++;
  t6++;
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
  bat_data[10] = (uint8_t)(t6 >> 8);
  bat_data[11] = (uint8_t)t6;

#endif

  if(fuel_gauge_polling_interval_s >= 10){
    ctimer_set(&polling_timer, fuel_gauge_polling_interval_s*CLOCK_SECOND, poll_fuel_gauge, NULL);
  }
  process_post(&vela_sender_process, event_bat_data_ready, bat_data);
}

static void set_fuel_gauge_interval(uint8_t interval_s){
  ctimer_stop(&polling_timer);
  if(interval_s >= 10){
    fuel_gauge_polling_interval_s = interval_s;
    ctimer_set(&polling_timer, fuel_gauge_polling_interval_s*CLOCK_SECOND, poll_fuel_gauge, NULL);
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(vela_node_process, ev, data)
{

  PROCESS_BEGIN();
  //    event_data_ready = process_alloc_event();
  set_battery_info_interval = process_alloc_event();

  LOG_INFO("main: started\n");

  // initialize and start the other threads
  vela_uart_init();
  LOG_INFO("Uart initialized\n");

  vela_sender_init();

  //set_fuel_gauge_interval(FUEL_GAUGE_POLLING_INTERVAL_DEF); //by default battery info are off
  // do something, probably related to the watchdog


  while (1)
    {
      PROCESS_WAIT_EVENT() ;

      if (ev == set_battery_info_interval)
        {
	  set_fuel_gauge_interval(((uint8_t*)data)[0]);
        }
    }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
