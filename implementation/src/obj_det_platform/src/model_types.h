#ifndef SRC_MODEL_TYPES_H_
#define SRC_MODEL_TYPES_H_


#include "const.def"
//#include "ap_cint.h"
// Macro to define fire_models
#define FIRE(inputDim, insideDim, halfOutputDim)  {					\
					model_t sq1_1[insideDim][inputDim];				\
					model_t sq1_1_bias[insideDim];					\
					model_t ex1_1[halfOutputDim][insideDim];			\
					model_t ex1_1_bias[halfOutputDim];				\
					model_t ex3_3[halfOutputDim][3][3][insideDim];	\
					model_t ex3_3_bias[halfOutputDim];				\
				}

typedef signed char model_t;
typedef signed char activation_t;
typedef short mult_t;
typedef int accum_t;
typedef unsigned char img_t;
typedef signed char bits_t;
typedef float finish_t;

typedef struct fire_model_2 FIRE(64,32,64) fire_model_2;
typedef struct fire_model_3 FIRE(128,32,64) fire_model_3;
typedef struct fire_model_4 FIRE(128,32,128) fire_model_4;
typedef struct fire_model_5 FIRE(256,32,128) fire_model_5;
typedef struct fire_model_6 FIRE(256,64,192) fire_model_6;
typedef struct fire_model_7 FIRE(384,64,192) fire_model_7;
typedef struct fire_model_8 FIRE(384,64,256) fire_model_8;
typedef struct fire_model_9 FIRE(512,64,256) fire_model_9;
typedef struct fire_model_10 FIRE(512,96,384) fire_model_10;
typedef struct fire_model_11 FIRE(768,96,384) fire_model_11;


typedef struct net_model {
	model_t conv1[64][3][3][8];
	model_t conv1_bias[64];
	fire_model_2 fire2;
	fire_model_3 fire3;
	fire_model_4 fire4;
	fire_model_5 fire5;
	fire_model_6 fire6;
	fire_model_7 fire7;
	fire_model_8 fire8;
	fire_model_9 fire9;
	fire_model_10 fire10;
	fire_model_11 fire11;
	model_t convDet[8][128][3][3][96];
	model_t convDet_bias[CONVDET_CHANNELS];
	model_t convDet_bias_zero[128];
} net_model;

#endif
