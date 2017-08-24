#include <stdio.h>
#include "hw_convs.h"



// Convolution with Relu and custom filtersize(1 or 3)/ padding / stride and offset
int perform_convolution_hw( io_model_hw_t * conv_params,  io_model_hw_t * bias_params, int kernel_size,
						int input_channels, int output_channels, int padding, int inp_w, int inp_h,
						int outp_w, int outp_h,  io_act_hw_t * inp_buf, io_act_hw_t * outp_buf, int stride, int relu,
						int is_fire_expand_layer, int outp_channel_offset, bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits){


	if(kernel_size == 3 && inp_w==outp_w && inp_h==outp_h){
		conv_hw_3x3(conv_params, bias_params, input_channels, output_channels/OUTP_HW_UNITS_3x3, inp_w, inp_h,
				outp_w, outp_h, inp_buf, outp_buf, outp_channel_offset, (bits_hw_t)inp_act_int_bits, (bits_hw_t)outp_act_int_bits,
				(bits_hw_t)conv_int_bits, (bits_hw_t)bias_int_bits);


	}
	else
		printf("ERROR: kernel size or input/output size is not matching");
		return -1;
	return 0;
}

int conv_hw_1x1( io_model_hw_t conv_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1*MAX_INP_CHANNELS*2/8],
						io_model_hw_t bias_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						int input_channels, int output_channels_per_hw_unit, int inp_w, int inp_h,
						int outp_w, int outp_h, io_act_hw_t inp_buf1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t outp_buf[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8], int concat_nr,
						bits_hw_t inp_act_int_bits, bits_hw_t outp_act_int_bits, bits_hw_t conv_int_bits, bits_hw_t bias_int_bits){

	int new_h1,new_h2, offset_inp, offset_outp;
	new_h1=inp_h/2;
	new_h2=inp_h-new_h1;
	offset_inp = new_h1*inp_w*input_channels/8;
	offset_outp = new_h1*inp_w*output_channels_per_hw_unit*OUTP_HW_UNITS_1x1/8;
	#pragma SDS async(1)
	conv_hw_1x1_double(conv_params, bias_params, input_channels, output_channels_per_hw_unit,
			inp_w, new_h1, new_h2, outp_w, new_h1, new_h2, inp_buf1, inp_buf1+offset_inp, inp_buf2, inp_buf2+offset_inp,
			outp_buf, outp_buf+offset_outp,concat_nr, inp_act_int_bits, outp_act_int_bits,
			conv_int_bits , bias_int_bits);



	return 0;
}


