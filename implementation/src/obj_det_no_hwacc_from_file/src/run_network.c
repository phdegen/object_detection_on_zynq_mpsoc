
#include "const.def"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

#include "run_network.h"


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

bits_t integer_bits_per_act_layer[] = {
8, //meaned input image
10, //conv1
11,11, //fire2 ..
11,11,
11,11,
11,10,
11,11,
10,10,
9,8,
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
	double begin, end;


	//#pragma omp parallel private(in_w, in_h, out_w, out_h,begin,end) shared (model, image, fm_buf_1, fm_buf_2) num_threads(4)
	{
	// Convolution 1
	out_w = (int) ceilf((IMG_WIDTH - 3 + 1) / 2.0);
	out_h = (int) ceilf((IMG_HEIGHT - 3 + 1) / 2.0);

	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).conv1, (*model).conv1_bias, 3, 3, 64, 0,
			IMG_WIDTH, IMG_HEIGHT, out_w, out_h, image, fm_buf_1,2,1,0,0,
			integer_bits_per_act_layer[0],integer_bits_per_act_layer[1],
			integer_bits_per_layer[0],integer_bits_per_layer[1]);
	end = omp_get_wtime();
	printf("L1 Wall time for 3x3 convolution is %2.6f\n", end-begin);


	//Pooling L 1
	in_w = out_w;
	in_h = out_h;
	out_w = (int) ceilf((in_w - 3 + 1) / 2.0);
	out_h = (int) ceilf((in_h - 3 + 1) / 2.0);
	begin = omp_get_wtime();
	pooling(in_w, in_h, out_w, out_h, 64, fm_buf_1, fm_buf_2);
	end = omp_get_wtime();
	printf("L1 Wall time for Pooling is %2.6f\n", end-begin);

	//Fire 2
	in_w = out_w;
	in_h = out_h;
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire2.sq1_1, (*model).fire2.sq1_1_bias, 1, 64, 16, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[1],integer_bits_per_act_layer[2],
		integer_bits_per_layer[2],integer_bits_per_layer[3]);
	end = omp_get_wtime();
	printf("L2 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire2.ex1_1, (*model).fire2.ex1_1_bias, 1, 16, 64, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[2],integer_bits_per_act_layer[3],
		integer_bits_per_layer[4],integer_bits_per_layer[5]);
	end = omp_get_wtime();
	printf("L2 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire2.ex3_3, (*model).fire2.ex3_3_bias, 3, 16, 64, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,64,
		integer_bits_per_act_layer[2],integer_bits_per_act_layer[3],
		integer_bits_per_layer[6],integer_bits_per_layer[7]);
	end = omp_get_wtime();
	printf("L2 Wall time for Ex3x3 is %2.6f\n", end-begin);


	//Fire 3
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire3.sq1_1, (*model).fire3.sq1_1_bias, 1, 128, 16, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[3],integer_bits_per_act_layer[4],
		integer_bits_per_layer[8],integer_bits_per_layer[9]);
	end = omp_get_wtime();
	printf("L3 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire3.ex1_1, (*model).fire3.ex1_1_bias, 1, 16, 64, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[4],integer_bits_per_act_layer[5],
		integer_bits_per_layer[10],integer_bits_per_layer[11]);
	end = omp_get_wtime();
	printf("L3 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire3.ex3_3, (*model).fire3.ex3_3_bias, 3, 16, 64, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,64,
		integer_bits_per_act_layer[4],integer_bits_per_act_layer[5],
		integer_bits_per_layer[12],integer_bits_per_layer[13]);
	end = omp_get_wtime();
	printf("L3 Wall time for Ex3x3 is %2.6f\n", end-begin);



	// Pooling L 3
	out_w = (int) ceilf((in_w - 3 + 1) / 2.0);
	out_h = (int) ceilf((in_h - 3 + 1) / 2.0);
	pooling(in_w, in_h, out_w, out_h, 128, fm_buf_2, fm_buf_1);


	//Fire 4
	in_w = out_w;
	in_h = out_h;
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire4.sq1_1, (*model).fire4.sq1_1_bias, 1, 128, 32, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,0,0,
		integer_bits_per_act_layer[5],integer_bits_per_act_layer[6],
		integer_bits_per_layer[14],integer_bits_per_layer[15]);
	end = omp_get_wtime();
	printf("L4 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire4.ex1_1, (*model).fire4.ex1_1_bias, 1, 32, 128, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,1,0,
		integer_bits_per_act_layer[6],integer_bits_per_act_layer[7],
		integer_bits_per_layer[16],integer_bits_per_layer[17]);
	end = omp_get_wtime();
	printf("L4 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire4.ex3_3, (*model).fire4.ex3_3_bias, 3, 32, 128, 1,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,1,128,
		integer_bits_per_act_layer[6],integer_bits_per_act_layer[7],
		integer_bits_per_layer[18],integer_bits_per_layer[19]);
	end = omp_get_wtime();
	printf("L4 Wall time for Ex3x3 is %2.6f\n", end-begin);

	//Fire 5
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire5.sq1_1, (*model).fire5.sq1_1_bias, 1, 256, 32, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,0,0,
		integer_bits_per_act_layer[7],integer_bits_per_act_layer[8],
		integer_bits_per_layer[20],integer_bits_per_layer[21]);
	end = omp_get_wtime();
	printf("L5 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire5.ex1_1, (*model).fire5.ex1_1_bias, 1, 32, 128, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,1,0,
		integer_bits_per_act_layer[8],integer_bits_per_act_layer[9],
		integer_bits_per_layer[22],integer_bits_per_layer[23]);
	end = omp_get_wtime();
	printf("L5 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire5.ex3_3, (*model).fire5.ex3_3_bias, 3, 32, 128, 1,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,1,128,
		integer_bits_per_act_layer[8],integer_bits_per_act_layer[9],
		integer_bits_per_layer[24],integer_bits_per_layer[25]);
	end = omp_get_wtime();
	printf("L5 Wall time for Ex3x3 is %2.6f\n", end-begin);


	// Pooling L 5
	out_w = (int) ceilf((in_w - 3 + 1) / 2.0);
	out_h = (int) ceilf((in_h - 3 + 1) / 2.0);
	pooling(in_w, in_h, out_w, out_h, 256, fm_buf_1, fm_buf_2);



	//Fire 6
	in_w = out_w;
	in_h = out_h;
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire6.sq1_1, (*model).fire6.sq1_1_bias, 1, 256, 48, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[9],integer_bits_per_act_layer[10],
		integer_bits_per_layer[26],integer_bits_per_layer[27]);
	end = omp_get_wtime();
	printf("L6 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire6.ex1_1, (*model).fire6.ex1_1_bias, 1, 48, 192, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[10],integer_bits_per_act_layer[11],
		integer_bits_per_layer[28],integer_bits_per_layer[29]);
	end = omp_get_wtime();
	printf("L6 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire6.ex3_3, (*model).fire6.ex3_3_bias, 3, 48, 192, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,192,
		integer_bits_per_act_layer[10],integer_bits_per_act_layer[11],
		integer_bits_per_layer[30],integer_bits_per_layer[31]);
	end = omp_get_wtime();
	printf("L6 Wall time for Ex3x3 is %2.6f\n", end-begin);

	//Fire 7
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire7.sq1_1, (*model).fire7.sq1_1_bias, 1, 384, 48, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[11],integer_bits_per_act_layer[12],
		integer_bits_per_layer[32],integer_bits_per_layer[33]);
	end = omp_get_wtime();
	printf("L7 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire7.ex1_1, (*model).fire7.ex1_1_bias, 1, 48, 192, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[12],integer_bits_per_act_layer[13],
		integer_bits_per_layer[34],integer_bits_per_layer[35]);
	end = omp_get_wtime();
	printf("L7 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire7.ex3_3, (*model).fire7.ex3_3_bias, 3, 48, 192, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,192,
		integer_bits_per_act_layer[12],integer_bits_per_act_layer[13],
		integer_bits_per_layer[36],integer_bits_per_layer[37]);
	end = omp_get_wtime();
	printf("L7 Wall time for Ex3x3 is %2.6f\n", end-begin);


	//Fire 8
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire8.sq1_1, (*model).fire8.sq1_1_bias, 1, 384, 64, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[13],integer_bits_per_act_layer[14],
		integer_bits_per_layer[38],integer_bits_per_layer[39]);
	end = omp_get_wtime();
	printf("L8 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire8.ex1_1, (*model).fire8.ex1_1_bias, 1, 64, 256, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[14],integer_bits_per_act_layer[15],
		integer_bits_per_layer[40],integer_bits_per_layer[41]);
	end = omp_get_wtime();
	printf("L8 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire8.ex3_3, (*model).fire8.ex3_3_bias, 3, 64, 256, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,256,
		integer_bits_per_act_layer[14],integer_bits_per_act_layer[15],
		integer_bits_per_layer[42],integer_bits_per_layer[43]);
	end = omp_get_wtime();
	printf("L8 Wall time for Ex3x3 is %2.6f\n", end-begin);


	//Fire 9
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire9.sq1_1, (*model).fire9.sq1_1_bias, 1, 512, 64, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[15],integer_bits_per_act_layer[16],
		integer_bits_per_layer[44],integer_bits_per_layer[45]);
	end = omp_get_wtime();
	printf("L9 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire9.ex1_1, (*model).fire9.ex1_1_bias, 1, 64, 256, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[16],integer_bits_per_act_layer[17],
		integer_bits_per_layer[46],integer_bits_per_layer[47]);
	end = omp_get_wtime();
	printf("L9 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire9.ex3_3, (*model).fire9.ex3_3_bias, 3, 64, 256, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,256,
		integer_bits_per_act_layer[16],integer_bits_per_act_layer[17],
		integer_bits_per_layer[48],integer_bits_per_layer[49]);
	end = omp_get_wtime();
	printf("L9 Wall time for Ex3x3 is %2.6f\n", end-begin);


	//Fire 10
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire10.sq1_1, (*model).fire10.sq1_1_bias, 1, 512, 96, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[17],integer_bits_per_act_layer[18],
		integer_bits_per_layer[50],integer_bits_per_layer[51]);
	end = omp_get_wtime();
	printf("L10 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire10.ex1_1, (*model).fire10.ex1_1_bias, 1, 96, 384, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[18],integer_bits_per_act_layer[19],
		integer_bits_per_layer[52],integer_bits_per_layer[53]);
	end = omp_get_wtime();
	printf("L10 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire10.ex3_3, (*model).fire10.ex3_3_bias, 3, 96, 384, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,384,
		integer_bits_per_act_layer[18],integer_bits_per_act_layer[19],
		integer_bits_per_layer[54],integer_bits_per_layer[55]);
	end = omp_get_wtime();
	printf("L10 Wall time for Ex3x3 is %2.6f\n", end-begin);


	//Fire 11
	begin = omp_get_wtime();
	perform_convolution_1x1_sq((model_t *)(*model).fire11.sq1_1, (*model).fire11.sq1_1_bias, 1, 768, 96, 0,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,1,1,0,0,
		integer_bits_per_act_layer[19],integer_bits_per_act_layer[20],
		integer_bits_per_layer[56],integer_bits_per_layer[57]);
	end = omp_get_wtime();
	printf("L11 Wall time for Sq1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_1x1_ex((model_t *)(*model).fire11.ex1_1, (*model).fire11.ex1_1_bias, 1, 96, 384, 0,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,0,
		integer_bits_per_act_layer[20],integer_bits_per_act_layer[21],
		integer_bits_per_layer[58],integer_bits_per_layer[59]);
	end = omp_get_wtime();
	printf("L11 Wall time for Ex1x1 is %2.6f\n", end-begin);
	begin = omp_get_wtime();
	perform_convolution_3x3((model_t *)(*model).fire11.ex3_3, (*model).fire11.ex3_3_bias, 3, 96, 384, 1,
		in_w, in_h, out_w, out_h, fm_buf_1, fm_buf_2,1,1,1,384,
		integer_bits_per_act_layer[20],integer_bits_per_act_layer[21],
		integer_bits_per_layer[60],integer_bits_per_layer[61]);
	end = omp_get_wtime();
	printf("L11 Wall time for Ex3x3 is %2.6f\n", end-begin);


	//ConvDet
	begin = omp_get_wtime();
	perform_convDet((model_t *)(*model).convDet, (*model).convDet_bias, 3, 768, CONVDET_CHANNELS,
		in_w, in_h, out_w, out_h, fm_buf_2, fm_buf_1,
		integer_bits_per_act_layer[21],integer_bits_per_act_layer[22],
		integer_bits_per_layer[62],integer_bits_per_layer[63]);
	end = omp_get_wtime();
	printf("L12 Wall time for ConvDet is %2.6f\n", end-begin);





	begin = omp_get_wtime();
	perform_softmax_sigmoid(out_w, out_h, fm_buf_1, integer_bits_per_act_layer[22], integer_bits_per_act_layer[23]);
	end = omp_get_wtime();
	printf("L12 Wall time for Softmax/Sigmoid is %2.6f\n", end-begin);

	}
	return out_w*out_h;
}


