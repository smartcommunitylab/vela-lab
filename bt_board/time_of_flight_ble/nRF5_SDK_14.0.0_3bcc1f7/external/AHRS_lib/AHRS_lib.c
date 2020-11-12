/*
 * ahrs_lib
 * main file with high level API
 *
 *  Created on: 14/jan2013
 *     Authors:  Bojan Milosevic, Filippo Casamassima
 *     Adapted by Davide Giovanelli
 */


#include <stdio.h>
#include "AHRS_lib.h"
#include <stdlib.h>

#define QV 						(float)0.00027777778//(float)0.00027777778 //(float)((0.05/3)*(0.05/3))
#define RTT_STD_0 				(float)130
#define RSSI_STD_0 				(float)4.6
#define LN_10					(float)2.302585092994046 //value of log(10)
#define C_SQUARE				(float)89875517873681764

void KF_set_RTT_offset(KF_t* mkf, float O_runtime_new){
	mkf->O_runtime=O_runtime_new;
}

void KF_increase_RTT_offset_by_m(KF_t* mkf, float meters_to_compensate){
	mkf->O_runtime=mkf->O_runtime + (meters_to_compensate)/(C/2*C_GAIN);
	//reset KF (kalman hack not checked)
}

void KF_hack_filter_state(KF_t* mkf, KF_status* newStatus){

	mkf->kf_status.x_hat[0]=newStatus->x_hat[0];
	mkf->kf_status.x_hat[1]=newStatus->x_hat[1];
	//TODO: USE memcpy
	return;
}

void KF_Raw_Data_Convert(KF_t* mkf, Sampled_Raw_Data *sData, float dT, Measurement_Data *mData){


	mData->r_m[0] = C/2*(sData->tof[0] - mkf->O_runtime)*C_GAIN; //assign distance

	//for computing the velocity we need last two samples, use arrays for this
	//float FREQ_mov[2];
	//float RSS_ref[2];
	//FREQ_mov[0]=sData->freq[0];
	//FREQ_mov[1]=sData->freq[1];

	//RSS_ref[0]=-54;//computed fixing d_ref_runtime=5 and FREQ_mov=2.442MHz in: 10*log10(mkf->PTX*C_SQUARE/(1e-3*pow(4*PI*mkf->d_ref_runtime*FREQ_mov[0],2)));
	//RSS_ref[1]=-54;//computed fixing d_ref_runtime=5 and FREQ_mov=2.442MHz in: 10*log10(mkf->PTX*C_SQUARE/(1e-3*pow(4*PI*mkf->d_ref_runtime*FREQ_mov[1],2)));

	float r_rssi[2];
	float RSSI_av[2];

	RSSI_av[0] = sData->rssi[0];
	RSSI_av[1] = sData->rssi[1];

	KF_Rssi_Data_Convert(mkf,RSSI_av[0],&r_rssi[0]);//mkf->d_ref_runtime*pow(10,((RSS_ref[0]-RSSI_av[0]+mkf->THETA)/(10*mkf->alpha)));
	KF_Rssi_Data_Convert(mkf,RSSI_av[1],&r_rssi[1]);//r_rssi[1] = mkf->d_ref_runtime*pow(10,((RSS_ref[1]-RSSI_av[1]+mkf->THETA)/(10*mkf->alpha)));

	mData->v_m[0] = (r_rssi[0] - r_rssi[1])/dT;

	mData->dT[0]=dT;
}

void KF_Rssi_Data_Convert(KF_t* mkf, int8_t rssi, float *distance){
	float RSS_ref[1];
	RSS_ref[0]=-54;//computed fixing d_ref_runtime=5 and FREQ_mov=2.442MHz in: 10*log10(mkf->PTX*C_SQUARE/(1e-3*pow(4*PI*mkf->d_ref_runtime*FREQ_mov[0],2)));
	*distance=mkf->d_ref_runtime*pow(10,((RSS_ref[0]-rssi+mkf->THETA)/(10*mkf->alpha)));
}


