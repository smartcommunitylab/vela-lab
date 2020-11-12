/*
 * ahrs_lib
 * math utilities
 *
 *  Created on: 14/jan2013
 *     Authors:  Bojan Milosevic, Filippo Casamassima
 */

#include "Math_Utils.h"

extern float dT;

/**
* @brief  Conversion of orientation representation fron rotation matrix (R) to quaternion q
* @param  R : rotation matix 
* @param  q : quaternion
* @retval None
*/
void R2quat(float* R, float* q)
{
	int i;
	float S, T = 1 + R[0] + R[4] + R[8];

  if (T < 0.0001) {
    if (R[0] > R[4] && R[0] > R[8]) {
        T = 1 + R[0] - R[4] - R[8];
        S = 2*sqrt(T);

        q[0]=(R[5]-R[7])/S;
        q[1]=0.25*S;
        q[2]=(R[1]+R[3])/S;
        q[3]=(R[6]+R[2])/S;
    } else if (R[4] > R[8]) {
        T = 1 + R[4] - R[0] - R[8];
        S = 2*sqrt(T);

        q[3]=(R[5]+R[7])/S;
        q[2]=0.25*S;
        q[1]=(R[1]+R[3])/S;
        q[0]=(R[6]-R[2])/S;
    } else {
        T = 1 + R[8] - R[0] - R[4];
        S = 2*sqrt(T);

        q[2]=(R[5]+R[7])/S;
        q[3]=0.25*S;
        q[0]=(R[1]-R[3])/S;
        q[1]=(R[6]+R[2])/S;
    }
  } else {
    T = 1 + R[0] + R[4] + R[8];
	  S = (T >= 0) ? 2*sqrt(T) : 0;

    q[0] = 0.25 * S;
    q[1] = (R[5]-R[7])/S;
    q[2] = (R[6]-R[2])/S;
    q[3] = (R[1]-R[3])/S;
  }

  for (i=1;i<4;i++)
    q[i] = -q[i];

  if (q[0] < 0)
    for (i=0;i<4;i++)
      q[i] = -q[i];

  /*
	float tempq[4];
	float normq;

	float sqrt1 = 1 + R[0] + R[4] + R[8];
	float sqrt2 = 1 + R[0] - R[4] - R[8];
	float sqrt3 = 1 + R[4] - R[8] - R[0];
	float sqrt4 = 1 + R[8] - R[4] - R[0];

	//Azzeramento delle variabili negative
	if (sqrt1 < 0)
		sqrt1 = 0;
	if (sqrt2 < 0)
		sqrt2 = 0;
	if (sqrt3 < 0)
		sqrt3 = 0;
	if (sqrt4 < 0)
		sqrt4 = 0;


	tempq[0] = 0.5 * sqrt(sqrt1);
	tempq[1] = 0.5 * SGN((R[5] - R[7])) * sqrt(sqrt2);
	tempq[2] = 0.5 * SGN((R[6] - R[2])) * sqrt(sqrt3);
	tempq[3] = 0.5 * SGN((R[1] - R[3])) * sqrt(sqrt4);

	for (i=0;i<=3;i++)
		q[i] = tempq[i];

	normq = sqrt((q[0]*q[0]) + (q[1]*q[1]) + (q[2]*q[2]) + (q[3]*q[3]));	//Norma-2 di q

	for (i=0;i<=3;i++)
		q[i] = q[i] / normq;
	*/
}