////////////////
////Functions
////////////////
//identical convolutions with different names in order to differentiate in tracing

// Convolution with Relu and custom filtersize(1 or 3)/ padding / stride and offset
int perform_convolution_3x3( model_t * __restrict__ conv_params,  model_t * __restrict__ bias_params, int kernel_size,
						int input_channels, int output_channels, int padding, int inp_w, int inp_h,
						int outp_w, int outp_h,  activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf, int stride, int relu,
						int is_fire_expand_layer, int outp_channel_offset, bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits){
	int i,j,k,l,p,q;
	const int half_kernelsize = (int)floorf(kernel_size/2.0);
	const int start = half_kernelsize-padding;
	const bits_t acc_frac_bits = (QUANT_BITS-inp_act_int_bits) + (QUANT_BITS-conv_int_bits);
	const bits_t bias_rightshift_bits = (QUANT_BITS-bias_int_bits)-acc_frac_bits;
	const bits_t out_rightshift_bits = acc_frac_bits - (QUANT_BITS - outp_act_int_bits);

	//#pragma omp for private(i,j,k,l,p,q) collapse(2) schedule(dynamic)
	for(i=start;i<outp_w+start;i++){
		for(j=start;j<outp_h+start;j++){
			int x0,y0;
			int outp_arr_idx = ((j-start)*outp_w+(i-start))*output_channels*(1+is_fire_expand_layer)+outp_channel_offset;
			int weight_arr_idx = 0;
			if(stride == 1){
				x0=i;
				y0=j;
			}
			else if (stride == 2){
				x0=2*i-1;
				y0=2*j-1;
			}
			for(q=0;q<output_channels;q++){
				accum_t res=0;
				for(k=-half_kernelsize;k<=half_kernelsize;k++){
					for(l=-half_kernelsize;l<=half_kernelsize;l++){
						int x,y;
						x=x0+k;
						y=y0+l;
						for(p=0;p<input_channels;p++){
							if(x>=0 && x<inp_w && y>=0 && y<inp_h)
							{
								mult_t mult = conv_params[weight_arr_idx] * inp_buf[y*input_channels*inp_w+x*input_channels+p];
								res += mult;
							}
							weight_arr_idx++;
						}
					}
				}
				if(bias_rightshift_bits>0)
					res += bias_params[q]>>bias_rightshift_bits;
				else
					res += ((accum_t)bias_params[q])<<(-bias_rightshift_bits);

				activation_t quant_res =0;
				res += 1<<(out_rightshift_bits-1); //add 1 bit for rounding (instead of truncating)
				quant_res = (activation_t)(res>>out_rightshift_bits);

				if(res>0 || relu == 0)
					outp_buf[outp_arr_idx] = quant_res;
				else
					outp_buf[outp_arr_idx] = 0;
				outp_arr_idx++;
			}
		}
	}
	return 0;
}

