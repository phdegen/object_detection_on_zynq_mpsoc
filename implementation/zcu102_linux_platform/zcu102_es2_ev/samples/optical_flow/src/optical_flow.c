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

#include "filter.h"
#include "optical_flow_int.h"
#include "optflow.h"
#include "video_int.h"

static struct filter_ops ops = {
	.init = optical_flow_init,
	.func2 = optical_flow_func2
};

const static struct filter_s FS = {
	"Optical Flow",
	"",
	"",
	NULL,
	-1,
	FILTER_MODE_OFF,
	&ops
};

struct filter_s *optical_flow_create()
{
	struct filter_s *fs = (struct filter_s *) (malloc(sizeof *fs));
	*fs = FS;

	return fs;
}

void optical_flow_init(struct filter_s *fs) {}

void optical_flow_func2(struct filter_s *fs,
			unsigned short *frame_prev, unsigned short *frame_curr,
			unsigned short *frame_out,
			int height_in, int width_in, int stride_in,
			int height_out, int width_out, int stride_out)
{
	/* The stride parameter passed from video_lib is expressed in Bytes.
	 * Convert to pixels for image processing function.
	 */
	int pix_stride_in = stride_in / BYTES_PER_PIXEL;
	int pix_stride_out = stride_out / BYTES_PER_PIXEL;

	/* Check for max supported resolution */
	if (width_in > MAX_WIDTH_OF || height_in > MAX_HEIGHT_OF) {
		printf("Resolution exceeds maximum for filter 'optical_flow': %dx%d\n",
			MAX_WIDTH_OF, MAX_HEIGHT_OF);
		return;
	}

	switch (fs->mode) {
#ifdef WITH_SDSOC
	case FILTER_MODE_HW:
#endif
	case FILTER_MODE_SW:
	default:
		fpga_optflow(frame_prev, frame_curr, frame_out,
			     height_in, width_in, pix_stride_in,
			     height_out, width_out, pix_stride_out, 10.0);
		break;
	}
}
