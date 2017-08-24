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

#ifdef WITH_V4L2
#include <linux/xilinx-hls.h>
#include <linux/xilinx-v4l2-controls.h>
#include <sys/ioctl.h>
#endif

#include "filter.h"
#include "filter2d_int.h"
#include "video_int.h"

static struct filter_ops ops = {
	.init = filter2d_init,
	.func = filter2d_func
};

const static struct filter_s FS = {
	"2D Filter",
	"xlnx,v-hls-filter2d",
	"",
	NULL,
	-1,
	FILTER_MODE_OFF,
	&ops
};

struct filter_s *filter2d_create()
{
	struct filter_s *fs = (struct filter_s *) (malloc(sizeof *fs));
	*fs = FS;

	return fs;
}

const coeff_t coeff_blur = {
	{ 1, 1, 1},
	{ 1,-7, 1},
	{ 1, 1, 1}
};

const coeff_t coeff_edge = {
	{ 0, 1, 0},
	{ 1,-4, 1},
	{ 0, 1, 0}
};

const coeff_t coeff_edge_h = {
	{ 0,-1, 0},
	{ 0, 2, 0},
	{ 0,-1, 0}
};

const coeff_t coeff_edge_v = {
	{ 0, 0, 0},
	{-1, 2,-1},
	{ 0, 0, 0}
};

const coeff_t coeff_emboss = {
	{-2,-1, 0},
	{-1, 1, 1},
	{ 0, 1, 2}
};

const coeff_t coeff_gradient_h = {
	{-1,-1,-1},
	{ 0, 0, 0},
	{ 1, 1, 1}
};

const coeff_t coeff_gradient_v = {
	{-1, 0, 1},
	{-1, 0, 1},
	{-1, 0, 1}
};

const coeff_t coeff_identity = {
	{ 0, 0, 0},
	{ 0, 1, 0},
	{ 0, 0, 0}
};

const coeff_t coeff_sharpen = {
	{ 0,-1, 0},
	{-1, 5,-1},
	{ 0,-1, 0}
};

const coeff_t coeff_sobel_h = {
	{ 1, 2, 1},
	{ 0, 0, 0},
	{-1,-2,-1}
};

const coeff_t coeff_sobel_v = {
	{ 1, 0,-1},
	{ 2, 0,-2},
	{ 1, 0,-1}
};

/* store current coefficients */
coeff_t coeff_cur;

static struct {
	filter2d_preset preset;
	const char *name;
	const coeff_t *coeff;
} filter2d_presets[] = {
	{ FILTER2D_PRESET_BLUR, "Blur", &coeff_blur },
	{ FILTER2D_PRESET_EDGE, "Edge", &coeff_edge },
	{ FILTER2D_PRESET_EDGE_H, "Edge Horizontal", &coeff_edge_h },
	{ FILTER2D_PRESET_EDGE_V, "Edge Vertical", &coeff_edge_v },
	{ FILTER2D_PRESET_EMBOSS, "Emboss", &coeff_emboss },
	{ FILTER2D_PRESET_GRADIENT_H, "Gradient Horizontal", &coeff_gradient_h },
	{ FILTER2D_PRESET_GRADIENT_V, "Gradient Vertical", &coeff_gradient_v },
	{ FILTER2D_PRESET_IDENTITY, "Identity", &coeff_identity },
	{ FILTER2D_PRESET_SHARPEN, "Sharpen", &coeff_sharpen },
	{ FILTER2D_PRESET_SOBEL_H, "Sobel Horizontal", &coeff_sobel_h },
	{ FILTER2D_PRESET_SOBEL_V, "Sobel Vertical", &coeff_sobel_v }
};

const char *filter2d_get_preset_name(filter2d_preset preset)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(filter2d_presets); ++i) {
		if (filter2d_presets[i].preset == preset)
			return filter2d_presets[i].name;
	}

	return NULL;
}

void filter2d_set_coeff(struct filter_s *fs, const coeff_t coeff)
{
	unsigned int row;
	unsigned int col;

#ifdef WITH_V4L2
	struct xilinx_axi_hls_register reg[KSIZE*KSIZE];
	unsigned int off = 0;
	unsigned int i;
	int ret;

	for (row = 0; row < KSIZE; row++) {
		for (col = 0; col < KSIZE; col++) {
			i = row * KSIZE + col;
			reg[i].offset = off;
			reg[i].value = coeff[row][col];
			off += 8;
		}
	}

	struct xilinx_axi_hls_registers regs = {
		ARRAY_SIZE(reg),
		reg
	};

	/* configure hw module */
	ret = ioctl(fs->fd, XILINX_AXI_HLS_WRITE, &regs);
	ASSERT2(ret >= 0, "XILINX_AXI_HLS_WRITE failed: %s\n", ERRSTR);
#endif

	/* store new values */
	for (row = 0; row < KSIZE; row++)
		for (col = 0; col < KSIZE; col++)
			coeff_cur[row][col] = coeff[row][col];
}

coeff_t *filter2d_get_coeff(struct filter_s *fs)
{
	return &coeff_cur;
}

void filter2d_set_preset_coeff(struct filter_s *fs, filter2d_preset preset)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(filter2d_presets); ++i) {
		if (filter2d_presets[i].preset == preset)
			filter2d_set_coeff(fs, *filter2d_presets[i].coeff);
	}
}

const coeff_t *filter2d_get_preset_coeff(filter2d_preset preset)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(filter2d_presets); ++i) {
		if (filter2d_presets[i].preset == preset)
			return filter2d_presets[i].coeff;
	}

	return NULL;
}

void filter2d_init(struct filter_s *fs)
{
	filter2d_set_preset_coeff(fs, FILTER2D_PRESET_EDGE);
}

void filter2d_func(struct filter_s *fs,
		   unsigned short *frm_data_in, unsigned short *frm_data_out,
		   int height_in, int width_in, int stride_in,
		   int height_out, int width_out, int stride_out)
{
	/* The stride_in parameter passed from video_lib is expressed in Bytes.
	 * Convert to pixels for image processing function.
	 */
	int pix_stride_in = stride_in / BYTES_PER_PIXEL;

	/* filter2d requires equal input/output resolution */
	if (height_in != height_out || width_in != width_out) {
		return;
	}

	switch (fs->mode) {
#ifdef WITH_SDSOC
	case FILTER_MODE_HW:
		filter2d_sds(frm_data_in, frm_data_out, height_in, width_in,
			     pix_stride_in, coeff_cur);
		break;
#endif
	case FILTER_MODE_SW:
	default:
		filter2d_cv(frm_data_in, frm_data_out, height_in, width_in,
			    pix_stride_in, coeff_cur);
		break;
	}
}