int perform_convolution_1x1_sq( model_t * __restrict__ conv_params,  model_t * __restrict__ bias_params, int kernel_size,
						int input_channels, int output_channels, int padding, int inp_w, int inp_h, 
						int outp_w, int outp_h,  activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf, int stride, int relu,
						int is_fire_expand_layer, int outp_channel_offset, bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits){
	int i,j,k,l,p,q;
	const int half_kernelsize = (int)floorf(kernel_size/2.0);
	const int start = half_kernelsize-padding;
	const bits_t acc_frac_bits = (QUANT_BITS-inp_act_int_bits) + (QUANT_BITS-conv_int_bits);
	const bits_t bias_rightshift_bits = (QUANT_BITS-bias_int_bits)-acc_frac_bits;
	const bits_t out_rightshift_bits = acc_frac_bits - (QUANT_BITS - outp_act_int_bits);

	//#pragma omp for private(i,j,k,l,p,q) collapse(2) schedule(dynamic)
	for(i=start;i<outp_w+start;i++){
		for(j=start;j<outp_h+start;j++){
			int x0,y0;
			int outp_arr_idx = ((j-start)*outp_w+(i-start))*output_channels*(1+is_fire_expand_layer)+outp_channel_offset;
			int weight_arr_idx = 0;
			if(stride == 1){
				x0=i;
				y0=j;
			}
			else if (stride == 2){
				x0=2*i-1;
				y0=2*j-1;
			}
			for(q=0;q<output_channels;q++){
				accum_t res=0;
				for(k=-half_kernelsize;k<=half_kernelsize;k++){
					for(l=-half_kernelsize;l<=half_kernelsize;l++){
						int x,y;
						x=x0+k;
						y=y0+l;
						for(p=0;p<input_channels;p++){
							if(x>=0 && x<inp_w && y>=0 && y<inp_h)
							{
								mult_t mult = conv_params[weight_arr_idx] * inp_buf[y*input_channels*inp_w+x*input_channels+p];
								res += mult;
							}
							weight_arr_idx++;
						}
					}
				}
				if(bias_rightshift_bits>0)
					res += bias_params[q]>>bias_rightshift_bits;
				else
					res += ((accum_t)bias_params[q])<<(-bias_rightshift_bits);

				activation_t quant_res =0;
					res += 1<<(out_rightshift_bits-1); //add 1 bit for rounding (instead of truncating)
					quant_res = (activation_t)(res>>out_rightshift_bits);


				if(res>0 || relu == 0)
					outp_buf[outp_arr_idx] = quant_res;
				else
					outp_buf[outp_arr_idx] = 0;
				outp_arr_idx++;
			}
		}
	}
	return 0;
}

