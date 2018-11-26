/*
 *Copyright 2017 Fondazione Bruno Kessler
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

#include "contiki.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "constraints.h"
#include "sys/ctimer.h"
#include "vela_i2c.h"
#include "max-17260-sensor.h"

typedef struct sensors_sensor sensors_sensor_t;

PROCESS(cc2650_i2c_process, "cc2650_i2c_process");

void vela_i2c_init()
{
    // start the uart process
    process_start(&cc2650_i2c_process, "cc2650 i2c process");
    return;

}

static uint8_t ii = 1;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_i2c_process, ev, data)
{
    static uint16_t REP_CAP_mAh, REP_SOC_permillis, TTE_minutes, AVG_voltage_mV;
    static int16_t AVG_current_10uA;
    PROCESS_BEGIN()
    ;

    REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
    REP_SOC_permillis = max_17260_sensor.value(
    MAX_17260_SENSOR_TYPE_REP_SOC);
    TTE_minutes = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_TTE);
    AVG_current_10uA = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_I);
    AVG_voltage_mV = max_17260_sensor.value(
    MAX_17260_SENSOR_TYPE_AVG_V);

    printf("POLLING: ");
    printf("Remaining battery capacity = %d mAh ", REP_CAP_mAh);
    printf("or %d.%d %%. ", (uint16_t) (REP_SOC_permillis / 10),
           REP_SOC_permillis % 10);
    printf("Remaining runtime with this consumption: %d hours and %d minutes. ",
           TTE_minutes / 60, TTE_minutes % 60);
    if (AVG_current_10uA > 0)
    {
        printf("Avg current %d.%d mA ", AVG_current_10uA / 100,
               AVG_current_10uA % 100);
    }
    else
    {
        printf("Avg current -%d.%d mA ", -AVG_current_10uA / 100,
               -AVG_current_10uA % 100);
    }
    printf("Avg voltage = %d mV ", AVG_voltage_mV);
    printf("\n");

    while (1)
    {
        ii = 5;
        SENSORS_ACTIVATE(max_17260_sensor);

        while (ii--)
        {
            //TODO: do something useful
            PROCESS_WAIT_EVENT()
            ;
            if (ev == sensors_event)
            {
                if (data == (void*) &max_17260_sensor)
                {
                    REP_CAP_mAh = max_17260_sensor.value(
                    MAX_17260_SENSOR_TYPE_REP_CAP);
                    REP_SOC_permillis = max_17260_sensor.value(
                    MAX_17260_SENSOR_TYPE_REP_SOC);
                    TTE_minutes = max_17260_sensor.value(
                    MAX_17260_SENSOR_TYPE_TTE);
                    AVG_current_10uA = max_17260_sensor.value(
                    MAX_17260_SENSOR_TYPE_AVG_I);
                    AVG_voltage_mV = max_17260_sensor.value(
                    MAX_17260_SENSOR_TYPE_AVG_V);
                    printf("EVENT_BASED: ");
                    printf("Remaining battery capacity = %d mAh ", REP_CAP_mAh);
                    printf("or %d.%d %%. ", (uint16_t) (REP_SOC_permillis / 10),
                           REP_SOC_permillis % 10);
                    printf("Remaining runtime with this consumption: %d hours and %d minutes. ",
                           TTE_minutes / 60, TTE_minutes % 60);
                    if (AVG_current_10uA > 0)
                    {
                        printf("Avg current %d.%d mA ", AVG_current_10uA / 100,
                               AVG_current_10uA % 100);
                    }
                    else
                    {
                        printf("Avg current -%d.%d mA ",
                               -AVG_current_10uA / 100,
                               -AVG_current_10uA % 100);
                    }
                    printf("Avg voltage = %d mV ", AVG_voltage_mV);
                    printf("\n");
                    //ii--;
                }
            }
        }

//    printf("MAIN LOOP FINISH!!!\n");
        SENSORS_DEACTIVATE(max_17260_sensor);

        REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
        REP_SOC_permillis = max_17260_sensor.value(
        MAX_17260_SENSOR_TYPE_REP_SOC);
        TTE_minutes = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_TTE);
        AVG_current_10uA = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_I);
        AVG_voltage_mV = max_17260_sensor.value(
        MAX_17260_SENSOR_TYPE_AVG_V);

        printf("POLLING: ");
        printf("Remaining battery capacity = %d mAh ", REP_CAP_mAh);
        printf("or %d.%d %%. ", (uint16_t) (REP_SOC_permillis / 10),
               REP_SOC_permillis % 10);
        printf("Remaining runtime with this consumption: %d hours and %d minutes. ",
               TTE_minutes / 60, TTE_minutes % 60);
        if (AVG_current_10uA > 0)
        {
            printf("Avg current %d.%d mA ", AVG_current_10uA / 100,
                   AVG_current_10uA % 100);
        }
        else
        {
            printf("Avg current -%d.%d mA ", -AVG_current_10uA / 100,
                   -AVG_current_10uA % 100);
        }
        printf("Avg voltage = %d mV ", AVG_voltage_mV);
        printf("\n");
    }
PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
