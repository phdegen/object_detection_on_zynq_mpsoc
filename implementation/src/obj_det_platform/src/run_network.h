/*
 * run_network.h
 *
 *  Created on: Jan 26, 2017
 *      Author: msc17f2
 */

#ifndef SRC_RUN_NETWORK_H_
#define SRC_RUN_NETWORK_H_

#include "load_model.h"

int run_network(net_model * model, activation_t * image, activation_t * fm_buf_1, activation_t * fm_buf_2);

int perform_convolution(model_t * conv_params, model_t * bias_params, int kernel_size,
						int input_channels, int output_channels, int padding, int inp_w, int inp_h,
						int outp_w, int outp_h, activation_t * inp_buf, activation_t * outp_buf, int stride, int relu,
						int is_fire_expand_layer, int outp_channel_offset, bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits);

int perform_convDet(model_t * conv_params, model_t * bias_params, int kernel_size,
						int input_channels, int output_channels, int inp_w, int inp_h,
						int outp_w, int outp_h, activation_t * inp_buf, activation_t * outp_buf,
						bits_t inp_act_int_bits,
						bits_t outp_act_int_bits, bits_t conv_int_bits, bits_t bias_int_bits);

int pooling(int inp_w, int inp_h, int outp_w, int outp_h, int channels, activation_t * inp_buf, activation_t * outp_buf, int concat);

int perform_softmax_sigmoid(int inp_w, int inp_h, activation_t * fm_buf, bits_t int_bits_convdet, bits_t softmax_bits);

int concat(int width, int height, int single_channels, activation_t * fm_buff_1, activation_t * fm_buff_2);

int split_image(activation_t * image, activation_t * fm_buf_2);

int pooling_with_stride_and_concat(int inp_w, int inp_h, int outp_w, int outp_h, int channels, activation_t * __restrict__ inp_buf, activation_t * __restrict__ outp_buf);

int deconcat(int width, int height, int single_channels, activation_t * fm_buff_1, activation_t * fm_buff_2);

int sum_intermediate_results(int width, int height, int fm_sets, int input_channels, int output_channels, model_t * bias, int outp_act_int_bits, int bias_int_bits, activation_t * fm_buff_1, activation_t * fm_buff_2);

#endif /* SRC_RUN_NETWORK_H_ */