int perform_convolution_1x1_ex( model_t * __restrict__ conv_params,  model_t * __restrict__ bias_params, int kernel_size,
						int input_channels, int output_channels, int padding, int inp_w, int inp_h,
						int outp_w, int outp_h,  activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf, int stride, int relu,
						int is_fire_expand_layer, int outp_channel_offset, bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits){
	int i,j,k,l,p,q;
	const int half_kernelsize = (int)floorf(kernel_size/2.0);
	const int start = half_kernelsize-padding;
	const bits_t acc_frac_bits = (QUANT_BITS-inp_act_int_bits) + (QUANT_BITS-conv_int_bits);
	const bits_t bias_rightshift_bits = (QUANT_BITS-bias_int_bits)-acc_frac_bits;
	const bits_t out_rightshift_bits = acc_frac_bits - (QUANT_BITS - outp_act_int_bits);

	//#pragma omp for private(i,j,k,l,p,q) collapse(2) schedule(dynamic)
	for(i=start;i<outp_w+start;i++){
		for(j=start;j<outp_h+start;j++){
			int x0,y0;
			int outp_arr_idx = ((j-start)*outp_w+(i-start))*output_channels*(1+is_fire_expand_layer)+outp_channel_offset;
			int weight_arr_idx = 0;
			if(stride == 1){
				x0=i;
				y0=j;
			}
			else if (stride == 2){
				x0=2*i-1;
				y0=2*j-1;
			}
			for(q=0;q<output_channels;q++){
				accum_t res=0;
				for(k=-half_kernelsize;k<=half_kernelsize;k++){
					for(l=-half_kernelsize;l<=half_kernelsize;l++){
						int x,y;
						x=x0+k;
						y=y0+l;
						for(p=0;p<input_channels;p++){
							if(x>=0 && x<inp_w && y>=0 && y<inp_h)
							{
								mult_t mult = conv_params[weight_arr_idx] * inp_buf[y*input_channels*inp_w+x*input_channels+p];
								res += mult;
							}
							weight_arr_idx++;
						}
					}
				}
				if(bias_rightshift_bits>0)
					res += bias_params[q]>>bias_rightshift_bits;
				else
					res += ((accum_t)bias_params[q])<<(-bias_rightshift_bits);

				activation_t quant_res =0;
				res += 1<<(out_rightshift_bits-1); //add 1 bit for rounding (instead of truncating)
				quant_res = (activation_t)(res>>out_rightshift_bits);


				if(res>0 || relu == 0)
					outp_buf[outp_arr_idx] = quant_res;
				else
					outp_buf[outp_arr_idx] = 0;
				outp_arr_idx++;
			}
		}
	}
	return 0;
}

