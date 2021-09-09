/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
/**
 * \addtogroup sensortag-cc26xx-peripherals
 * @{
 *
 * \defgroup sensortag-cc26xx-hdc-sensor SensorTag 2.0 TI HDC1000 Sensor
 *
 * Due to the time required for the sensor to startup, this driver is meant to
 * be used in an asynchronous fashion. The caller must first activate the
 * sensor by calling SENSORS_ACTIVATE(). This will trigger the sensor's startup
 * sequence, but the call will not wait for it to complete so that the CPU can
 * perform other tasks or drop to a low power mode. Once the sensor has taken
 * readings, it will automatically go back to low power mode.
 *
 * Once the sensor is stable, the driver will retrieve readings from the sensor
 * and latch them. It will then generate a sensors_changed event.
 *
 * The user can then retrieve readings by calling .value() and by passing
 * either HDC_1000_SENSOR_TYPE_TEMP or HDC_1000_SENSOR_TYPE_HUMIDITY as the
 * argument. Multiple calls to value() will not trigger new readings, they will
 * simply return the most recent latched values.
 *
 * The user can query the sensor's status by calling .status()
 *
 * To get a fresh reading, the user must trigger a new reading cycle by calling
 * SENSORS_ACTIVATE().
 * @{
 *
 * \file
 * Header file for the Sensortag TI HDC1000 sensor
 */
/*---------------------------------------------------------------------------*/
#ifndef MAX_17260_SENSOR_H
#define MAX_17260_SENSOR_H
/*---------------------------------------------------------------------------*/
#include "lib/sensors.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
/*---------------------------------------------------------------------------*/
#define MAX_17260_SENSOR "MAX17260"
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#define MAX_17260_SENSOR_TYPE_REP_CAP           1
#define MAX_17260_SENSOR_TYPE_REP_SOC           2
#define MAX_17260_SENSOR_TYPE_TTE               3
#define MAX_17260_SENSOR_TYPE_AVG_I             4
#define MAX_17260_SENSOR_TYPE_AVG_V             5
#define MAX_17260_SENSOR_TYPE_AVG_TEMP          6
/*---------------------------------------------------------------------------*/
/**
 * \name HDC1000 driver states
 * @{
 */
#define MAX_17260_SENSOR_STATUS_DISABLED         0 /**< Not initialised */
#define MAX_17260_SENSOR_STATUS_INITIALISED      1 /**< Initialised but idle */
#define MAX_17260_SENSOR_STATUS_TAKING_FIRST_READING  2 /**< Readings in progress */
#define MAX_17260_SENSOR_STATUS_READINGS_READY   3 /**< Both readings ready */
#define MAX_17260_SENSOR_STATUS_ERROR            80 /**< Both readings ready */
/** @} */

//static bool max_17260_getReg(uint16_t *reg_value, uint8_t reg);
//static bool max_17260_setReg(uint16_t *reg_value, uint8_t reg);
//static bool max_17260_setAndVerifyReg(uint16_t* reg_value, uint8_t reg);

/*---------------------------------------------------------------------------*/
extern const struct sensors_sensor max_17260_sensor;
/*---------------------------------------------------------------------------*/
#endif /* MAX_17260_SENSOR_H */
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
