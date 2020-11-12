/*
 * ahrs_lib.h
 *
 *  Created on: 14/jan2013
 *     Authors:  Bojan Milosevic, Filippo Casamassima
 */

#ifndef AHRS_CONF_H_
#define AHRS_CONF_H_

//#include <stdlib.h>
#include <stdint.h>
#include <math.h>
//#include "stm32f10x.h"

#include "DataTypes.h"

//#include "Math_Utils.h"

//#define AHRS

// #ifndef ML_LIB_H_

// typedef struct
// {

//   int16_t Acc[3];
//   int16_t Gyr[3];
//   int16_t Mag[3];

// #ifdef _PRESS
// //  s16 Temp;
// //  u16 Press;
// #endif
// //  float uGain[11]; /*AccX AccY AccZ - GyroX GyroY GyroZ - MagnX MagnY MagnZ - Press - Temp*/
// //  s16 sOffset[11];      /*AccX AccY AccZ - GyroX GyroY GyroZ - MagnX MagnY MagnZ - Press - Temp*/

// } Sensor_RawData;

// /**
//  * @struct
//  * @brief sensor calibrated and converted data struct
//  */
// typedef struct
// {

//   float Acc[3];
//   float Gyr[3];
//   float Mag[3];

// #ifdef _PRESS
// //  float Temp;
// //  float Press;
// #endif

// //  float uGain[11]; /*AccX AccY AccZ - GyroX GyroY GyroZ - MagnX MagnY MagnZ - Press - Temp*/
// //  s16 sOffset[11];      /*AccX AccY AccZ - GyroX GyroY GyroZ - MagnX MagnY MagnZ - Press - Temp*/

// } Sensor_Data;

// typedef struct
// {

//   float AccOffset[3];
//   float GyrOffset[3];
//   float MagOffset[3];

//   float AccGain[9];
//   float GyrGain[9];
//   float MagGain[9];

// #ifdef _PRESS
// //  float Temp;
// //  float Press;
// #endif

// } Calibration_Data;

// typedef struct
// {

//   float q[4];
//   float E[3];
//   float R[9];

// //  float AccGain[9];
// //  float GyrGain[9];
// //  float MagGain[9];

// #ifdef _PRESS
// //  float Temp;
// //  float Press;
// #endif

// } Orientation_Data;


// #endif


/*general constants and parameters definition*/
#define		PI				(float)3.141592653589793
#define		r2d 			180/PI
#define 	d2r				PI/180
//#define  	dt 				0.05 //output data rate

#define 	SGN(x)  		( ((x) < 0) ?  -1 : ( ((x) == 0 ) ? 0 : 1) )
#define 	ABS(x)  		( ((x) < 0) ?  -x : ( ((x) == 0 ) ? 0 : x) )
#define 	dimA			18
#define		dim2A			18*18


#define 	    ge				9.822
#define		me				0.5
#define		meo				0.9

// const gn matrix for gnx init issue
#define   gn_0      0
#define   gn_1      0
#define   gn_2     -ge
// const mn matrix for mnx init issue
#define   mn_0      me
#define   mn_1      0
#define   mn_2     -meo


//#define		gyroSens		8.75
//#define		Acc_Calibr		0.0005988//9.81 * pow(2,-14)
//
//#define		Acc_Offset_x	104.598//133150722
//#define		Acc_Offset_y	-314.451//875216604
//#define		Acc_Offset_z	26.633//9139925501
//
//#define		Gyro_Offset_x	80.245
//#define		Gyro_Offset_y	-14.7505
//#define		Gyro_Offset_z	-69.265
//
//#define		Mag_Gain_x		0.00234//706770526724
//#define		Mag_Gain_y		0.00242//422720122682
//#define		Mag_Gain_z		0.00226//509409399240
//
//#define		Mag_Offset_x	7.60248//124523434
//#define		Mag_Offset_y	-6.17936//513592715
//#define		Mag_Offset_z	3.18983//059595225

//#define		Acc_Offset_x	-2.4475
//#define		Acc_Offset_y	16.6104
//#define		Acc_Offset_z	-41.1757
//#define		Gyro_Offset_x	-10.9027
//#define		Gyro_Offset_y	-2.4137
//#define		Gyro_Offset_z	-2.7747
//#define		Mag_Gain_x		0.00276223
//#define		Mag_Gain_y		0.00269022
//#define		Mag_Gain_z		0.00317052
//#define		Mag_Offset_x	0.2914
//#define		Mag_Offset_y	0.1681
//#define		Mag_Offset_z	0.1696

/*kalman filter parameters*/

#define GAIN_G 10
#define GAIN_A 2000
#define GAIN_M 100

// noise params

// gyro bias noise
#define omega_g_x  3E-8
#define omega_g_y  3E-8
#define omega_g_z  3E-8

// gyr noise
#define nu_g_x  0.028
#define nu_g_y  0.028
#define nu_g_z  0.036

// acc bias noise
#define omega_a_x  3E-6
#define omega_a_y  3E-6
#define omega_a_z  3E-6

//// acc noise
//#define nu_a_x  2
//#define nu_a_y  2.8
//#define nu_a_z  7.1
//
//// mag noise
//#define nu_m_x  0.84
//#define nu_m_y  0.81
//#define nu_m_z  1.59


//#define		omega_g_x		0.0028
//#define		omega_g_y		0.0029
//#define		omega_g_z		0.0032
//
//#define		nu_g_x			0.0028
//#define		nu_g_y			0.0029
//#define		nu_g_z			0.0032
//
//#define		omega_a_x		0.42
//#define		omega_a_y		0.38
//#define		omega_a_z		2.24
//
//#define		nu_a_x			0.42
//#define		nu_a_y			0.38
//#define		nu_a_z			2.24
//
//#define		nu_m_x			0.0098
//#define		nu_m_y			0.0068
//#define		nu_m_z			0.0129


//////////////////////////////////////////////////////////
// KF noise params
#define omega_q0 4E-7
#define omega_q1 4E-7
#define omega_q2 4E-7
#define omega_q3 4E-7

#define nu_a_x  5E-4
#define nu_a_y  5E-4
#define nu_a_z  5E-4

#define nu_m_x  6E-6
#define nu_m_y  6E-6
#define nu_m_z  6E-6


#endif