// Convolution with filtersize 3, padding and stride 1 and float output
int perform_convDet( model_t * __restrict__ conv_params,  model_t * __restrict__ bias_params, int kernel_size,
						int input_channels, int output_channels, int inp_w, int inp_h,
						int outp_w, int outp_h,  activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf,
						bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits){
	int i,j,k,l,p,q;
	const int half_kernelsize = 1;
	const bits_t acc_frac_bits = (QUANT_BITS-inp_act_int_bits) + (QUANT_BITS-conv_int_bits);
	const bits_t bias_rightshift_bits = (QUANT_BITS-bias_int_bits)-acc_frac_bits;
	const bits_t out_rightshift_bits = acc_frac_bits - (QUANT_BITS - outp_act_int_bits);
	//printf("bias rightshift bits is %d and outact rightshift bits is %d \n",bias_rightshift_bits,out_rightshift_bits);

	//#pragma omp for private(i,j,k,l,p,q) collapse(2) schedule(dynamic)
	for(i=0;i<outp_w;i++){
		for(j=0;j<outp_h;j++){
			int outp_arr_idx = (j*outp_w+i)*output_channels;
			int weight_arr_idx = 0;
			for(q=0;q<output_channels;q++){
				accum_t res=0;
				for(k=-half_kernelsize;k<=half_kernelsize;k++){
					for(l=-half_kernelsize;l<=half_kernelsize;l++){
						int x,y;
						x=i+k;
						y=j+l;
						for(p=0;p<input_channels;p++){
							if(x>=0 && x<inp_w && y>=0 && y<inp_h)
							{
								//mult_t mult = conv_params[((q*kernel_size+(k+half_kernelsize))*kernel_size+ (l+half_kernelsize))*input_channels+p] * inp_buf[y*input_channels*inp_w+x*input_channels+p];
								mult_t mult = conv_params[weight_arr_idx] * inp_buf[y*input_channels*inp_w+x*input_channels+p];
								res += mult;
							}
							weight_arr_idx++;
						}
					}
				}
				if(bias_rightshift_bits>0)
					res += bias_params[q]>>bias_rightshift_bits;
				else
					res += ((accum_t)bias_params[q])<<(-bias_rightshift_bits);


				((finish_t *)outp_buf)[outp_arr_idx] = ((finish_t)res) / (pow(2,(float)(QUANT_BITS-outp_act_int_bits+out_rightshift_bits)));
				outp_arr_idx++;
			}
		}
	}
	return 0;
}

