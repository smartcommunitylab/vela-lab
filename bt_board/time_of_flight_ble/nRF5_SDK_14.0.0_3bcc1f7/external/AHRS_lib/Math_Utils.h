/*
 * ahrs_lib
 * math utilities
 *
 *  Created on: 14/jan2013
 *     Authors:  Bojan Milosevic, Filippo Casamassima
 */

#ifndef MATH_UTILS_H_
#define MATH_UTILS_H_

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
//#include "stm32f10x.h"

 #include "AHRS_conf.h"

// /*general constants and parameters definition*/
// #define		PI				3.14159265
// #define		r2d 			180/PI
// #define 	d2r				PI/180
// //#define  	dt 				0.05 //output data rate

// #define 	SGN(x)  		( ((x) < 0) ?  -1 : ( ((x) == 0 ) ? 0 : 1) )
// #define 	ABS(x)  		( ((x) < 0) ?  -x : ( ((x) == 0 ) ? 0 : x) )
// #define 	dimA			18
// #define		dim2A			18*18


// #define 	    ge				9.822
// #define		me				0.5
// #define		meo				0.9

// // const gn matrix for gnx init issue
// #define   gn_0      0
// #define   gn_1      0
// #define   gn_2     -ge
// // const mn matrix for mnx init issue
// #define   mn_0      me
// #define   mn_1      0
// #define   mn_2     -meo

float invSqrt(float x);
void R2quat(float *R, float *q);
void quat2R(float *b, float *Rn2b);
void Matrix_Add(float *A, float *B, float *C, int row, int col);
void Matrix_Multiply(float *A, float *B, float *C, int rowA, int colA, int rowB, int colB, char trasp);
void expm(float* A, float* E, float prec);
void swap_riga (float* A, int ordine, int riga1, int riga2);
void comb_lin (float* A, int ordine, int riga1, int riga2, float coeff);
void Matrix_Inversion (float* A, float* Y, int ordine);
void expm2 (float* A, float* E);

//void calcola_init (fx_int* Acc, fx_int* Mag, fx_int* bE0);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*fixed point*/
// #define FX_FRAC 22 //must be even
// #define ITERS	(15+ (FX_FRAC>>1))
// #define FX_ONE (1<<FX_FRAC)
// typedef int32_t fx_int;
// #define fx_float(a) ((float)(a) / (1<<FX_FRAC))
// #define fx_fxpnt(a) ((int32_t)((a) * (1<<FX_FRAC)+ ((a)>=0 ? 0.5 : -0.5)))
// #define fx_mul(a,b) ((int32_t)(((int64_t)(a) * (int64_t)(b)) >> FX_FRAC))
// #define fx_div(a,b) ((int32_t)(((int64_t)(a) << FX_FRAC) / (b)))
// #define fx_square(a) (fx_mul((a),(a)))
// #define fx_SGN(x)  		( ((x) < 0) ?  -FX_ONE : ( ((x) == 0 ) ? 0 : FX_ONE) )

// #define cmp(a,b)    ( (ABS(a)>ABS(b)) ? a : b )

// void fx_AHRS_Acc_Calibration(int16_t* raw_data, fx_int* data_out); /*Acc data calibration*/
// void fx_AHRS_Mag_Calibration(int16_t* raw_data, fx_int* data_out); /*Mag data calibration*/
// void fx_AHRS_Gyr_Calibration(int16_t* raw_data, fx_int* data_out); /*Gyro data calibration*/
// void fx_AHRS_LPF(fx_int* Acc_Data, fx_int* Mag_Data, fx_int* Gyr_Data); /*Low Pass Filter*/
// void fx_AHRS_Kalman(fx_int* Acc_Data, fx_int* Mag_Data, fx_int* Gyr_Data, float* Euler_Angles); //, fx_int* fx_bE);
// void fx_rot2quat(fx_int *R, fx_int *q);
// void fx_quat2R(fx_int *b, fx_int *Rn2b);
// void fx_Mpiu(fx_int *A, fx_int *B, fx_int *C, int row, int col);
// void fx_Mper(fx_int *A, fx_int *B, fx_int *C, int rowA, int colA, int rowB, int colB, char trasp);
// void fx_expm(fx_int* A, fx_int* E, fx_int prec);
// void fx_swap_riga (fx_int* A, int ordine, int riga1, int riga2);
// void fx_comb_lin (fx_int* A, int ordine, int riga1, int riga2, fx_int coeff);
// void fx_invGauss (fx_int* A, fx_int* Y, int ordine);
// fx_int fx_sqrt(fx_int x);


#endif
