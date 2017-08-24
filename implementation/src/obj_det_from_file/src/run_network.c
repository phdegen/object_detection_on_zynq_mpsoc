
#include "const.def"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <omp.h>
#include "sds_lib.h"

#include "run_network.h"
#include "hw_convs.h"


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

bits_t integer_bits_per_act_layer[] = {
8, //meaned input image
10, //conv1
11,11, //fire2 ..
11,11,
11,11,
11,10, //5
11,11,
10,10,
9,8,  //8
8,7,
5,4,
3,2,
5,
0};//last is softmax_layer

bits_t integer_bits_per_layer[] = {
0,-1, //conv1: conv,bias
1,-1,0,-2,0,-3,
0,-1,0,-1,0,-2,
0,-2,0,-3,0,-4,
0,-2,0,-2,0,-4,
0,-2,-1,-3,0,-4,
0,-2,-1,-1,-1,-3,
0,3,0,0,-1,-1,
-1,1,-1,-3,-1,-3,
-2,-1,-3,-3,-2,-1,
-1,0,-2,-2,-2,-1,
-1,1};


int run_network(net_model * model, activation_t * image, activation_t * fm_buf_1, activation_t * fm_buf_2) {
	int in_w, in_h;
	int out_w, out_h;
	double begin,end;
	int i,j;

	// Convolution 1
	out_w = (int) ceilf((IMG_WIDTH - 3 + 1) / 2.0);
	out_h = (int) ceilf((IMG_HEIGHT - 3 + 1) / 2.0);
	begin = omp_get_wtime();
	split_image(image, fm_buf_2);

	for(i=0;i<16;i++){
		conv_hw_3x3((io_model_hw_t *)(*model).conv1, (io_model_hw_t *)(*model).conv1_bias,8, 1,
					IMG_WIDTH/4+2, IMG_HEIGHT/4+2, IMG_WIDTH/4+2, IMG_HEIGHT/4+2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE*i/16), (io_act_hw_t *)(fm_buf_1+MAX_FEATURE_MAP_SIZE*i/16),0,
					integer_bits_per_act_layer[0],integer_bits_per_act_layer[1],
					integer_bits_per_layer[0],integer_bits_per_layer[1]);

	}
	end = omp_get_wtime();
	printf("L1 Wall time for 3x3 convolution (with split_image and without pool&concat is %2.6f\n", end-begin);



	//Pooling L 1
	in_w = out_w;
	in_h = out_h;
	out_w = (int) ceilf((in_w - 3 + 1) / 2.0);
	out_h = (int) ceilf((in_h - 3 + 1) / 2.0);
	begin = omp_get_wtime();
	pooling_with_stride_and_concat(in_w, in_h, out_w, out_h, 64, fm_buf_1, fm_buf_2);
	end = omp_get_wtime();
	printf("L1 Wall time for Pooling with stride and concat is %2.6f\n", end-begin);


	//Fire 2
	in_w = out_w;
	in_h = out_h;
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire2.sq1_1, (io_model_hw_t *)(*model).fire2.sq1_1_bias,  64, 1,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2,(io_act_hw_t *)fm_buf_2, (io_act_hw_t *)fm_buf_1,0,
		integer_bits_per_act_layer[1],integer_bits_per_act_layer[2],
		integer_bits_per_layer[2],integer_bits_per_layer[3]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L2 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire2.ex1_1, (io_model_hw_t *)(*model).fire2.ex1_1_bias, 32, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1,(io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[2],integer_bits_per_act_layer[3],
		integer_bits_per_layer[4],integer_bits_per_layer[5]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire2.ex3_3, (io_model_hw_t *)(*model).fire2.ex3_3_bias, 32, 1,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[2],integer_bits_per_act_layer[3],
		integer_bits_per_layer[6],integer_bits_per_layer[7]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L2 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);

	//Fire 3
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire3.sq1_1, (io_model_hw_t *)(*model).fire3.sq1_1_bias, 64, 1,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[3],integer_bits_per_act_layer[4],
		integer_bits_per_layer[8],integer_bits_per_layer[9]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L3 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire3.ex1_1, (io_model_hw_t *)(*model).fire3.ex1_1_bias, 32, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[4],integer_bits_per_act_layer[5],
		integer_bits_per_layer[10],integer_bits_per_layer[11]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire3.ex3_3, (io_model_hw_t *)(*model).fire3.ex3_3_bias, 32, 1,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[4],integer_bits_per_act_layer[5],
		integer_bits_per_layer[12],integer_bits_per_layer[13]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L3 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);

	// Pooling L 3
	out_w = (int) ceilf((in_w - 3 + 1) / 2.0);
	out_h = (int) ceilf((in_h - 3 + 1) / 2.0);
	pooling(in_w, in_h, out_w, out_h, 128, fm_buf_2, fm_buf_1,1);


	//Fire 4
	in_w = out_w;
	in_h = out_h;
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire4.sq1_1, (io_model_hw_t *)(*model).fire4.sq1_1_bias, 64, 1,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_1+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_2,1,
		integer_bits_per_act_layer[5],integer_bits_per_act_layer[6],
		integer_bits_per_layer[14],integer_bits_per_layer[15]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L4 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire4.ex1_1, (io_model_hw_t *)(*model).fire4.ex1_1_bias, 32, 4,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)fm_buf_1,0,
		integer_bits_per_act_layer[6],integer_bits_per_act_layer[7],
		integer_bits_per_layer[16],integer_bits_per_layer[17]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire4.ex3_3, (io_model_hw_t *)(*model).fire4.ex3_3_bias, 32, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_1+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[6],integer_bits_per_act_layer[7],
		integer_bits_per_layer[18],integer_bits_per_layer[19]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L4 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);



	//Fire 5
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire5.sq1_1, (io_model_hw_t *)(*model).fire5.sq1_1_bias, 128, 1,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_1+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_2,1,
		integer_bits_per_act_layer[7],integer_bits_per_act_layer[8],
		integer_bits_per_layer[20],integer_bits_per_layer[21]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L5 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire5.ex1_1, (io_model_hw_t *)(*model).fire5.ex1_1_bias, 32, 4,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)fm_buf_1,0,
		integer_bits_per_act_layer[8],integer_bits_per_act_layer[9],
		integer_bits_per_layer[22],integer_bits_per_layer[23]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire5.ex3_3, (io_model_hw_t *)(*model).fire5.ex3_3_bias, 32, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_1+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[8],integer_bits_per_act_layer[9],
		integer_bits_per_layer[24],integer_bits_per_layer[25]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L5 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);


	// Pooling L 5
	out_w = (int) ceilf((in_w - 3 + 1) / 2.0);
	out_h = (int) ceilf((in_h - 3 + 1) / 2.0);
	pooling(in_w, in_h, out_w, out_h, 256, fm_buf_1, fm_buf_2,1);

	//Fire 6
	in_w = out_w;
	in_h = out_h;
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire6.sq1_1, (io_model_hw_t *)(*model).fire6.sq1_1_bias, 128, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[9],integer_bits_per_act_layer[10],
		integer_bits_per_layer[26],integer_bits_per_layer[27]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L6 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire6.ex1_1, (io_model_hw_t *)(*model).fire6.ex1_1_bias, 64, 6,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[10],integer_bits_per_act_layer[11],
		integer_bits_per_layer[28],integer_bits_per_layer[29]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire6.ex3_3, (io_model_hw_t *)(*model).fire6.ex3_3_bias, 64, 3,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[10],integer_bits_per_act_layer[11],
		integer_bits_per_layer[30],integer_bits_per_layer[31]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L6 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);

	//Fire 7
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire7.sq1_1, (io_model_hw_t *)(*model).fire7.sq1_1_bias, 192, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[11],integer_bits_per_act_layer[12],
		integer_bits_per_layer[32],integer_bits_per_layer[33]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L7 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire7.ex1_1, (io_model_hw_t *)(*model).fire7.ex1_1_bias, 64, 6,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[12],integer_bits_per_act_layer[13],
		integer_bits_per_layer[34],integer_bits_per_layer[35]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire7.ex3_3, (io_model_hw_t *)(*model).fire7.ex3_3_bias, 64, 3,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[12],integer_bits_per_act_layer[13],
		integer_bits_per_layer[36],integer_bits_per_layer[37]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L7 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);


	//Fire 8
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire8.sq1_1, (io_model_hw_t *)(*model).fire8.sq1_1_bias, 192, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[13],integer_bits_per_act_layer[14],
		integer_bits_per_layer[38],integer_bits_per_layer[39]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L8 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire8.ex1_1, (io_model_hw_t *)(*model).fire8.ex1_1_bias, 64, 8,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[14],integer_bits_per_act_layer[15],
		integer_bits_per_layer[40],integer_bits_per_layer[41]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire8.ex3_3, (io_model_hw_t *)(*model).fire8.ex3_3_bias, 64, 4,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[14],integer_bits_per_act_layer[15],
		integer_bits_per_layer[42],integer_bits_per_layer[43]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L8 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);

	//Fire 9
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire9.sq1_1, (io_model_hw_t *)(*model).fire9.sq1_1_bias, 256, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[15],integer_bits_per_act_layer[16],
		integer_bits_per_layer[44],integer_bits_per_layer[45]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L9 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire9.ex1_1, (io_model_hw_t *)(*model).fire9.ex1_1_bias, 64, 8,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[16],integer_bits_per_act_layer[17],
		integer_bits_per_layer[46],integer_bits_per_layer[47]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire9.ex3_3, (io_model_hw_t *)(*model).fire9.ex3_3_bias, 64, 4,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[16],integer_bits_per_act_layer[17],
		integer_bits_per_layer[48],integer_bits_per_layer[49]);
	#pragma SDS wait(1)
	#pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L9 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);


	//Fire 10
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire10.sq1_1, (io_model_hw_t *)(*model).fire10.sq1_1_bias, 256, 3,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[17],integer_bits_per_act_layer[18],
		integer_bits_per_layer[50],integer_bits_per_layer[51]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L10 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire10.ex1_1, (io_model_hw_t *)(*model).fire10.ex1_1_bias, 96, 12,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[18],integer_bits_per_act_layer[19],
		integer_bits_per_layer[52],integer_bits_per_layer[53]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire10.ex3_3, (io_model_hw_t *)(*model).fire10.ex3_3_bias, 96, 6,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[18],integer_bits_per_act_layer[19],
		integer_bits_per_layer[54],integer_bits_per_layer[55]);
	#pragma SDS wait(1)
    #pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L10 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);


	//Fire 11
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire11.sq1_1, (io_model_hw_t *)(*model).fire11.sq1_1_bias, 384, 3,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_2, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2), (io_act_hw_t *)fm_buf_1,1,
		integer_bits_per_act_layer[19],integer_bits_per_act_layer[20],
		integer_bits_per_layer[56],integer_bits_per_layer[57]);
	#pragma SDS wait(1)
	end = omp_get_wtime();
	printf("L11 Wall time for Sq1x1 convolution is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	conv_hw_1x1((io_model_hw_t *)(*model).fire11.ex1_1, (io_model_hw_t *)(*model).fire11.ex1_1_bias, 96, 12,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)fm_buf_2,0,
		integer_bits_per_act_layer[20],integer_bits_per_act_layer[21],
		integer_bits_per_layer[58],integer_bits_per_layer[59]);
	#pragma SDS async(2)
	conv_hw_3x3((io_model_hw_t *)(*model).fire11.ex3_3, (io_model_hw_t *)(*model).fire11.ex3_3_bias, 96, 6,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)fm_buf_1, (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE/2),0,
		integer_bits_per_act_layer[20],integer_bits_per_act_layer[21],
		integer_bits_per_layer[60],integer_bits_per_layer[61]);
	#pragma SDS wait(1)
    #pragma SDS wait(2)
	end = omp_get_wtime();
	printf("L11 Wall time for concurrentExp convolutions is %2.6f\n", end-begin);



	begin = omp_get_wtime();
	deconcat(out_w, out_h, 384, fm_buf_2, fm_buf_1);
	end = omp_get_wtime();
	printf("convdet Wall time for deconcat is %2.6f\n", end-begin);


	//ConvDet
	begin = omp_get_wtime();
	for(j=0;j<8;j++){
	conv_hw_3x3((io_model_hw_t *)(*model).convDet[j], (io_model_hw_t *)(*model).convDet_bias_zero, 96, 2,
		in_w, in_h, out_w, out_h, (io_act_hw_t *)(fm_buf_1+MAX_FEATURE_MAP_SIZE*j/8), (io_act_hw_t *)(fm_buf_2+MAX_FEATURE_MAP_SIZE*j/8),1,
		integer_bits_per_act_layer[21],integer_bits_per_act_layer[22],
		integer_bits_per_layer[62],integer_bits_per_layer[63]);
	}
	end = omp_get_wtime();
	printf("ConvDet Wall time for Exp is %2.6f\n", end-begin);

	begin = omp_get_wtime();
	sum_intermediate_results(out_w, out_h, 8, 128, CONVDET_CHANNELS, (*model).convDet_bias, integer_bits_per_act_layer[22],integer_bits_per_layer[63], fm_buf_2, fm_buf_1);
	end = omp_get_wtime();
	printf("ConvDet Wall time for sum_interm_res is %2.6f\n", end-begin);

	begin = omp_get_wtime();
	perform_softmax_sigmoid(out_w, out_h, fm_buf_1, integer_bits_per_act_layer[22], integer_bits_per_act_layer[23]);
	end = omp_get_wtime();
	printf("LSoftmax/Sigmoid time is %2.6f\n", end-begin);

	return out_w*out_h;
}