//Max-Pooling without padding: size 3, stride 2
int pooling(int inp_w, int inp_h, int outp_w, int outp_h, int channels, activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf){
	int j,i,q,m;

	//#pragma omp for private(j,i,q,m) collapse(2) schedule(dynamic)
	for(j=0;j<outp_h;j++){
		for(i=0;i<outp_w;i++){
			for(q=0;q<channels;q++){
				activation_t max_values[3];
				activation_t temp;
				for(m=0;m<3;m++){

					if((2*j+m>inp_h)){
						max_values[m]=-127;
						continue;
					}
					temp = MAX(inp_buf[((2*j+m)*inp_w+2*i)*channels+q],inp_buf[((2*j+m)*inp_w+2*i+1)*channels+q]);
					if(2*i+2<inp_w)
						max_values[m] = MAX(temp,inp_buf[((2*j+m)*inp_w+2*i+2)*channels+q]);
					else
						max_values[m] = temp;
				}
				outp_buf[(j*outp_w+i)*channels+q] = MAX(max_values[0],MAX(max_values[1],max_values[2]));
			}
		}
	}
	return 0;
}


int perform_softmax_sigmoid(int inp_w, int inp_h, activation_t * __restrict__ fm_buf,/* float * int_data, */ bits_t int_bits_convdet, bits_t softmax_bits){
	//Sigmoid only on NUM_ANCHOR of objectness scores (confidence)
	//Softmax only over NUM_ANCHOR * NUM_CLASSES  from channels and each only over NUM_CLASSES
	//including multiplication with sigmoid of objectness(confidence)
	int pixels, anchors,classes;
	//#pragma omp for private(pixels, anchors, classes) schedule(dynamic)
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
