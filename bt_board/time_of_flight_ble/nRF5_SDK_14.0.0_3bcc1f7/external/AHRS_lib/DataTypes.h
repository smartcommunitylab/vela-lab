
#ifndef DATATYPES_H_
#define DATATYPES_H_

/**
 * @struct
 * @brief sensor data struct
 */

#define DISTANCE_REPORT_DOWNSAMPLE_RATIO 10 //TODO: downsample ratio should be set at run time, or by using a define it should be possible to decide weather the buffer have to be statically or dynamucally allocated

/**
 * @struct
 * @brief raw data struct
 */
typedef struct
{

  float tof[1];		//data in s
  float rssi[2]; 	//data in dBm
  float freq[2];	//data in Hz
//  float tof_O_runtime;

} Sampled_Raw_Data;

/**
 * @struct
 * @brief converted data struct
 */
typedef struct
{
  float r_m[1];	//data in m
  float v_m[1];	//data in m/s
  float dT[1];  //time in s
} Measurement_Data;


/*
typedef struct
{

  int16_t AccOffset[3];
  int16_t GyrOffset[3];
  s32 MagOffset[3];

  int16_t AccGain[9];
  int16_t GyrGain[9];
  int16_t MagGain[9];

} Sensor_CalFX_Data;
*/
typedef struct
{
  float    d[DISTANCE_REPORT_DOWNSAMPLE_RATIO];
  float    v[DISTANCE_REPORT_DOWNSAMPLE_RATIO];
  int8_t    rssi[DISTANCE_REPORT_DOWNSAMPLE_RATIO]; //TODO: maybe this variable should be placed somewereelse
  uint16_t idx;
} Distance_Data;



#endif
