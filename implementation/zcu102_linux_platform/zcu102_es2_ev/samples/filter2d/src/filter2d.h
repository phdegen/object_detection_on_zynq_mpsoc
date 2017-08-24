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

#ifndef _FILTER2D_H_
#define _FILTER2D_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Kernel size */
#define KSIZE 3

/* Filter presets */
#define FILTER2D_PRESET_CNT 11

/* Filter presets */
typedef enum
{
	FILTER2D_PRESET_BLUR,
	FILTER2D_PRESET_EDGE,
	FILTER2D_PRESET_EDGE_H,
	FILTER2D_PRESET_EDGE_V,
	FILTER2D_PRESET_EMBOSS,
	FILTER2D_PRESET_GRADIENT_H,
	FILTER2D_PRESET_GRADIENT_V,
	FILTER2D_PRESET_IDENTITY,
	FILTER2D_PRESET_SHARPEN,
	FILTER2D_PRESET_SOBEL_H,
	FILTER2D_PRESET_SOBEL_V
} filter2d_preset;

typedef int coeff_t[KSIZE][KSIZE];

/* 2D filter functions */
struct filter_s *filter2d_create();
const char *filter2d_get_preset_name(filter2d_preset preset);
void filter2d_set_coeff(struct filter_s *fs, const coeff_t coeff);
coeff_t *filter2d_get_coeff(struct filter_s *fs);
void filter2d_set_preset_coeff(struct filter_s *fs, filter2d_preset preset);
const coeff_t *filter2d_get_preset_coeff(filter2d_preset preset);

#ifdef __cplusplus
}
#endif

#endif
