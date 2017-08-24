/******************************************************************************
 *
 * (c) Copyright 2012-2016 Xilinx, Inc. All rights reserved.
 *
 * This file contains confidential and proprietary information of Xilinx, Inc.
 * and is protected under U.S. and international copyright and other
 * intellectual property laws.
 *
 * DISCLAIMER
 * This disclaimer is not a license and does not grant any rights to the
 * materials distributed herewith. Except as otherwise provided in a valid
 * license issued to you by Xilinx, and to the maximum extent permitted by
 * applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL
 * FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS,
 * IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
 * MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
 * and (2) Xilinx shall not be liable (whether in contract or tort, including
 * negligence, or under any other theory of liability) for any loss or damage
 * of any kind or nature related to, arising under or in connection with these
 * materials, including for any direct, or any indirect, special, incidental,
 * or consequential loss or damage (including loss of data, profits, goodwill,
 * or any type of loss or damage suffered as a result of any action brought by
 * a third party) even if such damage or loss was reasonably foreseeable or
 * Xilinx had been advised of the possibility of the same.
 *
 * CRITICAL APPLICATIONS
 * Xilinx products are not designed or intended to be fail-safe, or for use in
 * any application requiring fail-safe performance, such as life-support or
 * safety devices or systems, Class III medical devices, nuclear facilities,
 * applications related to the deployment of airbags, or any other applications
 * that could lead to death, personal injury, or severe property or
 * environmental damage (individually and collectively, "Critical
 * Applications"). Customer assumes the sole risk and liability of any use of
 * Xilinx products in Critical Applications, subject only to applicable laws
 * and regulations governing limitations on product liability.
 *
 * THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE
 * AT ALL TIMES.
 *
 *******************************************************************************/

#ifdef WITH_SDSOC
#include <hls_video.h>

#include "filter2d_int.h"
#include "video_int.h"

using namespace hls;

#pragma SDS data mem_attribute(frm_data_in:PHYSICAL_CONTIGUOUS, frm_data_out:PHYSICAL_CONTIGUOUS)
#pragma SDS data access_pattern(frm_data_in:SEQUENTIAL, frm_data_out:SEQUENTIAL)
#pragma SDS data data_mover(frm_data_in:AXIDMA_SG, frm_data_out:AXIDMA_SG)
#pragma SDS data copy(frm_data_in[0:stride*height], frm_data_out[0:stride*height])
void filter2d_sds(unsigned short *frm_data_in, unsigned short *frm_data_out,
		  int height, int width, int stride, coeff_t coeff)
{
	Mat<MAX_HEIGHT, MAX_WIDTH, HLS_8UC2> src(height, width);
	Mat<MAX_HEIGHT, MAX_WIDTH, HLS_8UC2> dst(height, width);

	// planes
	Mat<MAX_HEIGHT, MAX_WIDTH, HLS_8UC1> img_y(height, width);
	Mat<MAX_HEIGHT, MAX_WIDTH, HLS_8UC1> img_uv(height, width);
	Mat<MAX_HEIGHT, MAX_WIDTH, HLS_8UC1> img_yf(height, width);

	// kernel
	Window<3,3,char> kernel;
	for(int i=0; i<KSIZE; i++)
		for(int j=0; j<KSIZE; j++)
			kernel.val[i][j] = coeff[i][j];

	// anchor
	Point_<int> anchor = Point_<int>( -1, -1 );

#pragma HLS dataflow
#pragma HLS stream depth=20000 variable=img_uv.data_stream

	// filter
	AXIM2Mat<MAX_STRIDE>(frm_data_in, stride, src);
	Split(src, img_y, img_uv);
	Filter2D(img_y, img_yf, kernel, anchor);
	Merge(img_yf, img_uv, dst);
	Mat2AXIM<MAX_STRIDE>(dst, frm_data_out, stride);
}
#endif