void KF_noise_update(Measurement_Data *mData, Sampled_Raw_Data* sData, KF_t* mkf){
	//fixed noise for now, to compute variance we need MQ2 samples
	// note that on matlab version QQ is fixed, while RR is updated
	mkf->kf_status.Q[0] = QV*pow(mData->dT[0],2);
	mkf->kf_status.Q[1] = QV*mData->dT[0];
	mkf->kf_status.Q[2] = QV*mData->dT[0];
	mkf->kf_status.Q[3] = QV;

	mkf->kf_status.R[0] = 379.7240630163055;////pow(RTT_STD_0*C*0.000000001,2);
	mkf->kf_status.R[1] = 0;
	mkf->kf_status.R[2] = 0;
//	mkf->kf_status.R[3] = 90000;//119563; // 2*(((mkf->d_ref_runtime{nodeIdx}(1)*(10.^((RSS_ref(1)-RSSI_mov{nodeIdx}(1)+mkf->THETA+RSSI_STD_0)./(10*mkf->alpha)))-mkf->d_ref_runtime{nodeIdx}(1)*(10.^((RSS_ref(1)-RSSI_mov{nodeIdx}(1)+mkf->THETA-RSSI_STD_0)./(10*mkf->alpha))))/2/DEFAULT_SAMPLING_RATE_S)^2)/M2

	//float FREQ_mov[1];
	//FREQ_mov[0]=sData->freq[0];

	float RSS_ref[1];
	RSS_ref[0]=-54;//computed fixing d_ref_runtime=5 and FREQ_mov=2.442MHz in: 10*log10(mkf->PTX*pow(C,2)/(1e-3*pow(4*PI*mkf->d_ref_runtime*FREQ_mov[0],2))); //TODO: this is computed also into KF_Raw_Data_Convert

	float RSSI_av[1];
	RSSI_av[0]=sData->rssi[0];

	//TODO: compute the closed form of the derivative of the model and use that one instead of this long expression
//	mkf->kf_status.R[3] =  2*pow(((mkf->d_ref_runtime*pow(10,((RSS_ref[0]-RSSI_av[0]+mkf->THETA+RSSI_STD_0)/(10*mkf->alpha)))-mkf->d_ref_runtime*pow(10,((RSS_ref[0]-RSSI_av[0]+mkf->THETA-RSSI_STD_0)/(10*mkf->alpha))))/2/mData->dT[0]),2);
//	mkf->kf_status.R[3] =  (2/pow(DEFAULT_SAMPLING_RATE_S,2))*pow(RSSI_STD_0*mkf->d_ref_runtime*LN_10*pow(10, (RSS_ref[0]-RSSI_av[0]+mkf->THETA)/(10*mkf->alpha)),2);
	float FADE_K=10*mkf->alpha;
	mkf->kf_status.R[3] = 2*pow(mkf->d_ref_runtime*LN_10*RSSI_STD_0/(FADE_K*mData->dT[0])*pow(10, (RSS_ref[0]-RSSI_av[0]+mkf->THETA)/(FADE_K)),2);
}

