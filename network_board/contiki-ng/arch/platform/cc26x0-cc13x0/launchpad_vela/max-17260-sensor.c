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
 * \addtogroup sensortag-cc26xx-hdc-sensor
 * @{
 *
 * \file
 *  Driver for the Sensortag HDC sensor
 */
/*---------------------------------------------------------------------------*/
#include "contiki-conf.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "max-17260-sensor.h"
#include "sensor-common.h"
#include "board-i2c.h"

#include "ti-lib.h"

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
/*---------------------------------------------------------------------------*/
#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif
/*---------------------------------------------------------------------------*/
#define R_SENSE_mOhm                    20 //TODO: This define should be in board.h maybe.. resistance of the sensing resistor (20mOhm)
#define BATTERY_CAPACITY_mAh            6600 //TODO: This define should be in board.h maybe..
#define CHARGE_CURRENT_mA               500 //this is when usb is used. for the solar panel I don't know how to calculate it
#define CHARGE_TERMINATION_CURRENT_mAh  CHARGE_CURRENT_mA*7/100 //(from MCP73831T datasheet, charge is considered terminated when current goes below 7.5% of Ireg)
#define V_EMPTY_W_LOAD_mV               2800 //this is the when the under voltage protection of the BQ29700 enters
#define V_RECOVERY_mV                   3800 //suggested value in the MAX17260 datasheet
#define MAX_BATT_VOLTAGE_mV             4200

/* Sensor I2C address */
#define SENSOR_I2C_ADDRESS              0x36

///* Registers */
#define MAX17260_REG_STATUS             0x00
#define MAX17260_REG_REP_CAP            0x05
#define MAX17260_REG_REP_SOC            0x06
#define MAX17260_REG_TEMP               0x08
#define MAX17260_REG_AVG_CURRENT        0x0B
#define MAX17260_REG_COMMAND            0x60
#define MAX17260_REG_FULL_CAP_REP       0x10
#define MAX17260_REG_TTE                0x11
#define MAX17260_REG_AVG_TEMP           0x16
#define MAX17260_REG_DESIGN_CAP         0x18
#define MAX17260_REG_AVG_V_CELL         0x19
#define MAX17260_REG_I_CHG_TERM         0x1E
#define MAX17260_REG_FULL_CAP_NOM       0x23
#define MAX17260_FILTER_CFG             0x29
#define MAX17260_REG_DEV_ID             0xB2
#define MAX17260_REG_HIB_CFG            0xBA
#define MAX17260_REG_V_EMPTY            0x3A
#define MAX17260_REG_FSTAT              0x3D
#define MAX17260_REG_MODEL_CFG          0xDB

#define MAX17260_FILTER_CFG_CURRENT_MASK 0x000F

///* Fixed values */
#define MAX17260_REG_STATUS_RESET_VALUE     0x8082

#define MAX17260_TIME_LSB_S             5.625

#define MAX17260_STATUS_POR(a)          ((a>>1) & 0x0001)
#define MAX17260_FSTAT_DNR(a)           (a & 0x0001)
#define MAX17260_MODEL_CFG_REFRESH(a)   ((a>>15) & 0x0001)

#define mAh_TO_MAX17260_REG(a)          ((a*1000*R_SENSE_mOhm)/(1000*5))
#define MAX17260_REG_TO_mAh(a)          ((a*1000*5)/(1000*R_SENSE_mOhm))
#define MAX17260_REG_TO_PERMILLS(a)     ((a*10)/256)
#define MAX17260_REG_TO_MINUTES(a)      ((a*MAX17260_TIME_LSB_S)/60)
#define MAX17260_REG_TO_100uA(a)        (int16_t)((int16_t)a)*((10*1000*1.5625)/(1000*R_SENSE_mOhm))
#define MAX17260_REG_TO_mV(a)           (a*1.25/16)
#define MAX17260_REG_TO_TEMP_10mDEG(a)  (int16_t)((((int16_t)a)*100)/256)

#define MAX17260_CURRENT_FILT_TIME_C_S  45

#define DESIGN_CAP_VALUE                mAh_TO_MAX17260_REG(BATTERY_CAPACITY_mAh)
#if(DESIGN_CAP_VALUE>65535)
#warning Battery capacity is too large to be written in the MAX17260 registers. Reduce R_SENSE or BATTERY_CAPACITY_mAh
#endif
#define MAX17260_DESIGN_CAP_VALUE       (uint16_t)DESIGN_CAP_VALUE

#define I_CHG_TERM_VALUE                mAh_TO_MAX17260_REG(CHARGE_TERMINATION_CURRENT_mAh)
#if(MAX17260_I_CHG_TERM_VALUE>65535)
#warning End of charge current is too large to be written in the MAX17260 registers. Something is wrong!
#endif
#define MAX17260_I_CHG_TERM_VALUE       (uint16_t)I_CHG_TERM_VALUE

#define REG_V_EMPTY_VE                  V_EMPTY_W_LOAD_mV/10    //VE is in 10mV resolution, see MAX17260 datasheet/user guide
#if(REG_V_EMPTY_VE > 5110)
#warning V_EMPTY_W_LOAD_mV is set too high!!
#endif
#define REG_V_EMPTY_VR                  V_RECOVERY_mV/40    //VE is in 10mV resolution, see MAX17260 datasheet/user guide
#if(REG_V_EMPTY_VR > 5080)
#warning V_RECOVERY_mV is set too high!!
#endif
#define MAX17260_V_EMPTY_VALUE          (uint16_t)((0xFF80 & (I_CHG_TERM_VALUE<<7)) | (0x007F & REG_V_EMPTY_VR))

#if MAX_BATT_VOLTAGE_mV>4275
#define MAX17260_MODEL_CFG_VALUE        0x8400              // see MAX17260 datasheet/user guide
#else
#define MAX17260_MODEL_CFG_VALUE        0x8000              // see MAX17260 datasheet/user guide
#endif

#define MAX_NUBMER_OF_INITIALIZATION_TIRALS 5

#define SENSOR_POLLING_INTERVAL CLOCK_SECOND*5
#define POR_CHECK_INTERVAL      CLOCK_SECOND*60

/* Sensor selection/deselection */
#define SENSOR_SELECT()     board_i2c_select(BOARD_I2C_INTERFACE_0, SENSOR_I2C_ADDRESS)
#define SENSOR_DESELECT()   board_i2c_deselect()
/*---------------------------------------------------------------------------*/
/* Byte swap of 16-bit register value */
#define HI_UINT16(a) (((a) >> 8) & 0xFF)
#define LO_UINT16(a) ((a) & 0xFF)

#define SWAP(v) ((LO_UINT16(v) << 8) | HI_UINT16(v))
/*---------------------------------------------------------------------------*/
/* Raw data as returned from the sensor (Big Endian) */
typedef struct sensor_data
{
    uint16_t id;
    uint16_t Status_reg;
    uint16_t RepCap_reg;
    uint16_t RepSoc_reg;
    uint16_t TTE_reg;
} sensor_data_t;

/* Raw data, little endian */
//static uint16_t raw_temp;
//static uint16_t raw_hum;
/*---------------------------------------------------------------------------*/
static bool success;
static sensor_data_t data;
/*---------------------------------------------------------------------------*/
static int enabled = MAX_17260_SENSOR_STATUS_DISABLED;

static struct ctimer polling_timer;
static struct ctimer periodic_por_check_timer;

static void read_handler();
static void periodic_por_check_handler(void *not_used);
static bool max_17260_getReg(uint16_t *reg_value, uint8_t reg);
static bool max_17260_setReg(uint16_t *reg_value, uint8_t reg);
static bool max_17260_setAndVerifyReg(uint16_t* reg_value, uint8_t reg);

//uint16_t mAh_TO_MAX17260_REG(uint16_t a){
//    return ((a*1000*R_SENSE_mOhm)/(1000*5));
//}

//static uint16_t MAX17260_REG_TO_mAh(uint16_t a){
//    return ((a*1000*5)/(1000*R_SENSE_mOhm));
//}
//
//static uint16_t MAX17260_REG_TO_PERMILLS(uint16_t a){
//    return  ((a*10)/256);
//}
//
//static uint16_t MAX17260_REG_TO_MINUTES(uint16_t a){
//    return ((a*MAX17260_TIME_LSB_S)/60);
//}
//
//static int16_t MAX17260_REG_TO_10uA(uint16_t a){
//    int16_t ret = ((int16_t)a)*((100*1000*1.5625)/(1000*R_SENSE_mOhm));
//    return ret;
//}
/*---------------------------------------------------------------------------*/
/**
 * \brief       Initialise the humidity sensor driver
 * \return      True if I2C operation successful
 */
static bool sensor_init(void) //based on User Guide 6595: MAX1726x Software Implementation Guide
{
    uint16_t val;
    uint8_t initializing_trials = MAX_NUBMER_OF_INITIALIZATION_TIRALS;
    success = true;

    ctimer_set(&periodic_por_check_timer, POR_CHECK_INTERVAL,
               periodic_por_check_handler,
               NULL);

    while (initializing_trials)
    {

        // Step 0: Check for POR
        if (!max_17260_getReg(&val, MAX17260_REG_STATUS))
        {
            PRINTF("max-17260-sensor: problem while reading writing STATUS register\n");
        }
        data.Status_reg = val;
        PRINTF("max-17260-sensor: status: 0x%04X success=0x%02X\n",data.Status_reg, success);

        if (MAX17260_STATUS_POR(data.Status_reg))
        {
            uint8_t loop = 1;
            while (loop)
            { //Step 1. Delay until FSTAT.DNR bit == 0
                max_17260_getReg(&val, MAX17260_REG_STATUS);
                if ((MAX17260_FSTAT_DNR(val) != 0) || loop > 100)
                { //after 1 s the execution exits from the loop regardless the register value
                    clock_delay(CLOCK_SECOND / 100);
                    loop++;
                }
                else
                {
                    loop = 0;
                }
            }

            // Step 2: Initialize Configuration
            uint16_t HibCFG = 0;
            if (!max_17260_getReg(&HibCFG, MAX17260_REG_HIB_CFG))
            {
                PRINTF("max-17260-sensor: problem while reading MAX17260_REG_HIB_CFG register\n");
            }
            val = 0x0090;
            if (!max_17260_setReg(&val, MAX17260_REG_COMMAND))
            {
                PRINTF("max-17260-sensor: problem while writing COMMAND register\n");
            }
            val = 0x0000;
            if (!max_17260_setReg(&val, MAX17260_REG_HIB_CFG))
            {
                PRINTF("max-17260-sensor: problem while writing MAX17260_REG_HIB_CFG register\n");
            }
            val = 0x0060;
            if (!max_17260_setReg(&val, MAX17260_REG_COMMAND))
            {
                PRINTF("max-17260-sensor: problem while writing COMMAND register\n");
            }
            // Step 2.1:
            val = MAX17260_DESIGN_CAP_VALUE;
            if (!max_17260_setReg(&val, MAX17260_REG_DESIGN_CAP))
            {
                PRINTF("max-17260-sensor: problem while writing MAX17260_REG_DESIGN_CAP register\n");
            }

            val = MAX17260_I_CHG_TERM_VALUE;
            if (!max_17260_setReg(&val, MAX17260_REG_I_CHG_TERM))
            {
                PRINTF("max-17260-sensor: problem while writing MAX17260_REG_I_CHG_TERM register\n");
            }

            val = MAX17260_V_EMPTY_VALUE;
            if (!max_17260_setReg(&val, MAX17260_REG_V_EMPTY))
            {
                PRINTF("max-17260-sensor: problem while writing MAX17260_REG_V_EMPTY register\n");
            }

            val = MAX17260_MODEL_CFG_VALUE;
            if (!max_17260_setReg(&val, MAX17260_REG_MODEL_CFG))
            {
                PRINTF("max-17260-sensor: problem while writing MAX17260_REG_MODEL_CFG register\n");
            }

            loop = 1;
            while (loop)
            { //Delay until ModelCFG.Refresh bit == 0
                max_17260_getReg(&val, MAX17260_REG_MODEL_CFG);
                if ((MAX17260_MODEL_CFG_REFRESH(val) != 0) || loop > 100)
                { //after 1 s the execution exits from the loop regardless the register value
                    clock_delay(CLOCK_SECOND / 100);
                    loop++;
                }
                else
                {
                    loop = 0;
                }
            }

            if (!max_17260_setReg(&HibCFG, MAX17260_REG_HIB_CFG))
            {
                PRINTF("max-17260-sensor: problem while MAX17260_REG_HIB_CFG register\n");
            }

            //CHANGE THE CURRENT SENSE FILTER CONFIGURATION
            //TODO:the equation on the datasheet doesn't make sense, then I just set the maximum value
            if (max_17260_getReg(&val, MAX17260_FILTER_CFG))
            {
                val &= !MAX17260_FILTER_CFG_CURRENT_MASK; //clear the current time constant field
                uint8_t curr = MAX17260_FILTER_CFG_CURRENT_MASK;
                val |= (curr & MAX17260_FILTER_CFG_CURRENT_MASK);
                if (!max_17260_setReg(&val, MAX17260_FILTER_CFG))
                {
                    PRINTF("max-17260-sensor: problem while writing MAX17260_FILTER_CFG register\n");
                }
            }else{
                PRINTF("max-17260-sensor: problem while reading MAX17260_FILTER_CFG register\n");
            }

            //Step 3: Initialization Complete
            if (!max_17260_getReg(&val, MAX17260_REG_STATUS))
            {
                PRINTF("max-17260-sensor: problem while reading writing STATUS register\n");
            }
            data.Status_reg = val;
            PRINTF("max-17260-sensor: status: 0x%04X success=0x%02X\n",data.Status_reg, success);

            loop = 0;
            while (loop)
            {
                val &= 0xFFFD;
                if (max_17260_setAndVerifyReg(&val, MAX17260_REG_MODEL_CFG)
                        || loop > 3)
                { //after 3 trials the execution exits from the loop regardless the register value
                    clock_delay(CLOCK_SECOND / 100);
                    loop++;
                }
                else
                {
                    loop = 0;
                }
            }
        }

        //Step 3.2: Check for MAX1726x Reset
        if (!max_17260_getReg(&val, MAX17260_REG_STATUS))
        {
            PRINTF("max-17260-sensor: problem while reading writing STATUS register\n");
        }
        data.Status_reg = val;
        if (MAX17260_STATUS_POR(data.Status_reg) == 0)
        {
            initializing_trials = 0; //this stops the external while loop
        }
        else
        {
            initializing_trials--;
            PRINTF("max-17260-sensor: initialization failed (POR bit not clear), I will retry for %d times\n",initializing_trials);
        }
    }
    PRINTF("max-17260-sensor: status: 0x%04X success=0x%02X\n",data.Status_reg, success);
    return MAX17260_STATUS_POR(data.Status_reg) == 0;
}

static void periodic_por_check_handler(void *not_used)
{
    PRINTF("Performing periodic POR check\n");
    sensor_init();
}

static bool max_17260_getReg(uint16_t *reg_value, uint8_t reg)
{
//    if (enabled != MAX_17260_SENSOR_STATUS_INITIALISED)
//    {
//        return false;
//    }

    if (success)
    {


        SENSOR_SELECT();
        success = board_i2c_write_read(&reg, sizeof(reg), (uint8_t*)reg_value, sizeof(*reg_value));
        SENSOR_DESELECT();

        if (!success)
        {
            PRINTF("max-17260-sensor: I2C problem in max_17260_getReg!\n");
            return false;
        }
        return true;
    }
    return false;
}

static bool max_17260_setReg(uint16_t* reg_value, uint8_t reg)
{
//    if (enabled != MAX_17260_SENSOR_STATUS_INITIALISED)
//    {
//        return false;
//    }

    if (success)
    {
        SENSOR_SELECT();

        success = board_i2c_write_single(reg);

        if (!success)
        {
            PRINTF("max-17260-sensor: I2C problem in max_17260_setReg!\n");
            SENSOR_DESELECT();
            return false;
        }

        success = board_i2c_write((uint8_t *) reg_value, sizeof(reg_value));

        SENSOR_DESELECT();

        if (!success)
        {
            PRINTF("max-17260-sensor: I2C problem in max_17260_setReg!\n");
            return false;
        }
        return true;
    }
    return false;
}

static bool max_17260_setAndVerifyReg(uint16_t* reg_value, uint8_t reg)
{
    max_17260_setReg(reg_value, reg);
    uint16_t val = 0;
    max_17260_getReg(&val, reg);
    if (val == *reg_value)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static bool max_17260_get_avg_temp(uint16_t *out)
{
    bool ret = max_17260_getReg(out, MAX17260_REG_AVG_TEMP);
    PRINTF("max-17260-sensor: Raw AVG_TEMP 0x%04X, success=0x%02X\n",*out,success);
    return ret;
}

static bool max_17260_get_avg_v(uint16_t *out)
{
    bool ret = max_17260_getReg(out, MAX17260_REG_AVG_V_CELL);
    PRINTF("max-17260-sensor: Raw AVG_V_CELL 0x%04X, success=0x%02X\n",*out,success);
    return ret;
}

static bool max_17260_get_avg_i(uint16_t *out)
{
    bool ret = max_17260_getReg(out, MAX17260_REG_AVG_CURRENT);
    PRINTF("max-17260-sensor: Raw AVG_I 0x%04X, signed raw AVG_I %d, success=0x%02X\n",*out,(int16_t)*out,success);
    return ret;
}

static bool max_17260_get_repCap(uint16_t *out)
{
    bool ret = max_17260_getReg(out, MAX17260_REG_REP_CAP);
    PRINTF("max-17260-sensor: Raw REP_CAP 0x%04X, success=0x%02X\n",*out,success);
    return ret;
}

static bool max_17260_get_repSoC(uint16_t *out)
{
    bool ret = max_17260_getReg(out, MAX17260_REG_REP_SOC);
    PRINTF("max-17260-sensor: Raw REP_SOC 0x%04X, success=0x%02X\n",*out,success);
    return ret;
}

static bool max_17260_get_tte(uint16_t *out)
{
    bool ret = max_17260_getReg(out, MAX17260_REG_TTE);
    PRINTF("max-17260-sensor: Raw TTE 0x%04X, success=0x%02X\n",*out,success);
    return ret;
}

//MAX17260_REG_REP_CAP and MAX17260_REG_REP_SOC are consecutive, then a read blob operation can be used instead of two byte read operations
static void max_17260_read_repCap_and_repSoC(void)
{
    uint32_t value32;

    SENSOR_SELECT();
    uint8_t reg=MAX17260_REG_REP_CAP;
    success = board_i2c_write_read(&reg, sizeof(reg), (uint8_t*)(&value32), sizeof(value32));
    SENSOR_DESELECT();

    if (success)
    {
        PRINTF("max-17260-sensor: read_data: value = 0x%08X\n",
               (unsigned int )value32);

        data.RepCap_reg = (uint16_t) (value32 & 0xFFFF);
        data.RepSoc_reg = (uint16_t) ((value32 >> 16) & 0xFFFF);

        PRINTF("max-17260-sensor: Raw RepCap_reg: 0x%04X\n", data.RepCap_reg);PRINTF(
                "max-17260-sensor: Raw RepSoc_reg: 0x%04X\n", data.RepSoc_reg);
    }
}

/*---------------------------------------------------------------------------*/
/**
 * \brief       Start measurement
 */
static void start(void *not_used)
{
    PRINTF("max-17260-sensor: start(). success=0x%02X\n",success);
    ctimer_set(&polling_timer, SENSOR_POLLING_INTERVAL, start, NULL);

    if (success)
    {
        read_handler();

        if (!success)
        {
            PRINTF("max-17260-sensor: I2C problem during start()!\n");
        }
    }
}

/*---------------------------------------------------------------------------*/
static void read_handler()
{
    uint16_t value;
////    max_17260_read_repCap_and_repSoC();
//    max_17260_get_repCap(&value);
//    if(success){
//        data.RepCap_reg=value;
//    }
//    max_17260_get_repSoC(&value);
//    if(success){
//        data.RepSoc_reg=value;
//    }
    max_17260_read_repCap_and_repSoC();
    max_17260_get_tte(&value);
    if(success){
        data.TTE_reg=value;
    }

    if (success)
    {
        enabled = MAX_17260_SENSOR_STATUS_READINGS_READY;
        sensors_changed(&max_17260_sensor);
    }

}

static int value(int type)
{
    uint16_t val;
    switch (type)
    {
    case MAX_17260_SENSOR_TYPE_REP_CAP:
        if (enabled != MAX_17260_SENSOR_STATUS_READINGS_READY)
        {
            if (!max_17260_get_repCap(&val))
            {
                PRINTF("max-17260-sensor: polling based REP_CAP read failed!\n");
                return CC26XX_SENSOR_READING_ERROR;
            }
        }
        else
        {
            val = data.RepCap_reg;
        }
        return (uint16_t)MAX17260_REG_TO_mAh(val); //returned value is in 1 mAh units
        break;
    case MAX_17260_SENSOR_TYPE_REP_SOC:
        if (enabled != MAX_17260_SENSOR_STATUS_READINGS_READY)
        {
            if (!max_17260_get_repSoC(&val))
            {
                PRINTF("max-17260-sensor: polling based REP_SOC read failed!\n");
                return CC26XX_SENSOR_READING_ERROR;
            }
        }
        else
        {
            val = data.RepSoc_reg;
        }
        return (uint16_t)MAX17260_REG_TO_PERMILLS(val); //returned value is in 0.1% units
        break;
    case MAX_17260_SENSOR_TYPE_TTE:
        if (enabled != MAX_17260_SENSOR_STATUS_READINGS_READY)
        {
            if (!max_17260_get_tte(&val))
            {
                PRINTF("max-17260-sensor: polling based TTE read failed!\n");
                return CC26XX_SENSOR_READING_ERROR;
            }
        }
        else
        {
            val = data.TTE_reg;
        }
        return (uint16_t)MAX17260_REG_TO_MINUTES(val); //returned value is in 1 minute units
        break;
    case MAX_17260_SENSOR_TYPE_AVG_I:
        if (!max_17260_get_avg_i(&val))
        {
            return CC26XX_SENSOR_READING_ERROR;
        }
        else
        {
            return MAX17260_REG_TO_100uA(val);
        }
        break;
    case MAX_17260_SENSOR_TYPE_AVG_V:
        if (!max_17260_get_avg_v(&val))
        {
            return CC26XX_SENSOR_READING_ERROR;
        }
        else
        {
            return MAX17260_REG_TO_mV(val);
        }
        break;
    case MAX_17260_SENSOR_TYPE_AVG_TEMP:
        if (!max_17260_get_avg_temp(&val))
        {
            return CC26XX_SENSOR_READING_ERROR;
        }
        else
        {
            return MAX17260_REG_TO_TEMP_10mDEG(val); //returned value is in 10m°C unit
        }
        break;
    default:
        return CC26XX_SENSOR_READING_ERROR;
        break;
    }
    return CC26XX_SENSOR_READING_ERROR;
}

static int configure(int type, int enable)
{
    PRINTF("max-17260-sensor: configure(...): type=0x%02X, enable=0x%02X, success=0x%02X\n", type, enable, success);
    switch (type)
    {
    case SENSORS_HW_INIT:
        memset(&data, 0, sizeof(data));

        if (sensor_init())
        {
            enabled = MAX_17260_SENSOR_STATUS_INITIALISED;
            PRINTF("max-17260-sensor: sensor initialized!\n");
        }
        else
        {
            enabled = MAX_17260_SENSOR_STATUS_ERROR;
            PRINTF("max-17260-sensor: sensor NOT CORRECTLY initialized!\n");
        }
        break;
    case SENSORS_ACTIVE:
        /* Must be initialised first */
        if (enabled == MAX_17260_SENSOR_STATUS_DISABLED)
        {
            return MAX_17260_SENSOR_STATUS_DISABLED;
        }

        if (enable)// && enabled != MAX_17260_SENSOR_STATUS_ERROR)
        {
            enabled = MAX_17260_SENSOR_STATUS_TAKING_FIRST_READING;
            start(0); //argument is dummy, never used
        }
        else
        {
            ctimer_stop(&polling_timer);
            enabled = MAX_17260_SENSOR_STATUS_INITIALISED;
        }
        break;
    default:
        break;
    }
    return enabled;
}
/*---------------------------------------------------------------------------*/
/**
 * \brief Returns the status of the sensor
 * \param type SENSORS_ACTIVE or SENSORS_READY
 * \return One of the SENSOR_STATUS_xyz defines
 */
static int status(int type)
{
    switch (type)
    {
    case SENSORS_ACTIVE:
    case SENSORS_READY:
        return enabled;
        break;
    default:
        break;
    }
    return MAX_17260_SENSOR_STATUS_DISABLED;
}
/*---------------------------------------------------------------------------*/
SENSORS_SENSOR(max_17260_sensor, MAX_17260_SENSOR, value, configure, status);
/*---------------------------------------------------------------------------*/
/** @} */
