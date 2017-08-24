
//dimensions of hw memories/units
#define MAX_INP_CHANNELS 96
#define MAX_INP_W 312 //309
#define MAX_INP_H 35 //93

#define OUTP_HW_UNITS_1x1 32
#define MAX_OUT_CHANNELS_PER_UNIT_1x1 12
#define PARALLEL_INPUT_CHANNEL_UNITS_1x1 2
//#define PRL_INP_U_PER_BLCK (OUTP_HW_UNITS_1x1/8) // (?!?)(8/PARALLEL_INPUT_CHANNEL_UNITS_1x1)
#define MAX_IO_CHANNELS_PER_OHW_UNIT (MAX_INP_CHANNELS*2*MAX_OUT_CHANNELS_PER_UNIT_1x1)
#define OUTPUT_PKGS (OUTP_HW_UNITS_1x1 / 8)

#define RUN_DEPTH_H 8
#define OUTP_HW_UNITS_3x3 64
#define MAX_OUT_CHANNELS_PER_UNIT_3x3 6
#define PARALLEL_INPUT_CHANNEL_UNITS_3x3 2

#define HALF_KERNEL_SIZE_3x3 1

#include <assert.h>

#include "const.def"
#include "model_types.h"



#ifdef __SDSVHLS__

typedef model_t model_hw_t;
typedef activation_t activation_hw_t;
typedef mult_t mult_hw_t;
typedef accum_t accum_hw_t;
typedef img_t img_hw_t;
typedef bits_t bits_hw_t;
typedef finish_t finish_hw_t;

#else
typedef model_t model_hw_t;
typedef activation_t activation_hw_t;
typedef mult_t mult_hw_t;
typedef accum_t accum_hw_t;
typedef img_t img_hw_t;
typedef bits_t bits_hw_t;
typedef finish_t finish_hw_t;
#endif

typedef struct {
	model_hw_t data[8];
}__attribute__((packed, aligned(1))) io_model_hw_t;

typedef struct {
	activation_hw_t data[8];
}__attribute__((packed, aligned(1))) io_act_hw_t;

int perform_convolution_hw( io_model_hw_t * conv_params,  io_model_hw_t * bias_params, int kernel_size,
						int input_channels, int output_channels, int padding, int inp_w, int inp_h,
						int outp_w, int outp_h,  io_act_hw_t * inp_buf, io_act_hw_t * outp_buf, int stride, int relu,
						int is_fire_expand_layer, int outp_channel_offset, bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits);

int conv_hw_1x1( io_model_hw_t conv_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1*MAX_INP_CHANNELS*2/8],
						io_model_hw_t bias_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						int input_channels, int output_channels_per_hw_unit, int inp_w, int inp_h,
						int outp_w, int outp_h, io_act_hw_t inp_buf1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t outp_buf[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8], int concat_nr,
						bits_hw_t inp_act_int_bits, bits_hw_t outp_act_int_bits, bits_hw_t conv_int_bits, bits_hw_t bias_int_bits);