// Convolution with Relu and filtersize 3, padding 1 , stride 1 and offset
void conv_hw_1x1_double( io_model_hw_t conv_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1*MAX_INP_CHANNELS*2/8],
						io_model_hw_t bias_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						int input_channels, int output_channels_per_hw_unit, int inp_w, int inp1_h, int inp2_h,
						int outp_w, int outp1_h, int outp2_h, io_act_hw_t inp_buf1_1[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf1_2[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf2_1[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t inp_buf2_2[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t outp_buf_1[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
						io_act_hw_t outp_buf_2[MAX_INP_W/2*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8], int concat_nr,
						bits_hw_t inp_act_int_bits, bits_hw_t outp_act_int_bits, bits_hw_t conv_int_bits, bits_hw_t bias_int_bits){


	#pragma HLS data_pack variable=conv_params
	#pragma HLS data_pack variable=bias_params
	#pragma HLS data_pack variable=inp_buf1_1
	#pragma HLS data_pack variable=inp_buf2_1
	#pragma HLS data_pack variable=outp_buf_1
	#pragma HLS data_pack variable=inp_buf1_2
	#pragma HLS data_pack variable=inp_buf2_2
	#pragma HLS data_pack variable=outp_buf_2

	const bits_hw_t acc_frac_bits = (QUANT_BITS-inp_act_int_bits) + (QUANT_BITS-conv_int_bits);
	//const bits_hw_t bias_rightshift_bits = (QUANT_BITS-bias_int_bits)-acc_frac_bits;
	const bits_hw_t bias_rightshift_bits = outp_act_int_bits-bias_int_bits;
	const bits_hw_t out_rightshift_bits = acc_frac_bits - (QUANT_BITS - outp_act_int_bits);

	static model_hw_t filter_conv[OUTP_HW_UNITS_1x1][MAX_IO_CHANNELS_PER_OHW_UNIT] = { { 0 } };
	//#pragma HLS RESOURCE variable=output_buff core=RAM_2P_BRAM
	#pragma HLS ARRAY_PARTITION variable=filter_conv complete dim=1
	//#pragma HLS ARRAY_PARTITION variable=filter_conv complete dim=3
	#pragma HLS ARRAY_PARTITION variable=filter_conv cyclic factor=8 dim=2


	static model_hw_t filter_bias[OUTP_HW_UNITS_1x1][MAX_OUT_CHANNELS_PER_UNIT_1x1] = { { 0 } };
	//#pragma HLS RESOURCE variable=output_buff core=RAM_2P_BRAM
	#pragma HLS ARRAY_PARTITION variable=filter_bias complete dim=1

	init_filters_1x1(conv_params, bias_params, concat_nr, filter_conv, filter_bias, output_channels_per_hw_unit, input_channels);

	perf_conv_1x1_1(filter_conv, filter_bias, inp_buf1_1, inp_buf2_1, outp_buf_1,  inp_buf1_2, inp_buf2_2, outp_buf_2,
				inp_w, inp1_h, inp2_h, input_channels, output_channels_per_hw_unit,
				bias_rightshift_bits, out_rightshift_bits,concat_nr);


}


int init_filters_1x1(io_model_hw_t conv_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1*MAX_INP_CHANNELS*2/8],
		io_model_hw_t bias_params[OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8], int concat,
		model_hw_t filter_conv[OUTP_HW_UNITS_1x1][MAX_IO_CHANNELS_PER_OHW_UNIT],
		model_hw_t filter_bias[OUTP_HW_UNITS_1x1][MAX_OUT_CHANNELS_PER_UNIT_1x1], int output_channels_per_hw_unit, int input_channels){
	#pragma HLS INLINE

	int cnt1_inp=0;
	int cnt1_dim2=0;
	int cnt2_inp=0;
	int cnt2_dim2=0;
	int a,b,c,e,f,g;
	int countpos=0;
	int inpchachtel=input_channels/8;

	filter_init_conv:
	for(a=0;a<output_channels_per_hw_unit;a++){
		#pragma HLS LOOP_TRIPCOUNT min=1  max=48 avg=3
		for(b=0;b<OUTP_HW_UNITS_1x1;b++){
			cnt1_dim2=(concat+1)*a*inpchachtel;
			for(g=0;g<=concat;g++){
				#pragma HLS LOOP_TRIPCOUNT min=1  max=2 avg=1
				for(e=0;e<input_channels;e+=8){
					#pragma HLS LOOP_TRIPCOUNT min=2  max=12 avg=8
					#pragma HLS PIPELINE II=1 REWIND

					io_model_hw_t temp = conv_params[cnt1_inp++];
					for(f=0;f<8;f++){
						#pragma HLS UNROLL
						#pragma HLS dependence variable=filter_conv inter false
						//#pragma HLS dependence variable=filter_conv intra false
						filter_conv[b][cnt1_dim2*8+e+f] = temp.data[f];
						//filter_conv[b][cnt1_dim2+e+f] = countpos++;
						//if(input_channels==32){
						//		printf("filter_conv is %d \t",filter_conv[b][cnt1_dim2*8+e+f]);
						//		printf("filter_pos is [%d][%d] \n",b ,cnt1_dim2*8+e+f);
						//}
					}
				}
				cnt1_dim2+=inpchachtel;
			}
			//cnt1_dim2=(concat+1)*a*input_channels;
		}
	}


	filter_init_bias:
	for(a=0;a<output_channels_per_hw_unit;a++){
		#pragma HLS LOOP_TRIPCOUNT min=1  max=6 avg=3
		for(b=0;b<OUTP_HW_UNITS_1x1;b+=8){
			#pragma HLS PIPELINE II=1 REWIND
			io_model_hw_t temp = bias_params[cnt2_inp++];
			for(c=0;c<8;c++){
				#pragma HLS UNROLL
				//filter_bias[b+c][cnt2_dim2]= temp.data[c];
				filter_bias[b+c][a]= temp.data[c];
			}
		}
		//cnt2_dim2++;
	}
	return 0;
}


int perf_conv_1x1_1(model_hw_t filter_conv[OUTP_HW_UNITS_1x1][MAX_IO_CHANNELS_PER_OHW_UNIT],
		model_hw_t filter_bias[OUTP_HW_UNITS_1x1][MAX_OUT_CHANNELS_PER_UNIT_1x1],
		io_act_hw_t inp_buf1_1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t inp_buf2_1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t outp_buf_1[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t inp_buf1_2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t inp_buf2_2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		io_act_hw_t outp_buf_2[MAX_INP_W*MAX_INP_H*OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8],
		int inout_w, int inout1_h, int inout2_h, int input_channels, int output_channels_per_hw_unit,
		bits_hw_t bias_rightshift_bits, bits_hw_t out_rightshift_bits, int concat){
	#pragma HLS INLINE


	int a,b,c,e,f,h,i,j,k,l,m,n,o;
	int inp_pos1[2]={ 0 };
	int inp_pos2[2]={ 0 };
	int filter_pos=0;
	int out_pos1=0;
	int out_pos2=0;

	static model_hw_t fil[OUTP_HW_UNITS_1x1][PARALLEL_INPUT_CHANNEL_UNITS_1x1] = { { 0 } };
	#pragma HLS ARRAY_PARTITION variable=fil complete dim=0

	static model_hw_t bias[OUTP_HW_UNITS_1x1] = { 0 };
	#pragma HLS ARRAY_PARTITION variable=bias complete

	static activation_hw_t inp[2][8][OUTP_HW_UNITS_1x1*MAX_OUT_CHANNELS_PER_UNIT_1x1/8][2] = { { { {0} } } };
	#pragma HLS ARRAY_PARTITION variable=inp complete dim=2
	#pragma HLS ARRAY_PARTITION variable=inp complete dim=4

	static accum_hw_t p[OUTP_HW_UNITS_1x1][PARALLEL_INPUT_CHANNEL_UNITS_1x1][2] = { { {0} } };
	#pragma HLS ARRAY_PARTITION variable=p complete dim=0

	static io_act_hw_t temp_out[OUTPUT_PKGS][2];
	#pragma HLS ARRAY_PARTITION variable=temp_out complete dim=0

	/*static accum_hw_t interm_res[OUTP_HW_UNITS_1x1][MAX_OUT_CHANNELS_PER_UNIT_1x1] = { 0 };
	#pragma HLS ARRAY_PARTITION variable=p complete dim=0*/

	convolution:

	for(a=0;a<inout2_h;a++){
		#pragma HLS LOOP_TRIPCOUNT min=22  max=93 avg=45
		for(b=0;b<inout_w;b++){
			#pragma HLS LOOP_TRIPCOUNT min=76  max=309 avg=150
			for(c=0;c<output_channels_per_hw_unit;c++){
				#pragma HLS LOOP_TRIPCOUNT min=1  max=48 avg=10
				for(e=0;e<=concat;e++){
					#pragma HLS LOOP_TRIPCOUNT min=1  max=2 avg=1
					for(f=0;f<input_channels;f+=PARALLEL_INPUT_CHANNEL_UNITS_1x1){
						#pragma HLS LOOP_TRIPCOUNT min=8  max=384 avg=196
						#pragma HLS PIPELINE II=1 REWIND
						io_act_hw_t temp_in1,temp_in2;
						if(c==0 && f%8==0){
							if(e==0){
								if (a<inout1_h)
									temp_in1=inp_buf1_1[inp_pos1[e]++];

								temp_in2=inp_buf1_2[inp_pos2[e]++];
							}
							else{
								if (a<inout1_h)
									temp_in1=inp_buf2_1[inp_pos1[e]++];

								temp_in2=inp_buf2_2[inp_pos2[e]++];
							}
							/*if(a==0 && b==1){
								printf("f and e are %d and %d \n",f, e);
								printf("temp_in is %d, %d, %d,%d,%d,%d,%d ,%d\n",temp_in.data[0],temp_in.data[1],temp_in.data[2],temp_in.data[3],temp_in.data[4],temp_in.data[5],temp_in.data[6],temp_in.data[7]);
							}*/
							for(l=0;l<8;l++){
								#pragma HLS UNROLL
								#pragma HLS dependence variable=inp inter false
								inp[e][l][f/8][0]=temp_in1.data[l];
								inp[e][l][f/8][1]=temp_in2.data[l];
							}
						}
						for(i=0;i<OUTP_HW_UNITS_1x1;i++){
							#pragma HLS UNROLL
							for(k=0;k<PARALLEL_INPUT_CHANNEL_UNITS_1x1;k++){
								#pragma HLS UNROLL
								fil[i][k]=filter_conv[i][filter_pos+k];
								/*if(a==0 && b==20 && c==2 && i==0 && input_channels<17){
									printf("filter_conv is %d \t",filter_conv[i][filter_pos+k]);
									printf("pos is [%d][%d] \n",i,filter_pos+k);
								}*/
							}
						}
						for(j=0;j<OUTP_HW_UNITS_1x1;j++){
							#pragma HLS UNROLL
							bias[j] = filter_bias[j][c];
						}

						for(h=0;h<OUTP_HW_UNITS_1x1;h++){
							#pragma HLS UNROLL
							for(n=0;n<PARALLEL_INPUT_CHANNEL_UNITS_1x1;n++){
								#pragma HLS UNROLL

								mult_hw_t mult1=fil[h][n]*inp[e][f%8+n][f/8][0];
								mult_hw_t mult2=fil[h][n]*inp[e][f%8+n][f/8][1];
								if(f==0 && e == 0){
									p[h][n][0] = 0 + mult1;
									p[h][n][1] = 0 + mult2;
								}
								else{
									p[h][n][0] += mult1;
									p[h][n][1] += mult2;

								}

								/*if(a==0 && b== && c==0 && h==15 && input_channels<65 && input_channels>33){
									//printf("f and n are : %d %d \n",f,n);
									printf("filterw*inp is %d * %d \t ",fil[h][n],inp[e][f%8+n][f/8]);
									printf("mult is %d and the sum is %d\n", mult, p[h][n]);
								}*/
							}

							if(e==concat && f==input_channels-PARALLEL_INPUT_CHANNEL_UNITS_1x1){
								accum_hw_t out1=0;
								accum_hw_t out2=0;
								for(m=0;m<PARALLEL_INPUT_CHANNEL_UNITS_1x1;m++){
									#pragma HLS UNROLL
									out1+=p[h][m][0];
									out2+=p[h][m][1];
								}

								/*if(a==0 && b==50 && c==0 && h==16 && input_channels<65 && input_channels>33){
									printf("out = %d\n",out);
								}*/
								activation_hw_t quant_res1, quant_res2;
								//out += 1<<(out_rightshift_bits-1); //add 1 bit for rounding (instead of truncating)
								quant_res1 = (activation_hw_t)(((out1>>(out_rightshift_bits-1))+1)>>1);
								quant_res2 = (activation_hw_t)(((out2>>(out_rightshift_bits-1))+1)>>1);

								/*if(a==0 && b==4 && c==0 && h==0){
									printf("out_rightshift_bits = %d\n",out_rightshift_bits);
									printf("(out>>(out_rightshift_bits-1)) = %d\n",(out>>(out_rightshift_bits-1)));
									printf("bias  = %d\n",bias[h]);
									printf("bias_rightshift_bits  = %d\n",bias_rightshift_bits);
								}*/

								if(bias_rightshift_bits>=0){
									quant_res1 += bias[h]>>bias_rightshift_bits;
								}
								else{
									quant_res2 -= (-bias[h])>>bias_rightshift_bits;
								}

								/*if(a==0 && b==50 && c==0 && h==16 && input_channels<65 && input_channels>33){
									printf("quant_res = %d\n",quant_res);
								}*/

								if (quant_res1>=0)
									temp_out[h/8][0].data[h%8]=quant_res1;
								else
									temp_out[h/8][0].data[h%8]=0;

								if (quant_res2>=0)
									temp_out[h/8][1].data[h%8]=quant_res2;
								else
									temp_out[h/8][1].data[h%8]=0;
								//output_buff[h][c][a][b][l]=(activation_hw_t)(out>>out_rightshift_bits);

							}
						}
						filter_pos+=PARALLEL_INPUT_CHANNEL_UNITS_1x1;
					}
				}
				for(o=0;o<OUTPUT_PKGS;o++){
					#pragma HLS UNROLL
					if (a<inout1_h)
						outp_buf_1[out_pos1++]=temp_out[o][0];

					outp_buf_2[out_pos2++]=temp_out[o][1];
				}
			}
			filter_pos = 0;
		}
	}


	return 0;
}




// Convolution with Relu and filtersize 3, padding 1 , stride 1 and offset
void conv_hw_3x3( io_model_hw_t conv_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3*9*MAX_INP_CHANNELS/8],
						io_model_hw_t bias_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3/8],
						int input_channels, int output_channels_per_hw_unit, int inp_w, int inp_h,
						int outp_w, int outp_h, io_act_hw_t inp_buf[MAX_INP_W*MAX_INP_H*MAX_INP_CHANNELS/8],
						io_act_hw_t outp_buf[MAX_INP_W*MAX_INP_H*MAX_OUT_CHANNELS_PER_UNIT_3x3*OUTP_HW_UNITS_3x3/8], char relu_off_and_16bit_out,
						bits_hw_t inp_act_int_bits, bits_hw_t outp_act_int_bits, bits_hw_t conv_int_bits, bits_hw_t bias_int_bits){


	#pragma HLS data_pack variable=conv_params
	#pragma HLS data_pack variable=bias_params
	#pragma HLS data_pack variable=inp_buf
	#pragma HLS data_pack variable=outp_buf

	const bits_hw_t acc_frac_bits = (QUANT_BITS-inp_act_int_bits) + (QUANT_BITS-conv_int_bits);
	//const bits_hw_t bias_rightshift_bits = (QUANT_BITS-bias_int_bits)-acc_frac_bits;
	//const bits_hw_t bias_rightshift_bits = (relu_off_and_16bit_out==0) ? (outp_act_int_bits-bias_int_bits) : (outp_act_int_bits-bias_int_bits-8);
	//const bits_hw_t out_rightshift_bits = (relu_off_and_16bit_out==0) ? acc_frac_bits - (QUANT_BITS - outp_act_int_bits): (acc_frac_bits - (QUANT_BITS - outp_act_int_bits)-8);
	const bits_hw_t bias_rightshift_bits =(outp_act_int_bits-bias_int_bits);
	const bits_hw_t out_rightshift_bits = (acc_frac_bits - (QUANT_BITS - outp_act_int_bits)-4);


	static activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS] = { { { 0 } } };
	//#pragma HLS RESOURCE variable=output_buff core=RAM_2P_BRAM
	//#pragma HLS ARRAY_PARTITION variable=input_buff cyclic factor=8 dim=1
	#pragma HLS ARRAY_PARTITION variable=input_buff complete dim=2
	#pragma HLS ARRAY_PARTITION variable=input_buff cyclic factor=8 dim=3

	static model_hw_t filter_conv[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3][3][3][MAX_INP_CHANNELS] = { { { { { 0 } } } } };
	//#pragma HLS RESOURCE variable=output_buff core=RAM_2P_BRAM
	#pragma HLS ARRAY_PARTITION variable=filter_conv complete dim=1
	#pragma HLS ARRAY_PARTITION variable=filter_conv cyclic factor=8 dim=5

	static model_hw_t filter_bias[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3] = { { 0 } };
	//#pragma HLS RESOURCE variable=output_buff core=RAM_2P_BRAM
	#pragma HLS ARRAY_PARTITION variable=filter_bias complete dim=1

	static activation_hw_t output_buff[OUTP_HW_UNITS_3x3][2][MAX_OUT_CHANNELS_PER_UNIT_3x3/2][RUN_DEPTH_H][MAX_INP_W] = { { { { { 0 } } } } };
	//#pragma HLS RESOURCE variable=output_buff core=RAM_2P_BRAM
	#pragma HLS ARRAY_PARTITION variable=output_buff complete dim=1
	//#pragma HLS ARRAY_PARTITION variable=output_buff complete dim=2
	#pragma HLS ARRAY_PARTITION variable=output_buff complete dim=4
	//#pragma HLS ARRAY_PARTITION variable=output_buff cyclic factor=8 dim=4

	init_filters_3x3(conv_params, bias_params, filter_conv, filter_bias, output_channels_per_hw_unit, input_channels);

	init_input_first_line(inp_buf, input_buff, inp_w, input_channels);

	int r;
	for(r=-1;r<outp_h-1;r+=RUN_DEPTH_H){
	#pragma HLS LOOP_TRIPCOUNT min=5  max=19 avg=10
		transfer_input(inp_buf, input_buff, inp_w, inp_h, input_channels, r);

		perf_conv_3x3(filter_conv, filter_bias, input_buff, output_buff, inp_w, inp_h, input_channels, output_channels_per_hw_unit,
				bias_rightshift_bits, out_rightshift_bits,r,relu_off_and_16bit_out);


		transfer_output(outp_buf, output_buff, outp_w, outp_h, output_channels_per_hw_unit, r, relu_off_and_16bit_out);

	}

}


int init_filters_3x3(io_model_hw_t conv_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3*9*MAX_INP_CHANNELS/8],
		io_model_hw_t bias_params[OUTP_HW_UNITS_3x3*MAX_OUT_CHANNELS_PER_UNIT_3x3/8],
		model_hw_t filter_conv[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3][3][3][MAX_INP_CHANNELS],
		model_hw_t filter_bias[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3], int output_channels_per_hw_unit, int input_channels){
	#pragma HLS INLINE

	int cnt1=0;
	int cnt2=0;
	int a,b,c,d,e,f;

	filter_init_conv:
	for(a=0;a<output_channels_per_hw_unit;a++){
		#pragma HLS LOOP_TRIPCOUNT min=1  max=6 avg=3
		for(b=0;b<OUTP_HW_UNITS_3x3;b++){
			for(c=0;c<3;c++){
				for(d=0;d<3;d++){
					for(e=0;e<input_channels;e+=8){
						#pragma HLS LOOP_TRIPCOUNT min=2  max=12 avg=8
						#pragma HLS PIPELINE II=1 REWIND

						io_model_hw_t temp = conv_params[cnt1++];
						for(f=0;f<8;f++){
							#pragma HLS UNROLL
							#pragma HLS dependence variable=filter_conv inter false
							filter_conv[b][a][c][d][e+f]= temp.data[f];
							//if(input_channels==32)
							//	printf("filter_conv is %d \t pos is [%d][%d][%d][%d][%d]\n", filter_conv[b][a][c][d][e+f],b,a,c,d,e+f);
						}
					}
				}
			}
		}
	}
	filter_init_bias:
	for(a=0;a<output_channels_per_hw_unit;a++){
		#pragma HLS LOOP_TRIPCOUNT min=1  max=6 avg=3
		for(b=0;b<OUTP_HW_UNITS_3x3;b+=8){
			#pragma HLS PIPELINE II=1 REWIND
			io_model_hw_t temp = bias_params[cnt2++];
			for(c=0;c<8;c++){
				//#pragma HLS UNROLL
				filter_bias[b+c][a]= temp.data[c];
			}
		}
	}
	return 0;
}

