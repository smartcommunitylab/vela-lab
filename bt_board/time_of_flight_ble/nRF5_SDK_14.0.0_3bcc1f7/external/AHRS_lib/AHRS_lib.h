/*
 * ahrs_lib.h
 *
 *  Created on: 14/jan2013
 *     Authors:  Bojan Milosevic, Filippo Casamassima
 */

#ifndef AHRS_H_
#define AHRS_H_

#include <stdint.h>
#include <math.h>

#include "AHRS_conf.h"
#include "Math_Utils.h"

#define DEFAULT_SAMPLING_RATE_S	(float)0.02
#define C 						(float)299792458
#define C_GAIN 					(float)0.5
#define FURTHER_RTT_OFFSET_M	0//73+88+2
#define DEFAULT_O				(float)-0.000000280
#define DEFAULT_D_REF			5

typedef struct
{

	float x_hat[2];
	float P[4];
	float Q[4];
	float R[4];

} KF_status;


typedef struct
{
	KF_status kf_status;
	float O_runtime;
	float d_ref_runtime;
	float THETA;
	float alpha;
	float PTX;
} KF_t;

/*functions declaration*/
void KF_Update(KF_t* mkf, Sampled_Raw_Data* sData, float dT);
void KF_Init(KF_t* mkf, Sampled_Raw_Data *sData);
void KF_set_RTT_offset(KF_t* mkf, float O_runtime_new);
void KF_increase_RTT_offset_by_m(KF_t* mkf, float meters_to_compensate);
void KF_Rssi_Data_Convert(KF_t* mkf, int8_t rssi, float *distance);
//////////////////////////////



#endif /* AHRS_H_ */