#pragma SDS data access_pattern(inp_buf:SEQUENTIAL)
#pragma SDS data access_pattern(outp_buf:SEQUENTIAL)
#pragma SDS data access_pattern(conv_params:SEQUENTIAL)
#pragma SDS data access_pattern(bias_params:SEQUENTIAL)
#pragma SDS data copy(inp_buf[0:inp_w*inp_h*input_channels/8])
#pragma SDS data copy(outp_buf[0:output_channels_per_hw_unit*OUTP_HW_UNITS_3x3*outp_w*outp_h/8*(1+relu_off_and_16bit_out)])
#pragma SDS data copy(conv_params[0:output_channels_per_hw_unit*OUTP_HW_UNITS_3x3*input_channels*9/8])
#pragma SDS data copy(bias_params[0:output_channels_per_hw_unit*OUTP_HW_UNITS_3x3/8])
//#pragma SDS data sys_port(inp_buf:AFI)
//#pragma SDS data sys_port(outp_buf:AFI)
//#pragma SDS data sys_port(conv_params:AFI)
//#pragma SDS data sys_port(bias_params:AFI)
#pragma SDS data mem_attribute (inp_buf:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (outp_buf:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (conv_params:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (bias_params:PHYSICAL_CONTIGUOUS)
#pragma SDS data data_mover(inp_buf:AXIDMA_SG)
#pragma SDS data data_mover(outp_buf:AXIDMA_SG)
#pragma SDS data data_mover(conv_params:AXIDMA_SIMPLE)
#pragma SDS data data_mover(bias_params:AXIDMA_SIMPLE)
//#pragma SDS data buffer_depth(conv_params:64,bias_params:64,inp_buf:64,outp_buf:64)
void conv_hw_3x3( io_model_hw_t conv_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3*9*MAX_INP_CHANNELS/8],
						io_model_hw_t bias_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3/8],
						int input_channels, int output_channels_per_hw_unit, int inp_w, int inp_h,
						int outp_w, int outp_h, io_act_hw_t inp_buf[MAX_INP_W*MAX_INP_H*MAX_INP_CHANNELS/8],
						io_act_hw_t outp_buf[MAX_INP_W*MAX_INP_H*MAX_OUT_CHANNELS_PER_UNIT_3x3*OUTP_HW_UNITS_3x3/8], char relu_off_and_16bit_out,
						bits_hw_t inp_act_int_bits, bits_hw_t outp_act_int_bits, bits_hw_t conv_int_bits, bits_hw_t bias_int_bits);


#pragma SDS data access_pattern(inp_buf1_1:SEQUENTIAL)
#pragma SDS data access_pattern(inp_buf2_1:SEQUENTIAL)
#pragma SDS data access_pattern(outp_buf_1:SEQUENTIAL)
#pragma SDS data access_pattern(inp_buf1_2:SEQUENTIAL)
#pragma SDS data access_pattern(inp_buf2_2:SEQUENTIAL)
#pragma SDS data access_pattern(outp_buf_2:SEQUENTIAL)
#pragma SDS data access_pattern(conv_params:SEQUENTIAL)
#pragma SDS data access_pattern(bias_params:SEQUENTIAL)
#pragma SDS data copy(inp_buf1_1[0:inp_w*inp1_h*input_channels/8])
#pragma SDS data copy(inp_buf2_1[0:inp_w*inp1_h*input_channels/8*concat_nr])
#pragma SDS data copy(inp_buf1_2[0:inp_w*inp2_h*input_channels/8])
#pragma SDS data copy(inp_buf2_2[0:inp_w*inp2_h*input_channels/8*concat_nr])
#pragma SDS data copy(outp_buf_1[0:output_channels_per_hw_unit*OUTP_HW_UNITS_1x1*outp_w*outp1_h/8])
#pragma SDS data copy(outp_buf_2[0:output_channels_per_hw_unit*OUTP_HW_UNITS_1x1*outp_w*outp2_h/8])
#pragma SDS data copy(conv_params[0:(output_channels_per_hw_unit*OUTP_HW_UNITS_1x1*input_channels+concat_nr*output_channels_per_hw_unit*OUTP_HW_UNITS_1x1*input_channels)/8])
#pragma SDS data copy(bias_params[0:output_channels_per_hw_unit*OUTP_HW_UNITS_1x1/8])
//#pragma SDS data sys_port(inp_buf:AFI)
//#pragma SDS data sys_port(outp_buf:AFI)
//#pragma SDS data sys_port(conv_params:AFI)
//#pragma SDS data sys_port(bias_params:AFI)
#pragma SDS data mem_attribute (inp_buf1_1:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (inp_buf2_1:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (outp_buf_1:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (inp_buf1_2:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (inp_buf2_2:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (outp_buf_2:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (conv_params:PHYSICAL_CONTIGUOUS)
#pragma SDS data mem_attribute (bias_params:PHYSICAL_CONTIGUOUS)
#pragma SDS data data_mover(inp_buf1_1:AXIDMA_SIMPLE)
#pragma SDS data data_mover(inp_buf2_1:AXIDMA_SIMPLE)
#pragma SDS data data_mover(outp_buf_1:AXIDMA_SG)
#pragma SDS data data_mover(inp_buf1_2:AXIDMA_SIMPLE)
#pragma SDS data data_mover(inp_buf2_2:AXIDMA_SIMPLE)
#pragma SDS data data_mover(outp_buf_2:AXIDMA_SG)
#pragma SDS data data_mover(conv_params:AXIDMA_SIMPLE)
#pragma SDS data data_mover(bias_params:AXIDMA_SIMPLE)
void conv_hw_1x1_double( io_model_hw_t conv_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1*MAX_INP_CHANNELS*2/8],
						io_model_hw_t bias_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						int input_channels, int output_channels_per_hw_unit, int inp_w, int inp1_h, int inp2_h,
						int outp_w, int outp1_h, int outp2_h, io_act_hw_t inp_buf1_1[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf1_2[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf2_1[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf2_2[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t outp_buf_1[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t outp_buf_2[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8], int concat_nr,
						bits_hw_t inp_act_int_bits, bits_hw_t outp_act_int_bits, bits_hw_t conv_int_bits, bits_hw_t bias_int_bits);

int init_filters_1x1(io_model_hw_t conv_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1*MAX_INP_CHANNELS*2/8],
		io_model_hw_t bias_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8], int concat,
		model_hw_t filter_conv[OUTP_HW_UNITS_1x1][MAX_IO_CHANNELS_PER_OHW_UNIT],
		model_hw_t filter_bias[OUTP_HW_UNITS_1x1][MAX_OUT_CHANNELS_PER_UNIT_1x1], int output_channels_per_hw_unit, int input_channels);



int perf_conv_1x1_1(model_hw_t filter_conv[OUTP_HW_UNITS_1x1][MAX_IO_CHANNELS_PER_OHW_UNIT],
		model_hw_t filter_bias[OUTP_HW_UNITS_1x1][MAX_OUT_CHANNELS_PER_UNIT_1x1],
		io_act_hw_t inp_buf1_1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t inp_buf2_1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t outp_buf_1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t inp_buf1_2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t inp_buf2_2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t outp_buf_2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		int inout_w, int inout1_h, int inout2_h, int input_channels, int output_channels_per_hw_unit,
		bits_hw_t bias_rightshift_bits, bits_hw_t out_rightshift_bits, int concat);


int init_filters_3x3(io_model_hw_t conv_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3*9*MAX_INP_CHANNELS/8],
		io_model_hw_t bias_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3/8],
		model_hw_t filter_conv[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3][3][3][MAX_INP_CHANNELS],
		model_hw_t filter_bias[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3], int output_channels_per_hw_unit, int input_channels);