int init_input_first_line(io_act_hw_t inp_stream[MAX_INP_W*MAX_INP_H*MAX_INP_CHANNELS/8], activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS],
		int inp_w, int inp_ch){
	#pragma HLS INLINE

	int b,c,d;
	int stream_pos = 0;

	input_init_trans:
		for(b=0;b<inp_w;b++){
			#pragma HLS LOOP_TRIPCOUNT min=76  max=309 avg=150
			for(c=0;c<inp_ch;c+=8){
				#pragma HLS LOOP_TRIPCOUNT min=2  max=12 avg=8
				#pragma HLS PIPELINE II=1 REWIND
					io_act_hw_t temp=inp_stream[stream_pos];
					for(d=0;d<8;d++){
						#pragma HLS UNROLL
						#pragma HLS dependence variable=input_buff inter false
						input_buff[b][RUN_DEPTH_H][c+d]=0;
						input_buff[b][RUN_DEPTH_H+1][c+d]=temp.data[d];
					}
				stream_pos++;
			}
	}


	return 0;
}



int transfer_input(io_act_hw_t inp_stream[MAX_INP_W*MAX_INP_H*MAX_INP_CHANNELS/8], activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS],
		int inp_w, int inp_h, int inp_ch, int start_height){
	#pragma HLS INLINE
	int a,b,c,d,e,f,g,h;
	int picture_h = start_height+2;
	int stream_pos = picture_h*inp_w*inp_ch/8;

	copy_last_two_rows:
	for(e=0;e<inp_w;e++){
		#pragma HLS LOOP_TRIPCOUNT min=76  max=309 avg=150
		for(f=0;f<inp_ch;f+=8){
			#pragma HLS LOOP_TRIPCOUNT min=2  max=12 avg=8
			#pragma HLS PIPELINE II=1 REWIND
			for(g=0;g<2;g++){
				#pragma HLS UNROLL
				for(h=0;h<8;h++){
					#pragma HLS UNROLL
					input_buff[e][g][f+h]=input_buff[e][g+RUN_DEPTH_H][f+h];

					if(start_height==-1)
					;//	printf("w is %d, inp_ch is %d g is %d\n value is %d\n",e,h,g,input_buff[e][g][f+h]);
				}

			}
		}

	}


	input_trans:
	for(a=2;a<RUN_DEPTH_H+2;a++){
		for(b=0;b<inp_w;b++){
			#pragma HLS LOOP_TRIPCOUNT min=76  max=309 avg=150
			for(c=0;c<inp_ch;c+=8){
				#pragma HLS LOOP_TRIPCOUNT min=2  max=12 avg=8
				#pragma HLS PIPELINE II=1 REWIND

				if(/*picture_h>=0 &&*/ picture_h<inp_h){
					io_act_hw_t temp=inp_stream[stream_pos++];
					for(d=0;d<8;d++){
						#pragma HLS UNROLL
						#pragma HLS dependence variable=input_buff inter false
						input_buff[b][a][c+d]=temp.data[d];
					}
				}
				else{
					for(d=0;d<8;d++){
						#pragma HLS UNROLL
						#pragma HLS dependence variable=input_buff inter false
						input_buff[b][a][c+d]=0;
					}
				}
			}
		}
		picture_h++;
	}
return 0;
}