void KF_Update(KF_t* mkf, Sampled_Raw_Data* sData, float dT)
{
	Measurement_Data mData;

	//system matrices
	float A[4] = {1, dT,
				  0, 1};

	float H[4] = {1, 0,
				  0, 1};

	KF_Raw_Data_Convert(mkf, sData, dT, &mData);

	KF_noise_update(&mData, sData, mkf);

	float *QQ=mkf->kf_status.Q;
	float *RR=mkf->kf_status.R;

	//PREDICTION STEP
	//x_hat_prior prediction
	float x_hat_prior[2];	//x_hat predicted before update (x prior)
	float *x_hat_prev=mkf->kf_status.x_hat;	//x_hat at previous k
	x_hat_prior[0] = A[0]*x_hat_prev[0]+ A[1]*x_hat_prev[1];
	x_hat_prior[1] = A[2]*x_hat_prev[0]+ A[3]*x_hat_prev[1];

	//P_prior prediction
	float A_t[4];
	A_t[0] = A[0];
	A_t[1] = A[2];
	A_t[2] = A[1];
	A_t[3] = A[3];

	float P_prior[4];
	float *P_prev=mkf->kf_status.P;
	P_prior[0] = A_t[0]*(A[0]*P_prev[0]+A[1]*P_prev[2]) + A_t[2]*(A[0]*P_prev[1]+A[1]*P_prev[3]) + QQ[0];
	P_prior[1] = A_t[1]*(A[0]*P_prev[0]+A[1]*P_prev[2]) + A_t[3]*(A[0]*P_prev[1]+A[1]*P_prev[3]) + QQ[1];
	P_prior[2] = A_t[0]*(A[2]*P_prev[0]+A[3]*P_prev[2]) + A_t[2]*(A[2]*P_prev[1]+A[3]*P_prev[3]) + QQ[2];
	P_prior[3] = A_t[1]*(A[2]*P_prev[0]+A[3]*P_prev[2]) + A_t[3]*(A[2]*P_prev[1]+A[3]*P_prev[3]) + QQ[3];


	//UPDATE STEP
    // Kalman gain
	float H_t[4];
	H_t[0]=H[0];
	H_t[1]=H[2];
	H_t[2]=H[1];
	H_t[3]=H[3];

    float num[4];
    num[0] = P_prior[0]*H_t[0]+P_prior[1]*H_t[2];
    num[1] = P_prior[0]*H_t[1]+P_prior[1]*H_t[3];
    num[2] = P_prior[2]*H_t[0]+P_prior[3]*H_t[2];
    num[3] = P_prior[2]*H_t[1]+P_prior[3]*H_t[3];

    float den[4];
    den[0] = H_t[0]*(H[0]*P_prior[0]+H[1]*P_prior[2]) + H_t[2]*(H[0]*P_prior[1]+H[1]*P_prior[3]) + RR[0];
    den[1] = H_t[1]*(H[0]*P_prior[0]+H[1]*P_prior[2]) + H_t[3]*(H[0]*P_prior[1]+H[1]*P_prior[3]) + RR[1];
    den[2] = H_t[0]*(H[2]*P_prior[0]+H[3]*P_prior[2]) + H_t[2]*(H[2]*P_prior[1]+H[3]*P_prior[3]) + RR[2];
    den[3] = H_t[1]*(H[2]*P_prior[0]+H[3]*P_prior[2]) + H_t[3]*(H[2]*P_prior[1]+H[3]*P_prior[3]) + RR[3];

    float den_inv[4], k;
    k= den[0]*den[3]-den[1]*den[2];
    den_inv[0] = den[3]/k;
    den_inv[1] = -den[1]/k;
    den_inv[2] = -den[2]/k;
    den_inv[3] = den[0]/k;

    float Gk[4];
    Gk[0] = num[0]*den_inv[0]+num[1]*den_inv[2];
    Gk[1] = num[0]*den_inv[1]+num[1]*den_inv[3];
    Gk[2] = num[2]*den_inv[0]+num[3]*den_inv[2];
    Gk[3] = num[2]*den_inv[1]+num[3]*den_inv[3];

    //updates
    float Z[2];
    Z[0]=mData.r_m[0];
    Z[1]=mData.v_m[0];
    //compute D=(Z-H*x_hat_prior)
    float D[2];
    D[0]=Z[0]-(H[0]*x_hat_prior[0]+H[1]*x_hat_prior[1]);
    D[1]=Z[1]-(H[2]*x_hat_prior[0]+H[3]*x_hat_prior[1]);

    //state update
    float x_hat[2];
    x_hat[0]=x_hat_prior[0]+(Gk[0]*D[0]+Gk[1]*D[1]);
    x_hat[1]=x_hat_prior[1]+(Gk[2]*D[0]+Gk[3]*D[1]);

    //P update
    float I[4];
    I[0] = 1;
    I[1] = 0;
    I[2] = 0;
    I[3] = 1;
    //compute W=(I-Gk*H)
    float W[4];
    W[0] = I[0] - Gk[0]*H[0]+Gk[1]*H[2];
    W[1] = I[1] - Gk[0]*H[1]+Gk[1]*H[3];
    W[2] = I[2] - Gk[2]*H[0]+Gk[3]*H[2];
    W[3] = I[3] - Gk[2]*H[1]+Gk[3]*H[3];

    float P[4];
    P[0] = W[0]*P_prior[0]+W[1]*P_prior[2];
    P[1] = W[0]*P_prior[1]+W[1]*P_prior[3];
    P[2] = W[2]*P_prior[0]+W[3]*P_prior[2];
    P[3] = W[2]*P_prior[1]+W[3]*P_prior[3];


//	float d_min=0;
//    if(x_hat[0] < d_min){
//    	x_hat[0]=x_hat[0]+1*(d_min - x_hat[0]);
//    	x_hat[1]=x_hat[1]+P[1]/P[0]*(d_min - x_hat[0]);
//    }

    //STORE NEW KALMAN STATUS
    mkf->kf_status.P[0] = P[0];
    mkf->kf_status.P[1] = P[1];
    mkf->kf_status.P[2] = P[2];
    mkf->kf_status.P[3] = P[3];

    mkf->kf_status.x_hat[0] = x_hat[0];
    mkf->kf_status.x_hat[1] = x_hat[1];

}

void KF_Init(KF_t* mkf, Sampled_Raw_Data *sData)
{
	//set RSSI and ToF models' parameters
	mkf->d_ref_runtime = DEFAULT_D_REF;
	mkf->PTX = 0.001;
	mkf->O_runtime = DEFAULT_O + (FURTHER_RTT_OFFSET_M)/(C/2*C_GAIN);
	mkf->THETA = 0.029207499216403;
	mkf->alpha = 2;

	Sampled_Raw_Data initData; //NB: if sData is set to non-NULL the space allocated for initData isn't used

	if(sData == NULL){
		// reset kalman status
		initData.rssi[0]=-50;
		initData.rssi[1]=-50;
		initData.tof[0]=mkf->O_runtime;
		initData.freq[0]=2400000000;
		initData.freq[1]=2400000000;

		sData=&initData;
	}

	Measurement_Data mData;

	KF_Raw_Data_Convert(mkf, sData, DEFAULT_SAMPLING_RATE_S, &mData);

	KF_noise_update(&mData, sData, mkf);

	mkf->kf_status.x_hat[0] = mData.r_m[0];
	mkf->kf_status.x_hat[1] = mData.v_m[0];

	mkf->kf_status.P[0] = 132.25;//pow(11.5,2);
	mkf->kf_status.P[1] = 0;
	mkf->kf_status.P[2] = 0;
	mkf->kf_status.P[3] = 0.00001;
}