int init_input_first_line(io_act_hw_t inp_stream[MAX_INP_W*MAX_INP_H*MAX_INP_CHANNELS/8], activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS],
		int inp_w, int inp_ch);

int transfer_input(io_act_hw_t inp_stream[MAX_INP_W*MAX_INP_H*MAX_INP_CHANNELS/8], activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS],
		int inp_w, int inp_h, int inp_ch, int start_height);

int perf_conv_3x3(model_hw_t filter_conv[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3][3][3][MAX_INP_CHANNELS],
		model_hw_t filter_bias[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3],
		activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS],
		activation_hw_t  output_buff[OUTP_HW_UNITS_3x3][2][MAX_OUT_CHANNELS_PER_UNIT_3x3/2][RUN_DEPTH_H][MAX_INP_W],
		int inout_w, int inout_h, int input_channels, int output_channels_per_hw_unit,
		bits_hw_t bias_rightshift_bits, bits_hw_t out_rightshift_bits, int start_height, char relu_off_and_16bit_out);

int transfer_output(io_act_hw_t outp_stream[MAX_INP_W*MAX_INP_H*MAX_OUT_CHANNELS_PER_UNIT_3x3*OUTP_HW_UNITS_3x3/8],
		activation_hw_t  output_buff[OUTP_HW_UNITS_3x3][2][MAX_OUT_CHANNELS_PER_UNIT_3x3/2][RUN_DEPTH_H][MAX_INP_W],
		int outp_w, int outp_h, int out_channels_per_hw_unit, int start_height, char relu_off_and_16bit_out);