int perf_conv_3x3(model_hw_t filter_conv[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3][3][3][MAX_INP_CHANNELS],
		model_hw_t filter_bias[OUTP_HW_UNITS_3x3][MAX_OUT_CHANNELS_PER_UNIT_3x3],
		activation_hw_t input_buff[MAX_INP_W][RUN_DEPTH_H+2][MAX_INP_CHANNELS],
		activation_hw_t  output_buff[OUTP_HW_UNITS_3x3][2][MAX_OUT_CHANNELS_PER_UNIT_3x3/2][RUN_DEPTH_H][MAX_INP_W],
		int inout_w, int inout_h, int input_channels, int output_channels_per_hw_unit,
		bits_hw_t bias_rightshift_bits, bits_hw_t out_rightshift_bits, int start_height, char relu_off_and_16bit_out){
	#pragma HLS INLINE


	int a,b,c,d,e,f,h,i,j,k,l,m,n,u,v;

	model_hw_t fil[OUTP_HW_UNITS_3x3][PARALLEL_INPUT_CHANNEL_UNITS_3x3] = { { 0 } };
	#pragma HLS ARRAY_PARTITION variable=fil complete dim=0

	model_hw_t bias[OUTP_HW_UNITS_3x3] = { 0 };
	#pragma HLS ARRAY_PARTITION variable=fil complete

	activation_hw_t inp[RUN_DEPTH_H][PARALLEL_INPUT_CHANNEL_UNITS_3x3] = { { 0 } };
	#pragma HLS ARRAY_PARTITION variable=inp complete dim=0

	static accum_hw_t p[OUTP_HW_UNITS_3x3][RUN_DEPTH_H][PARALLEL_INPUT_CHANNEL_UNITS_3x3] = { { { 0 } } };
	#pragma HLS ARRAY_PARTITION variable=p complete dim=0

	static mult_hw_t out_reg[OUTP_HW_UNITS_3x3][RUN_DEPTH_H] = { { 0 } };
	#pragma HLS ARRAY_PARTITION variable=out_reg complete dim=0

	convolution:

	for(b=0;b<inout_w;b++){
		#pragma HLS LOOP_TRIPCOUNT min=76  max=309 avg=150
		for(c=0;c<output_channels_per_hw_unit;c++){
			#pragma HLS LOOP_TRIPCOUNT min=1  max=6 avg=3
			for(f=0;f<input_channels;f+=PARALLEL_INPUT_CHANNEL_UNITS_3x3){
				//#pragma HLS LOOP_TRIPCOUNT min=16  max=96 avg=64
				#pragma HLS LOOP_TRIPCOUNT min=4  max=24 avg=12
				for(d=-HALF_KERNEL_SIZE_3x3;d<=HALF_KERNEL_SIZE_3x3;d++){
					for(e=-HALF_KERNEL_SIZE_3x3;e<=HALF_KERNEL_SIZE_3x3;e++){
						#pragma HLS PIPELINE II=1 REWIND
						#pragma HLS dependence variable=input_buff inter false

						for(i=0;i<OUTP_HW_UNITS_3x3;i++){
							#pragma HLS UNROLL
							for(k=0;k<PARALLEL_INPUT_CHANNEL_UNITS_3x3;k++){
								#pragma HLS UNROLL
								fil[i][k]=filter_conv[i][c][d+1][e+1][f+k];
								//if(b==50 && c==0 && i==32 && start_height==-1 && input_channels==32)
								//	printf("filter is %d and pos is [%d][%d][%d][%d][%d]\n",fil[i][k],i,c,d+1,e+1,f+k);
							}
						}
						for(j=0;j<OUTP_HW_UNITS_3x3;j++){
							#pragma HLS UNROLL
							bias[j] = filter_bias[j][c];
						}

						for(a=0;a<RUN_DEPTH_H;a++){
							#pragma HLS UNROLL

							int x0,y0;
							x0 = b+d;
							y0=start_height+1+a+e;

							for(l=0;l<PARALLEL_INPUT_CHANNEL_UNITS_3x3;l++){
								#pragma HLS UNROLL

								if(x0>=0 && x0<inout_w && y0>=0 && y0<inout_h){
									inp[a][l]=input_buff[x0][a+e+1][f+l];
									/*if(x0==309 && a+e+1==1 && (f+l==0  || f+l==1 || f+l==2) && start_height==-1 && d==1)
										printf("value is %d at w=%d and h is %d and inpch is %d \n",inp[a][l],x0,a+e+1,f+l);*/
								}
								else
									inp[a][l]=0;
							}


							if(y0<=inout_h){
								for(h=0;h<OUTP_HW_UNITS_3x3;h++){
									#pragma HLS UNROLL

									for(n=0;n<PARALLEL_INPUT_CHANNEL_UNITS_3x3;n++){
										#pragma HLS UNROLL

										if(a==0 && b==50 && c==0 && h==32 && start_height==-1 && input_channels==32){
											//printf("f and n are : %d %d , d and e are %d, %d    \t",f,n, d, e);
											//printf("filterw*inp is %d * %d \n ",fil[h][n],inp[a][n]);
										}

										mult_hw_t mult=fil[h][n]*inp[a][n];
										if(d==-1 && e==-1 && f==0)
											p[h][a][n] = 0 + mult;
										else
											p[h][a][n] += mult;
									}

									if(d==1 && e==1 && f==input_channels-PARALLEL_INPUT_CHANNEL_UNITS_3x3){
										accum_hw_t out=0;
										for(m=0;m<PARALLEL_INPUT_CHANNEL_UNITS_3x3;m++){
										#pragma HLS UNROLL
										#pragma HLS RESOURCE variable=out core=AddSub_DSP
											out+=p[h][a][m];
										}
										/*if(a==0 && b==2 && c==0 && h==60 && start_height==-1){
											printf("out is %d \n ",out);
										}*/

										activation_hw_t quant_res;
										short quant_res_convdet;
										out=out<<4;
										//out += 1<<(out_rightshift_bits-1); //add 1 bit for rounding (instead of truncating)
										assert(out_rightshift_bits<8);
										quant_res_convdet = (short)(((out>>(out_rightshift_bits-1))+1)>>1);
										quant_res = (activation_hw_t)(((quant_res_convdet>>7)+1)>>1);

										/*if(bias_rightshift_bits>=0)
											quant_res_convdet += bias[h]>>bias_rightshift_bits;
										else
											quant_res_convdet -= (-bias[h])>>bias_rightshift_bits;*/

										if(bias_rightshift_bits>=0)
											quant_res += bias[h]>>bias_rightshift_bits;
										else
											quant_res += bias[h]<<(-bias_rightshift_bits);

										/*if(a==6 && b==0 && c==0 && h==0 && start_height==87){
											printf("quant_res is %d \n ",quant_res);
											printf("\n\n");
										}*/


										if(relu_off_and_16bit_out==0){
											if (quant_res>=0)
												out_reg[h][a]=(mult_hw_t)quant_res;
											else
												out_reg[h][a]=0;
											//output_buff[h][c][a][b][l]=(activation_hw_t)(out>>out_rightshift_bits);
										}
										else{
											out_reg[h][a]=quant_res_convdet;
										}
									}
								}
							}
						}
					}
				}
			}
			for(u=0;u<OUTP_HW_UNITS_3x3;u++){
				#pragma HLS UNROLL
				for(v=0;v<RUN_DEPTH_H;v++){
					#pragma HLS UNROLL
					if(relu_off_and_16bit_out==0){
						output_buff[u][c%2][c/2][v][b]=(activation_hw_t)out_reg[u][v];
					}
					else{
						output_buff[(u*2)%OUTP_HW_UNITS_3x3][u/(OUTP_HW_UNITS_3x3/2)][c][v][b]=(activation_hw_t)(out_reg[u][v]%256);
						output_buff[(u*2+1)%OUTP_HW_UNITS_3x3][u/(OUTP_HW_UNITS_3x3/2)][c][v][b]=(activation_hw_t)(out_reg[u][v]>>8);
					}
				}
			}
		}
	}


	return 0;
}