//Max-Pooling without padding: size 3, stride 2
int pooling(int inp_w, int inp_h, int outp_w, int outp_h, int channels, activation_t * inp_buf, activation_t * outp_buf, int concat){
	int j,i,q,p,m;

	int channels_comp= (concat==0) ? channels : channels/2;
	activation_t * temp_in_buf_ptr[2] = {inp_buf,inp_buf+MAX_FEATURE_MAP_SIZE/2};
	activation_t * temp_out_buf_ptr[2] = {outp_buf,outp_buf+MAX_FEATURE_MAP_SIZE/2};


	//#pragma omp parallel for private(j,i,q,m) collapse(2)
	for(j=0;j<outp_h;j++){
		for(i=0;i<outp_w;i++){
			for(p=0;p<=concat;p++){
				for(q=0;q<channels_comp;q++){
					activation_t max_values[3];
					activation_t temp;
					for(m=0;m<3;m++){

						if((2*j+m>inp_h)){
							max_values[m]=-127;
							continue;
						}
						temp = MAX((temp_in_buf_ptr[p])[((2*j+m)*inp_w+2*i)*channels_comp+q],(temp_in_buf_ptr[p])[((2*j+m)*inp_w+2*i+1)*channels_comp+q]);
						if(2*i+2<inp_w)
							max_values[m] = MAX(temp,(temp_in_buf_ptr[p])[((2*j+m)*inp_w+2*i+2)*channels_comp+q]);
						else
							max_values[m] = temp;
						}
					(temp_out_buf_ptr[p])[(j*outp_w+i)*channels_comp+q] = MAX(max_values[0],MAX(max_values[1],max_values[2]));

				}
			}
		}
	}
	return 0;
}