/**
* @brief  Conversion of orientation representation from quaternion q to rotation matrix (R)
* @param  R : rotation matix 
* @param  q : quaternion
* @retval None
*/
void quat2R(float* q, float* R)
{
//	int i;
	float normb = sqrt(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
	if (normb != 0)
	{
//		for (i=0;i<=3;i++)
//			b[i] = b[i]/normb;

		R[0] = q[0]*q[0]+q[1]*q[1]-q[2]*q[2]-q[3]*q[3];
		R[1] = 2*(q[1]*q[2]+q[0]*q[3]);
		R[2] = 2*(q[1]*q[3]-q[0]*q[2]);
		R[3] = 2*(q[1]*q[2]-q[0]*q[3]);
		R[4] = q[0]*q[0]-q[1]*q[1]+q[2]*q[2]-q[3]*q[3];
		R[5] = 2*(q[0]*q[1]+q[2]*q[3]);
		R[6] = 2*(q[0]*q[2]+q[1]*q[3]);
		R[7] = 2*(q[2]*q[3]-q[0]*q[1]);
		R[8] = q[0]*q[0]-q[1]*q[1]-q[2]*q[2]+q[3]*q[3];
	}
	else
	{
		R[0] = 1;   // fault condition
		R[1] = 0;
		R[2] = 0;
		R[3] = 0;
		R[4] = 1;
		R[5] = 0;
		R[6] = 0;
		R[7] = 0;
		R[8] = 1;
	}
}


//---------------------------------------------------------------------------------------------------
// Fast inverse square-root
// See: http://en.wikipedia.org/wiki/Fast_inverse_square_root
float invSqrt(float x) {
	float halfx = 0.5f * x;
	float y = x;
	long i = *(long*)&y;
	i = 0x5f3759df - (i>>1);
	y = *(float*)&i;
	y = y * (1.5f - (halfx * y * y));
	return y;
}


/**
* @brief  Addition of two matrices: C = A+B
* @param  A,B matrices to add
* @param  C : matrix where the result is stored
* @param  row : number of rows
* @param  col : number of columns
* @retval None
*/
void Matrix_Add(float *A, float *B, float *C, int row, int col)
{
	int i;
	for (i=0; i<row*col; i++)
	{
		C[i]=A[i]+B[i];
	}
}


/**
* @brief  Multiplication of two matrices: C = A*B (with possible transposition of one of the two)
* @param  A,B matrices to multiply
* @param  C : matrix where the result is stored
* @param  rowA, rowB : number of rows of matrix A and B
* @param  colA, colB : number of columns of matrix A and B
* @param  trasp : transposition flag: 0 -> C = A*B; 1 -> C = A'*B; 2 -> C = A*B' (where A' is the transposition of A)
* @retval None
*/
void Matrix_Multiply(float *A, float *B, float *C, int rowA, int colA, int rowB, int colB, char trasp)
{
    int rowC,colC;
    int i,j,t,temp1,temp2;
    float acc;

    switch (trasp)
    {

    case 1:
        rowC=colA;
        colC=colB;
        for (i=0; i<rowC; i++)
            {
                for (j=0; j<colC; j++)
                {
                    acc = 0;
                    for (t=0; t<rowA; t++)
                        if (A[i+(t*rowA)]!=0&&B[j+(t*colB)]!=0)
                            acc += A[i+(t*rowA)]*B[j+(t*colB)];
                    C[j+(i*colC)]=acc;
                }
            }
        break;

    case 2:
        rowC=rowA;
        colC=rowB;
        for (i=0; i<rowC; i++)
            {
                temp1=i*colA;
                temp2=i*colC;
                for (j=0; j<colC; j++)
                {
                    acc = 0;
                    for (t=0; t<colA; t++)
                        if (A[t+temp1]!=0&&B[t+(j*colB)]!=0)
                            acc += A[t+temp1]*B[t+(j*colB)];
                    C[j+temp2]=acc;
                }
            }
        break;

    default:
        rowC=rowA;
        colC=colB;
        for (i=0; i<rowC; i++)
            {
                temp1=i*colA;
                temp2=i*colC;
                for (j=0; j<colC; j++)
                {
                    acc = 0;
                    for (t=0; t<colA; t++)
                        if (A[t+temp1]!=0&&B[j+(t*colB)]!=0)
                            acc += A[t+temp1]*B[j+(t*colB)];
                    C[j+temp2]=acc;
                }
            }
        break;
    }
}

/**
* @brief  Swaps two lines in a matrix 
* @param  A : matrix
* @param  ordine : number of columns
* @param  riga1, riga2 : lines to be swapped
* @retval None
*/

void swap_riga (float* A, int ordine, int riga1, int riga2)
{
    float temp;
    int i;
    for (i=0;i<ordine;i++)
        {
            temp=A[(riga1*ordine)+i];
            A[(riga1*ordine)+i]=A[(riga2*ordine)+i];
            A[(riga2*ordine)+i]=temp;
        }
}

/**
* @brief  linear combination of line in riga1 multiplied by the coefficents in coeff and put in line riga2
* @param  
* @param  
* @retval None
*/
void comb_lin (float* A, int ordine, int riga1, int riga2, float coeff)
{
    int i;
    for (i=0;i<ordine;i++)
        A[(riga2*ordine)+i]+=(A[(riga1*ordine)+i]*coeff);
}


/**
* @brief  Inversion of a matrix
* @param  
* @param  
* @retval None
*/
void Matrix_Inversion (float* A, float* Y, int ordine)
{
    int c=0,i,j,flag=0;
    float coeff,temp;
    for (i=0;i<ordine;i++)
        for (j=0;j<ordine;j++)
            {
                if (i==j)
                    Y[c]=1;
                else
                    Y[c]=0;
                c++;
            }

    //Azzera gli elementi sotto la diagonale
    for (i=0;i<ordine-1;i++)
        {
            //Cerca la riga con l'elemento maggiore sulla colonna i-esima e la scambia con la riga corrente
            coeff=0;
            flag=i;
            for (j=i;j<ordine;j++)
            {
                temp=ABS(A[j*ordine+i]);
                if (temp>coeff)
                    {
                        coeff=temp;
                        flag=j;
                    }
            }
            swap_riga(A,ordine,i,flag);
            swap_riga(Y,ordine,i,flag);

            //Azzera l'elemento sotto il pivot per ogni riga mediante combinazione lineare
            for (j=i+1;j<ordine;j++)
                {
                    coeff=-A[j*ordine+i]/A[i*ordine+i];
                    if (coeff!=0)
                    {
                        comb_lin(A,ordine,i,j,coeff);
                        comb_lin(Y,ordine,i,j,coeff);
                    }


                }
        }

    //Azzera gli elementi sopra la diagonale
    for (j=ordine-1;j>=0;j--)
        {
            for (i=j-1;i>=0;i--)
                {
                    temp=A[i*ordine+j];
                    if (temp!=0)
                        {
                            coeff=-temp/A[j*ordine+j];
                            comb_lin(A,ordine,j,i,coeff);
                            comb_lin(Y,ordine,j,i,coeff);
                        }
                }

            //Normalizza dividendo la riga per il pivot
            coeff=A[j*ordine+j];
            A[j*ordine+j]=1;
            for (i=0;i<ordine;i++)
                {
//                    A[i*ordine+j]=A[i*ordine+j]/coeff;
                    Y[j*ordine+i]=Y[j*ordine+i]/coeff;
                }
        }


}


void expm (float* A, float* E, float prec)
//A deve essere una matrice quadrata
//Porting di expmdemo2 di matlab
{
   float F[dim2A],temp[dim2A];
   int i,j,c;
   float normFtemp[dimA],k=1,kinv, normF;

    //Inizializzazione di E ed F
    c=0;
    for (i=0; i<dimA; i++)
        for (j=0; j<dimA; j++)
           {
                E[c]=0;
                if (i==j)
                   F[c]=1;
                else
                    F[c]=0;
                c++;
           }
//    while (normF>0.1);
do
        {
            Matrix_Add (E,F,E,dimA,dimA);   //E=E+F
            kinv=1/k;

            for (i=0;i<dim2A;i++)
                temp[i]=F[i]*kinv;

            Matrix_Multiply(A,temp,F,dimA,dimA,dimA,dimA,0);   //F=A*F/k;

            //Calcolo della norma di F
            normF=0;
            for (i=0;i<dimA;i++)
                normFtemp[i]=0;
            for (i=0;i<dim2A;i++)
                {
                    if  (F[i]<0)
                        normFtemp[i % dimA] -= F[i];
                    else
                        normFtemp[i % dimA] += F[i];
                }
            for (i=0;i<dimA;i++)
                if (normFtemp[i]>normF)
                    normF=normFtemp[i];

            k++;
        }   while(normF>prec);
}




/*************************************************************************
        FIXED POINT
**************************************************************************/

// fx_int fx_ACC_Gain[9]= {
// 		fx_fxpnt(0.999339361087703),fx_fxpnt(0.00111326712659132),fx_fxpnt(-0.00119521982609043),
// 		fx_fxpnt(-0.0270478775598373),fx_fxpnt(0.993274888205323),fx_fxpnt(0.0327490088849128),
// 		fx_fxpnt(0.00184103891512333),fx_fxpnt(-0.0183794751437976),fx_fxpnt(0.997883156324033)};


// fx_int fx_sqrt(fx_int x)
// {
// 	register uint32_t root, remHi, remLo, testDiv, count;
// 	root=0;
// 	remHi=0;
// 	remLo=x;
// 	count=ITERS;
// //	if (x<0)
// //	{
// //	    printf("Radice negativa!!!\n%f\t%f\n",fx_float(x),fx_float(remLo));
// //	    return(0);
// //	}
// 	do {
// 		remHi = (remHi << 2) | (remLo >> 30); remLo <<= 2; /* get 2 bits of arg */
// 		root <<= 1; /* Get ready for the next bit in the root */
// 		testDiv = (root << 1) + 1; /* Test radical */
// 		if (remHi >= testDiv) {
// 			remHi -= testDiv;
// 			root += 1;
// 		}
// 	} while (count-- != 0);
// 	return((fx_int)root);
// }

// void fx_expm (fx_int* A, fx_int* E, fx_int prec)
// //A deve essere una matrice quadrata
// //Porting di expmdemo2 di matlab
// {
//    fx_int F[dim2A],temp[dim2A];
//    int i,j,c;
//    fx_int normFtemp[dimA],k=1,kinv, normF;

//     //Inizializzazione di E ed F
//     c=0;
//     for (i=0; i<dimA; i++)
//         for (j=0; j<dimA; j++)
//             {
//                 E[c]=0;
//                 if (i==j)
//                    F[c]=FX_ONE;
//                 else
//                     F[c]=0;
//                 c++;
//            }
// //    while (normF>0.1);
// do
//         {
//             fx_Mpiu (E,F,E,dimA,dimA);   //E=E+F
//             kinv=fx_div(1,k);

//             for (i=0;i<dim2A;i++)
//                 temp[i]=fx_mul(F[i],kinv);

//             fx_Mper(A,temp,F,dimA,dimA,dimA,dimA,0);   //F=A*F/k;

//             //Calcolo della norma di F
//             normF=0;
//             for (i=0;i<dimA;i++)
//                 normFtemp[i]=0;
//             for (i=0;i<dim2A;i++)
//                 {
//                     if  (F[i]<0)
//                         normFtemp[i % dimA] -= F[i];
//                     else
//                         normFtemp[i % dimA] += F[i];
//                 }
//             for (i=0;i<dimA;i++)
//                 if (normFtemp[i]>normF)
//                     normF=normFtemp[i];

//             k++;
//         }   while(normF>prec); //2 rappresenta il 2° bit meno significativo del fixed point
// }


// //Funzione di conversione da matrici di rotazione a quaternioni
// void fx_rot2quat(fx_int* R, fx_int* q)
// {
// //    int i;
// //    float float_R[9], float_q[4];
// //    for (i=0;i<9;i++)
// //        float_R[i]=fx_float(R[i]);
// //    rot2quat(float_R, float_q);
// //    for (i=0;i<4;i++)
// //        q[i]=fx_fxpnt(float_q[i]);
// 	int i;

// 	fx_int tempq[4];
// 	fx_int normq;

// 	fx_int sqrt1 = FX_ONE + R[0] + R[4] + R[8];
// 	fx_int sqrt2 = FX_ONE + R[0] - R[4] - R[8];
// 	fx_int sqrt3 = FX_ONE + R[4] - R[8] - R[0];
// 	fx_int sqrt4 = FX_ONE + R[8] - R[4] - R[0];

// 	//Azzeramento delle variabili negative
// 	if (sqrt1 < 0)
// 		sqrt1 = 0;
// 	if (sqrt2 < 0)
// 		sqrt2 = 0;
// 	if (sqrt3 < 0)
// 		sqrt3 = 0;
// 	if (sqrt4 < 0)
// 		sqrt4 = 0;

// 	tempq[0] = fx_sqrt(sqrt1)/2;
// 	tempq[1] = fx_mul(fx_SGN((R[5] - R[7])),fx_sqrt(sqrt2)>>1);
// 	tempq[2] = fx_mul(fx_SGN((R[6] - R[2])),fx_sqrt(sqrt3)>>1);
// 	tempq[3] = fx_mul(fx_SGN((R[1] - R[3])),fx_sqrt(sqrt4)>>1);

// 	for (i=0;i<=3;i++)
// 		q[i] = tempq[i];

//     normq = fx_sqrt(fx_square(q[0]) + fx_square(q[1]) + fx_square(q[2]) + fx_square(q[3]));
// //    normq = fx_sqrt(fx_square(q[0]>>(FX_FRAC-16)) + fx_square(q[1]>>(FX_FRAC-16)) + fx_square(q[2]>>(FX_FRAC-16)) + fx_square(q[3]>>(FX_FRAC-16)))<<(FX_FRAC-16);
// 	for (i=0;i<=3;i++)
// 		q[i] = fx_div(q[i],normq);
// }


// //Funzione di conversione da quaternioni a matrici di rotazione
// void fx_quat2R(fx_int* b, fx_int* Rb2n)
// {
// 	int i;
//     fx_int normb = fx_sqrt(fx_square(b[0]) + fx_square(b[1]) + fx_square(b[2]) + fx_square(b[3]));
// 	if (normb != 0)
// 	{
// 		for (i=0;i<=3;i++)
// 			b[i] = fx_div(b[i],normb);

// 		Rb2n[0] = fx_square(b[0])+fx_square(b[1])-fx_square(b[2])-fx_square(b[3]);
// 		Rb2n[1] = 2*(fx_mul(b[1],b[2])+fx_mul(b[0],b[3]));
// 		Rb2n[2] = 2*(fx_mul(b[1],b[3])-fx_mul(b[0],b[2]));
// 		Rb2n[3] = 2*(fx_mul(b[1],b[2])-fx_mul(b[0],b[3]));
// 		Rb2n[4] = fx_square(b[0])-fx_square(b[1])+fx_square(b[2])-fx_square(b[3]);
// 		Rb2n[5] = 2*(fx_mul(b[0],b[1])+fx_mul(b[2],b[3]));
// 		Rb2n[6] = 2*(fx_mul(b[0],b[2])+fx_mul(b[1],b[3]));
// 		Rb2n[7] = 2*(fx_mul(b[2],b[3])-fx_mul(b[0],b[1]));
// 		Rb2n[8] = fx_square(b[0])-fx_square(b[1])-fx_square(b[2])+fx_square(b[3]);
// 	}
// 	else
// 	{
// 		Rb2n[0] = FX_ONE;   // fault condition
// 		Rb2n[1] = 0;
// 		Rb2n[2] = 0;
// 		Rb2n[3] = 0;
// 		Rb2n[4] = FX_ONE;
// 		Rb2n[5] = 0;
// 		Rb2n[6] = 0;
// 		Rb2n[7] = 0;
// 		Rb2n[8] = FX_ONE;
// 	}
// }





// //Calcolo della somma di matrici vettorizzate
// void fx_Mpiu(fx_int *A, fx_int *B, fx_int *C, int row, int col)
// {
// 	int i;
// 	for (i=0; i<row*col; i++)
// 	{
// 		C[i]=A[i]+B[i];
// 	}
// }

// //Calcolo del prodotto di matrici vettorizzate
// void fx_Mper(fx_int *A, fx_int *B, fx_int *C, int rowA, int colA, int rowB, int colB, char trasp)
// //C=A*B
// //trasp è una flag: se =0 fa il prodotto normale, se =1 traspone A, se =2 traspone B
// {
//     int rowC,colC;
//     int i,j,t,temp1,temp2;
//     int acc;

//     switch (trasp)
//     {

//     case 1:
//         rowC=colA;
//         colC=colB;
//         for (i=0; i<rowC; i++)
//             {
//                 for (j=0; j<colC; j++)
//                 {
//                     acc = 0;
//                     for (t=0; t<rowA; t++)
//                         if (A[i+(t*rowA)]!=0&&B[j+(t*colB)]!=0)
//                             acc += fx_mul(A[i+(t*rowA)],B[j+(t*colB)]);
//                     C[j+(i*colC)]=acc;
//                 }
//             }
//         break;

//     case 2:
//         rowC=rowA;
//         colC=rowB;
//         for (i=0; i<rowC; i++)
//             {
//                 temp1=i*colA;
//                 temp2=i*colC;
//                 for (j=0; j<colC; j++)
//                 {
//                     acc = 0;
//                     for (t=0; t<colA; t++)
//                         if (A[t+temp1]!=0&&B[t+(j*colB)]!=0)
//                             acc += fx_mul(A[t+temp1],B[t+(j*colB)]);
//                     C[j+temp2]=acc;
//                 }
//             }
//         break;

//     default:
//         rowC=rowA;
//         colC=colB;
//         for (i=0; i<rowC; i++)
//             {
//                 temp1=i*colA;
//                 temp2=i*colC;
//                 for (j=0; j<colC; j++)
//                 {
//                     acc = 0;
//                     for (t=0; t<colA; t++)
//                         if (A[t+temp1]!=0&&B[j+(t*colB)]!=0)
//                             acc += fx_mul(A[t+temp1],B[j+(t*colB)]);
//                     C[j+temp2]=acc;
//                 }
//             }
//         break;
//     }
// }

// /**
// *@}
// */ /* end of group MATRIX FUNCTIONS */


// /**
// * @brief  Low-Pass Filter.
// */
// // void fx_AHRS_LPF(fx_int* Acc_Data, fx_int* Mag_Data, fx_int* Gyr_Data)
// // {
// 	// static fx_int Acc_old_x=0;
// 	// static fx_int Acc_old_y=0;
// 	// static fx_int Acc_old_z=0;
// 	// static fx_int Mag_old_x=0;
// 	// static fx_int Mag_old_y=0;
// 	// static fx_int Mag_old_z=0;
// 	// static fx_int Gyr_old_x=0;
// 	// static fx_int Gyr_old_y=0;
// 	// static fx_int Gyr_old_z=0;

// 	// Acc_old_x= Acc_Data[0]/3+Acc_old_x*2/3;
// 	// Acc_old_y= Acc_Data[1]/3+Acc_old_y*2/3;
// 	// Acc_old_z= Acc_Data[2]/3+Acc_old_z*2/3;
// 	// Mag_old_x= Mag_Data[0]/3+Mag_old_x*2/3;
// 	// Mag_old_y= Mag_Data[1]/3+Mag_old_y*2/3;
// 	// Mag_old_z= Mag_Data[2]/3+Mag_old_z*2/3;
// 	// Gyr_old_x= Gyr_Data[0]/3+Gyr_old_x*2/3;
// 	// Gyr_old_y= Gyr_Data[1]/3+Gyr_old_y*2/3;
// 	// Gyr_old_z= Gyr_Data[2]/3+Gyr_old_z*2/3;

// 	// Acc_Data[0]=Acc_old_x;
// 	// Acc_Data[1]=Acc_old_y;
// 	// Acc_Data[2]=Acc_old_z;
// 	// Mag_Data[0]=Mag_old_x;
// 	// Mag_Data[1]=Mag_old_y;
// 	// Mag_Data[2]=Mag_old_z;
// 	// Gyr_Data[0]=Gyr_old_x;
// 	// Gyr_Data[1]=Gyr_old_y;
// 	// Gyr_Data[2]=Gyr_old_z;
// // }

// /**
// * @brief  Implementa il filtro di kalman.
// * Restituisce gli angoli di eulero
// * sul puntatore passato in ingresso.
// */
// void fx_AHRS_Kalman(fx_int* Acc_Data, fx_int* Mag_Data, fx_int* Gyr_Data, float* Euler_Angles)
// {
// 	//Dichiarazione variabili
// 	int i,j;
// 	static fx_int fx_x_gE[3]={0,0,0}, fx_x_aE[3]={0,0,0}, fx_x_temp[3], fx_hatg_n[3], fx_hatm_n[3];
// 	static fx_int fx_btemp[4];
// 	extern fx_int fx_bE[4];

// 	static fx_int fx_Pp[81]={FX_ONE,0,0,0,0,0,0,0,0,
//                             0,FX_ONE,0,0,0,0,0,0,0,
//                             0,0,FX_ONE,0,0,0,0,0,0,
//                             0,0,0,0,0,0,0,0,0,
//                             0,0,0,0,0,0,0,0,0,
//                             0,0,0,0,0,0,0,0,0,
//                             0,0,0,0,0,0,0,0,0,
//                             0,0,0,0,0,0,0,0,0,
//                             0,0,0,0,0,0,0,0,0};

// 	static fx_int fx_z[6]; //6x1
// 	fx_int mnx[9]={0,fx_fxpnt(-meo),0,fx_fxpnt(meo),0,-fx_fxpnt(me),0,fx_fxpnt(me),0};
// 	fx_int gnx[9]={0,-fx_fxpnt(ge),0,fx_fxpnt(ge),0,0,0,0,0};
// 	fx_int H[54], K[54], m_temp3[54]; //H(6x9);K(9x6);m_temp3(9x6)
// 	fx_int Rb2n[9], Rb2n_temp[9], del_x[9], rho_cross[9];//Rb2n(3x3);Rb2n_temp(3x3);del_x(9x1);rho_cross(3x3)
// 	fx_int norm_fx_btemp;
// 	fx_int wbn_b[4],dot_b[4];
// 	fx_int F[81], G[81], E[81], m_temp[81], Q[81], phi[81], Qd[81], Pm[81]; //9x9
// 	fx_int gamma[324], chi[324]; //18x18
// 	fx_int R[36],Ra[36],m_temp2[36],Rb[36]; //6x6
// 	float  float_bE[4];



// //Inizializzazione
// 	for (i=0; i<81; i++){
// 	  Q[i]=0;
// 	}
// 	Q[0] =fx_fxpnt(omega_g_x);
// 	Q[10]=fx_fxpnt(omega_g_y);
// 	Q[20]=fx_fxpnt(omega_g_z);
// 	Q[30]=fx_fxpnt(nu_g_x);
// 	Q[40]=fx_fxpnt(nu_g_y);
// 	Q[50]=fx_fxpnt(nu_g_z);
// 	Q[60]=fx_fxpnt(omega_a_x);
// 	Q[70]=fx_fxpnt(omega_a_y);
// 	Q[80]=fx_fxpnt(omega_a_z);

// 	for (i=0; i<36; i++){
// 	  R[i]=0;
// 	}
// 	R[0] =fx_fxpnt(nu_a_x);
// 	R[7] =fx_fxpnt(nu_a_y);
// 	R[14]=fx_fxpnt(nu_a_z);
// 	R[21]=fx_fxpnt(nu_m_x);
// 	R[28]=fx_fxpnt(nu_m_y);
// 	R[35]=fx_fxpnt(nu_m_z);

// 	//Calcolo
// 	wbn_b[0] =  -(Gyr_Data[0] - fx_x_gE[0]);   // velocità angolare (giro corrretto con bias),  eqn. 10.24
// 	wbn_b[1] =  -(Gyr_Data[1] - fx_x_gE[1]);
// 	wbn_b[2] =  -(Gyr_Data[2] - fx_x_gE[2]);

// 	dot_b[0] = fx_mul(fx_fxpnt(dT),( (fx_mul(-fx_bE[1],wbn_b[0]) - fx_mul(fx_bE[2],wbn_b[1]) - fx_mul(fx_bE[3],wbn_b[2]))>>1 ));
// 	dot_b[1] = fx_mul(fx_fxpnt(dT),( (fx_mul( fx_bE[0],wbn_b[0]) - fx_mul(fx_bE[3],wbn_b[1]) + fx_mul(fx_bE[2],wbn_b[2]))>>1 ));
// 	dot_b[2] = fx_mul(fx_fxpnt(dT),( (fx_mul( fx_bE[3],wbn_b[0]) + fx_mul(fx_bE[0],wbn_b[1]) - fx_mul(fx_bE[1],wbn_b[2]))>>1 ));
// 	dot_b[3] = fx_mul(fx_fxpnt(dT),( (fx_mul(-fx_bE[2],wbn_b[0]) + fx_mul(fx_bE[1],wbn_b[1]) + fx_mul(fx_bE[0],wbn_b[2]))>>1 ));


// 	for (i=0;i<=3;i++) {
// 		fx_btemp[i]=fx_bE[i]+dot_b[i];
// 	}
//     norm_fx_btemp=fx_sqrt(fx_square(fx_btemp[0])+fx_square(fx_btemp[1])+fx_square(fx_btemp[2])+fx_square(fx_btemp[3]));

// 	for (i=0;i<=3;i++) {
// 		fx_bE[i]=fx_div(fx_btemp[i],norm_fx_btemp);
// 	}

// 	//The following section accumulates phi and Qd fx_bEtween measurements
// 	fx_quat2R(fx_bE, Rb2n);

// 	//costruisco le matrici F e G
// 	for (i=0; i<9; i++){
//         for (j=0; j<9; j++){
//             F[9*i+j]=0;
//             G[9*i+j]=0;
//             if (i<3){
//                 if (j>2 && j<6){
//                     F[9*i+j]=-Rb2n[3*i+j-3];
//                     G[9*i+j]=F[9*i+j];
//                 }
//             }
//             else if (i-j==0){
//                 F[9*i+j]=-fx_fxpnt(0.001);
//                 if (i>5){
//                     G[9*i+j]=FX_ONE;
//                 }
//             }
//             else if (j<3 && i-j==3){
//                 G[9*i+j]=FX_ONE;
//             }
//         }
// 	}


// 	//E=G*Q*G'
// 	fx_Mper(Q, G, m_temp, 9, 9, 9, 9, 2);
// 	fx_Mper(G, m_temp, E, 9, 9, 9, 9, 0);

// 	//Costruisco chi
// 	for (i=0; i<18; i++){
// 	  for (j=0; j<18; j++){
// 	    if (i<9){
// 	      if (j<9){
//             chi[i*18+j]=-fx_mul(F[i*9+j],fx_fxpnt(dT));
// 	      }
// 	      else {
// 		chi[i*18+j]=fx_mul(E[i*9+j-9],fx_fxpnt(dT));
// 	      }
// 	    }
// 	    else {
// 	      if (j<9){
// 		chi[i*18+j]=0;
// 	      }
// 	      else {
// 		chi[i*18+j]=fx_mul(F[(j-9)*9+i-9],fx_fxpnt(dT));
// 	      }
// 	    }
// 	  }
// 	}
// 	//STOPWATCH_START
// 	fx_expm(chi,gamma,fx_fxpnt(0.0001));
//     //STOPWATCH_STOP
// //    time[4]=cyc[1];
// //	expm2(chi,gamma2);
// //    for (i=0;i<8;i++)
// //        printf("%2.3f ",chi[i]);
// //    printf("\n");
// //    for (i=0;i<8;i++)
// //        printf("%2.3f ",gamma[i]);
// //    printf("\n");
// //    for (i=0;i<8;i++)
// //        printf("%2.3f ",gamma2[i]);
// //    printf("\n\n");

// 	//Assegno phi e Qd
// 	for (i=0; i<18; i++){
// 	  for (j=9; j<18; j++){
// 	    if (i<9){
// 	      Qd[i*9+j-9]=gamma[i*18+j];
// 	    }
// 	    else {
//             phi[(j-9)*9+i-9]=gamma[i*18+j];
// 	    }
// 	  }
// 	}



// 	//Aggiornamento matrice cov. P
// 	//STOPWATCH_START
// 	fx_Mper(fx_Pp, phi, Pm, 9, 9, 9, 9, 2);
//     fx_Mper(phi, Pm, m_temp, 9, 9, 9, 9, 0);
//     //STOPWATCH_STOP
//     //time[0]=cyc[1];
// 	fx_Mpiu(m_temp, Qd, Pm, 9, 9);

// 	//calcolo Ra (Rb=[Rb2n Z; Z Rb2n])
// 	for (i=0; i<6; i++){
// 	  for (j=0; j<6; j++){
// 	    if (i<3 && j<3){
// 	      Rb[i*6+j]=Rb2n[i*3+j];
// 	    }
// 	    else if (i>2 && j>2){
// 	      Rb[i*6+j]=Rb2n[(i-3)*3+j-3];
// 	    }
// 	    else
// 	      Rb[i*6+j]=0;
// 	  }
// 	}
// 	//STOPWATCH_START
// 	fx_Mper(R, Rb, m_temp2, 6, 6, 6, 6, 2);
// 	fx_Mper(Rb, m_temp2, Ra, 6, 6, 6, 6, 0);
//     //STOPWATCH_STOP
//     //time[1]=cyc[1];
// 	//Costruisco H
// 	for (i=0; i<6; i++){
// 		  for (j=0; j<9; j++){
// 		    if (i<3 && j<3){
// 		      H[i*9+j]=-gnx[i*3+j];
// 		    }
// 		    else if (i>2 && j<3){
// 		      H[i*9+j]=-mnx[(i-3)*3+j];
// 		    }
// 		    else if (i<3 && j>5){
// 		      H[i*9+j]=Rb2n[i*3+j-6];
// 		    }
// 		    else
// 		      H[i*9+j]=0;
// 		  }
// 	}


// 	//KF gain (Rb  e m_temp2 matrici di appoggio 6x6; m_temp3 6x9)
// 	fx_Mper(Pm, H, m_temp3, 9, 9, 6, 9, 2);
// 	fx_Mper(H, m_temp3, m_temp2, 6, 9, 9, 6, 0);
// 	fx_Mpiu(m_temp2, Ra, Rb, 6, 6);
//     //STOPWATCH_START
//     fx_invGauss (Rb, m_temp2, 6);
//     //STOPWATCH_STOP
//     //time[2]=cyc[1];
// //    //debug
// //	for (i=0;i<6;i++){
// //		for (j=0;j<6;j++){
// //            printf("%2.5f ",fx_float(m_temp2[i*6+j]));
// //		}
// //		printf("\n");
// //	}
// //    printf("\n");
// //	//
//     //STOPWATCH_START
// 	fx_Mper(m_temp3, m_temp2, K, 9, 6, 6, 6, 0);
//     for  (i=0;i<54;i++)
//         K[i]=K[i]<<(FX_FRAC-16);
// 	fx_Mper(K, H, m_temp, 9, 6, 6, 9, 0);
// 	//STOPWATCH_STOP
//     //time[5]=cyc[1];
// //    //Test-K
// //    for (i=0;i<8;i++)
// //        printf("%2.5f ",fx_float(K[i]));
// //    printf("\n\n");

// 	for (i=0;i<9;i++){
// 		for (j=0;j<9;j++){
// 			if (i-j==0)
// 				m_temp[i*9+j]=FX_ONE-m_temp[i*9+j];
// 			else
// 				m_temp[i*9+j]=-m_temp[i*9+j];
// 		}
// 	}
// 	fx_Mper(m_temp, Pm, fx_Pp, 9, 9, 9, 9, 0);


// 	//Calcola il valore predetto della y
// 	for (i=0;i<3;i++){
// 		fx_x_temp[i]=fx_x_aE[i]-Acc_Data[i];
// 	}
// 	fx_Mper(Rb2n, fx_x_temp, fx_hatg_n, 3, 3, 3, 1, 0);
// 	fx_Mper(Rb2n, Mag_Data, fx_hatm_n, 3, 3, 3, 1, 0);

// 	//Calcola il residuo di z
// 	fx_z[0]=-fx_hatg_n[0];
// 	fx_z[1]=-fx_hatg_n[1];
// 	fx_z[2]= fx_fxpnt(ge)-fx_hatg_n[2];
// 	fx_z[3]= fx_fxpnt(me)-fx_hatm_n[0];
// 	fx_z[4]=-fx_hatm_n[1];
// 	fx_z[5]= fx_fxpnt(meo)-fx_hatm_n[2];
// 	//aggiornamento errore dx
// 	fx_Mper(K, fx_z, del_x, 9, 6, 6, 1, 0);

// //    for (i=0;i<6;i++)
// //        printf("%2.4f  ",fx_float(fx_z[i]));
// //    printf("\n\n");

// 	//correzione errore matrice (rho_cross qui corrisponde a (eye(3)-rho_cross)' in matlab)
// 	rho_cross[0]=FX_ONE;
// 	rho_cross[1]=-del_x[2];
// 	rho_cross[2]=del_x[1];
// 	rho_cross[3]=del_x[2];
// 	rho_cross[4]=FX_ONE;
// 	rho_cross[5]=-del_x[0];
// 	rho_cross[6]=-del_x[1];
// 	rho_cross[7]=del_x[0];
// 	rho_cross[8]=FX_ONE;
// 	fx_Mper(rho_cross, Rb2n, Rb2n_temp, 3, 3, 3, 3, 0);
// 	for (i=0;i<9;i++){
// 		Rb2n[i]=Rb2n_temp[i];
// 	}

// //kjghkjgkjg
// //	        for (i=0;i<3;i++)
// //        {
// //            for (j=0;j<3;j++)
// //                printf("%2.9f\t",fx_float(Rb2n[i*3+j]));
// //            printf("\n");
// //        }
// //        printf("\n\n");
// ////
//     //correzione quaternione e bias
// 	fx_rot2quat(Rb2n, fx_bE);

// 	for (i=0;i<3;i++){
// 		fx_x_gE[i]=fx_x_gE[i]+del_x[i+3];
// 		fx_x_aE[i]=fx_x_aE[i]+del_x[i+6];
// 	}
// 	for (i=0;i<4;i++)
//         float_bE[i]=fx_float(fx_bE[i]);
// 	//angoli eulero da quaternione
//     //STOPWATCH_START
// 	Euler_Angles[0]=-r2d*atan2(2*(-float_bE[2]*float_bE[3] + float_bE[0]*float_bE[1]), 1 - 2*(float_bE[1]*float_bE[1] + float_bE[2]*float_bE[2]));
// 	Euler_Angles[1]=-r2d*asin(2*(float_bE[1]*float_bE[3] + float_bE[0]*float_bE[2]));
// 	Euler_Angles[2]=-r2d*atan2(2*(-float_bE[1]*float_bE[2] + float_bE[0]*float_bE[3]), 1 - 2*(float_bE[2]*float_bE[2] + float_bE[3]*float_bE[3]));
//     //STOPWATCH_STOP
//     //time[3]=cyc[1];
// }


// void fx_swap_riga (fx_int* A, int ordine, int riga1, int riga2)
// {
// 	fx_int temp;
//     int i;
//     for (i=0;i<ordine;i++)
//         {
//             temp=A[(riga1*ordine)+i];
//             A[(riga1*ordine)+i]=A[(riga2*ordine)+i];
//             A[(riga2*ordine)+i]=temp;
//         }
// }

// void fx_comb_lin (fx_int* A, int ordine, int riga1, int riga2, fx_int coeff)
// {
//     int i;
//     for (i=0;i<ordine;i++)
//         A[(riga2*ordine)+i]+=fx_mul(A[(riga1*ordine)+i],coeff);
// }


// void fx_invGauss (fx_int* A, fx_int* Y, int ordine)
// {
//     int lenght=ordine*ordine;
// //    float float_A[lenght], float_Y[lenght];		// gives error !!!
// 		float float_A[81], float_Y[81];
//     int i;
// /*
//     printf("original:\n\r");
//     for (int index1=0;index1<6; index1++) {
//       for (int index2=0;index2<6; index2++)
//         printf("%d,\t",A[index1+6*index2]);
//       printf("\n\r");
//     }
// */
//     for (i=0;i<lenght;i++)
//     {
//         float_A[i]=fx_float(A[i]);
//     }
//     Matrix_Inversion(float_A,float_Y, ordine);
//     for (i=0;i<lenght;i++)
//     {
//         Y[i]=fx_fxpnt(float_Y[i]/(1<<(FX_FRAC-16)));
//     }
// /*
//     printf("inverted:\n\r");
//     for (int index1=0;index1<6; index1++) {
//       for (int index2=0;index2<6; index2++)
//         printf("%d,\t",Y[index1+6*index2]);
//       printf("\n\r");
//     }
// */
// //    int c=0,i,j,flag=0;
// //    fx_int coeff,temp;
// //    for (i=0;i<ordine;i++)
// //        for (j=0;j<ordine;j++)
// //            {
// //                if (i==j)
// //                    Y[c]=FX_ONE;
// //                else
// //                    Y[c]=0;
// //                c++;
// //            }
// //
// //    //Azzera gli elementi sotto la diagonale
// //    for (i=0;i<ordine-1;i++)
// //        {
// //            //Cerca la riga con l'elemento maggiore sulla colonna i-esima e la scambia con la riga corrente
// //            coeff=0;
// //            flag=i;
// //            for (j=i;j<ordine;j++)
// //            {
// //                temp=ABS(A[j*ordine+i]);
// //                if (temp>coeff)
// //                    {
// //                        coeff=temp;
// //                        flag=j;
// //                    }
// //            }
// //            fx_swap_riga(A,ordine,i,flag);
// //            fx_swap_riga(Y,ordine,i,flag);
// //
// //            //Azzera l'elemento sotto il pivot per ogni riga mediante combinazione lineare
// //            for (j=i+1;j<ordine;j++)
// //                {
// //                    coeff=fx_div(-A[j*ordine+i],A[i*ordine+i]);
// //                    if (coeff!=0)
// //                    {
// //                        fx_comb_lin(A,ordine,i,j,coeff);
// //                        fx_comb_lin(Y,ordine,i,j,coeff);
// //                    }
// //                }
// //        }
// //    //Azzera gli elementi sopra la diagonale
// //    for (j=ordine-1;j>=0;j--)
// //        {
// //            for (i=j-1;i>=0;i--)
// //                {
// //                    temp=A[i*ordine+j];
// //                    if (temp!=0)
// //                        {
// //                            coeff=fx_div(-temp,A[j*ordine+j]);
// //                            fx_comb_lin(A,ordine,j,i,coeff);
// //                            fx_comb_lin(Y,ordine,j,i,coeff);
// //                        }
// //                }
// //
// //            //Normalizza dividendo la riga per il pivot
// //            coeff=A[j*ordine+j];
// //            A[j*ordine+j]=FX_ONE;
// //            for (i=0;i<ordine;i++)
// //                {
// ////                    A[i*ordine+j]=A[i*ordine+j]/coeff;
// //                    Y[j*ordine+i]=fx_div(Y[j*ordine+i],coeff);
// //                }
// //        }

// }