int transfer_output(io_act_hw_t outp_stream[MAX_INP_W*MAX_INP_H*MAX_OUT_CHANNELS_PER_UNIT_3x3*OUTP_HW_UNITS_3x3/8],
		activation_hw_t  output_buff[OUTP_HW_UNITS_3x3][2][MAX_OUT_CHANNELS_PER_UNIT_3x3/2][RUN_DEPTH_H][MAX_INP_W],
		int outp_w, int outp_h, int out_channels_per_hw_unit, int start_height, char relu_off_and_16bit_out){
	#pragma HLS INLINE
	int a,b,c,d,e;
	int picture_h = start_height+1;
	int effective_out_channels_per_hw_unit = (relu_off_and_16bit_out==0) ? out_channels_per_hw_unit : out_channels_per_hw_unit*2;
	int new_channel_offset = 0;
	int stream_pos = picture_h*outp_w*effective_out_channels_per_hw_unit*OUTP_HW_UNITS_3x3/8+new_channel_offset;
	//int stream_pos = picture_h*outp_w*out_channels_per_hw_unit*OUTP_HW_UNITS_3x3/8*2+new_channel_offset; //factor 2 because of double oupt_channels due to concat
	output_trans:
	for(a=0;a<RUN_DEPTH_H;a++, picture_h++){
		for(b=0;b<outp_w;b++){
			#pragma HLS LOOP_TRIPCOUNT min=76  max=309 avg=150
			for(c=0;c<effective_out_channels_per_hw_unit;c++){
				#pragma HLS LOOP_TRIPCOUNT min=1  max=6 avg=3
				for(d=0;d<OUTP_HW_UNITS_3x3;d+=8){
				#pragma HLS PIPELINE II=1 REWIND
					if(picture_h<outp_h){
						io_act_hw_t temp;
						for(e=0;e<8;e++){
							#pragma HLS UNROLL
							#pragma HLS dependence variable=output_buff inter false
							#pragma HLS dependence variable=output_buff intra false
							temp.data[e]=output_buff[d+e][c%2][c/2][a][b];
							//temp.data[e]=output_buff[d+e][c%2][c/2][a][b];
						}
						if(d==0 && c==0 && a==0 && b==1){
							//printf("stream_pos is %d \n",stream_pos);
							//printf("temp.data[0] is %d \n",temp.data[0]);
						}
						outp_stream[stream_pos] = temp;
					}
					stream_pos++;

				}
			}
			stream_pos+=new_channel_offset;
		}
	}
return 0;
}