int perform_softmax_sigmoid(int inp_w, int inp_h, activation_t * fm_buf,/* float * int_data, */ bits_t int_bits_convdet, bits_t softmax_bits){
	//Sigmoid only on NUM_ANCHOR of objectness scores (confidence)
	//Softmax only over NUM_ANCHOR * NUM_CLASSES  from channels and each only over NUM_CLASSES
	//including multiplication with sigmoid of objectness(confidence)
	int pixels, anchors,classes;
	//#pragma omp parallel for private(pixels, anchors, classes)
	for(pixels=0;pixels<inp_w*inp_h;pixels++){
		for(anchors=0;anchors<NUM_ANCHOR;anchors++){
			float sum=0;
			float sigmoid;
			float exps[NUM_CLASSES];

			//Sigmoid on confidence scores
			sigmoid = 1/(1+ expf(-((finish_t *)fm_buf)[pixels*CONVDET_CHANNELS+NUM_ANCHOR*NUM_CLASSES+anchors]));

			//Softmax over classes including multiplication with confidence score
			for(classes=0;classes<NUM_CLASSES;classes++){
				exps[classes] = expf(((finish_t *)fm_buf)[pixels*CONVDET_CHANNELS+anchors*NUM_CLASSES+classes]);
				sum += exps[classes];
			}
			for(classes=0;classes<NUM_CLASSES;classes++){
				((finish_t *)fm_buf)[pixels*CONVDET_CHANNELS+anchors*NUM_CLASSES+classes] = sigmoid * exps[classes]/sum;
			}
		}
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////
/////////////////  Layer 1 helper functions
/////////////////////////////////////////////////////////////////////


int split_image(activation_t * image, activation_t * fm_buf_2){

	int i,j,k,t,u;
	int newwidth = IMG_WIDTH/4+2;
	int newheight = IMG_HEIGHT/4+2;
	int offsetnewline = IMG_WIDTH*3;
	int offsetnewcol = (IMG_WIDTH/4)*3;

	activation_t * imagepointers[4][4];
	activation_t * target[4][4];

	for(t=0;t<4;t++){
		for(u=0;u<4;u++){
			imagepointers[t][u]=image+t*(IMG_HEIGHT/4)*offsetnewline+u*offsetnewcol;
			target[t][u] = fm_buf_2+MAX_FEATURE_MAP_SIZE*(4*t+u)/16;
		}
	}


	for(i=0;i<16;i++){
		int a=i/4 , b=i%4;
		for(k=0;k<newheight;k++){
			for(j=0;j<newwidth;j++){
				memcpy(target[a][b],imagepointers[a][b],3);
				memset(target[a][b]+3,0,5);
				imagepointers[a][b] += 3;
				target[a][b] += 8;
			}
			imagepointers[a][b] += offsetnewline-offsetnewcol-6;
		}
	}
return 0;
}


int pooling_with_stride_and_concat(int inp_w, int inp_h, int outp_w, int outp_h, int channels, activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf){
	int j,i,q,m,k,l;

	activation_t * src[4][4] = {{inp_buf, inp_buf+MAX_FEATURE_MAP_SIZE*1/16, inp_buf+MAX_FEATURE_MAP_SIZE*2/16, inp_buf+MAX_FEATURE_MAP_SIZE*3/16},
			{inp_buf+MAX_FEATURE_MAP_SIZE*4/16,inp_buf+MAX_FEATURE_MAP_SIZE*5/16, inp_buf+MAX_FEATURE_MAP_SIZE*6/16, inp_buf+MAX_FEATURE_MAP_SIZE*7/16},
			{inp_buf+MAX_FEATURE_MAP_SIZE*8/16, inp_buf+MAX_FEATURE_MAP_SIZE*9/16, inp_buf+MAX_FEATURE_MAP_SIZE*10/16, inp_buf+MAX_FEATURE_MAP_SIZE*11/16},
			{inp_buf+MAX_FEATURE_MAP_SIZE*12/16, inp_buf+MAX_FEATURE_MAP_SIZE*13/16, inp_buf+MAX_FEATURE_MAP_SIZE*14/16, inp_buf+MAX_FEATURE_MAP_SIZE*15/16}};

	activation_t * pntr[3][3]={{inp_buf,inp_buf,inp_buf},{inp_buf,inp_buf,inp_buf},{inp_buf,inp_buf,inp_buf}};
	int imgNr[2][3] = { { 0 } };
	int imagepartwidth=IMG_WIDTH/4+2;
	int limits[2] = {IMG_WIDTH/4+1,IMG_HEIGHT/4+1};
	int pos[2][3]={{1,3,5},{1,3,5}}; // x , y

	//#pragma omp for private(j,i,q,m) collapse(2) schedule(dynamic)
	for(j=0;j<outp_h;j++){
		for(i=0;i<outp_w;i++){

			for(q=0;q<channels;q++){
				activation_t max_values[3];
				activation_t temp;

				for(m=0;m<3;m++){

					temp = MAX((pntr[m][0])[(pos[1][m]*imagepartwidth+pos[0][0])*channels+q],(pntr[m][1])[(pos[1][m]*imagepartwidth+pos[0][1])*channels+q]);
					max_values[m] = MAX(temp,(pntr[m][2])[(pos[1][m]*imagepartwidth+pos[0][2])*channels+q]);

				}
				outp_buf[(j*outp_w+i)*channels+q] = MAX(max_values[0],MAX(max_values[1],max_values[2]));
			}
			for(l=0;l<3;l++){
				pos[0][l]+=4;
				if(pos[0][l]>=limits[0]){
					pos[0][l]=(pos[0][l]+1)%limits[0]; //+1 because the boarders are even
					imgNr[0][l]++;
				}
			}
			for(k=0;k<3;k++){
				for(l=0;l<3;l++){
					pntr[k][l] = src[imgNr[1][k]][imgNr[0][l]];
				}
			}

		}
		for(l=0;l<3;l++){
			pos[1][l]+=4;
			pos[0][l]=2*l+1;
			imgNr[0][l]=0;
			if(pos[1][l]>=limits[1] && imgNr[1][l]<3){
				pos[1][l]=(pos[1][l]+1)%limits[1];
				imgNr[1][l]++;
			}
		}
		for(k=0;k<3;k++){
			for(l=0;l<3;l++){
				pntr[k][l] = src[imgNr[1][k]][imgNr[0][l]];
			}
		}

	}
	return 0;
}




/////////////////////////////////////////////////////////////////////
/////////////////  ConvDet Layer helper functions
/////////////////////////////////////////////////////////////////////


int deconcat(int width, int height, int single_channels, activation_t * fm_buff_1, activation_t * fm_buff_2){

	int a,b,c;
	activation_t * temp_ptr[2]={fm_buff_1,fm_buff_1+MAX_FEATURE_MAP_SIZE/2};
	activation_t * temp_ptr_out[2][4] = {{fm_buff_2,fm_buff_2+MAX_FEATURE_MAP_SIZE/8,fm_buff_2+MAX_FEATURE_MAP_SIZE*2/8,fm_buff_2+MAX_FEATURE_MAP_SIZE*3/8},{fm_buff_2+MAX_FEATURE_MAP_SIZE*4/8, fm_buff_2+MAX_FEATURE_MAP_SIZE*5/8, fm_buff_2+MAX_FEATURE_MAP_SIZE*6/8, fm_buff_2+MAX_FEATURE_MAP_SIZE*7/8}};

	for(c=0;c<2;c++){
		for(a=0;a<height;a++){
			for(b=0;b<width;b++){
					memcpy(temp_ptr_out[c][0]+(a*width+b)*96,temp_ptr[c]+(a*width+b)*single_channels,96);
					memcpy(temp_ptr_out[c][1]+(a*width+b)*96,temp_ptr[c]+(a*width+b)*single_channels+96,96);
					memcpy(temp_ptr_out[c][2]+(a*width+b)*96,temp_ptr[c]+(a*width+b)*single_channels+192,96);
					memcpy(temp_ptr_out[c][3]+(a*width+b)*96,temp_ptr[c]+(a*width+b)*single_channels+288,96);
			}
		}
	}



	return 0;
}

int sum_intermediate_results(int width, int height, int fm_sets, int input_channels, int output_channels, model_t * bias, int outp_act_int_bits, int bias_int_bits, activation_t * fm_buff_1, activation_t * fm_buff_2){

	int a,b,c,d;
	const bits_t bias_rightshift_bits =(outp_act_int_bits-bias_int_bits-8);
	activation_t * temp_ptr_in[8] = {fm_buff_1,fm_buff_1+MAX_FEATURE_MAP_SIZE/8,fm_buff_1+MAX_FEATURE_MAP_SIZE*2/8,fm_buff_1+MAX_FEATURE_MAP_SIZE*3/8, fm_buff_1+MAX_FEATURE_MAP_SIZE*4/8, fm_buff_1+MAX_FEATURE_MAP_SIZE*5/8, fm_buff_1+MAX_FEATURE_MAP_SIZE*6/8, fm_buff_1+MAX_FEATURE_MAP_SIZE*7/8};

	for(a=0;a<height;a++){
		for(b=0;b<width;b++){
			for(c=0;c<output_channels;c++){
				short acc=0;
				int offset1 = (a*width+b)*input_channels+c, offset2 = (a*width+b)*output_channels+c;
				for(d=0;d<fm_sets;d++){
					acc+=((short*)temp_ptr_in[d])[offset1];
				}
				acc+= ((short)bias[c])>>bias_rightshift_bits;
				((finish_t*)fm_buff_2)[offset2]=((finish_t)acc)/(pow(2,(float)(QUANT_BITS-outp_act_int_bits+8)));

			}
		}
	}



	return 0;
}



