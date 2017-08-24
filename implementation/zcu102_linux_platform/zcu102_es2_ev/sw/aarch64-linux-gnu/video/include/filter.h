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

#ifndef ___FILTER_H___
#define ___FILTER_H___

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	FILTER_MODE_OFF,
	FILTER_MODE_SW,
	FILTER_MODE_HW
} filter_mode;

struct filter_s {
	const char *display_text;
	const char *dt_comp_string;
	const char *pr_file_name;
	char *pr_buf;
	int fd;
	filter_mode mode;
	struct filter_ops *ops;
};

struct filter_tbl {
	unsigned int size;
	struct filter_s **filter_types;
};

struct filter_ops {
	void (*init)(struct filter_s *fs);
	void (*func)(struct filter_s *fs,
		     unsigned short *frm_data_in, unsigned short *frm_data_out,
		     int height_in, int width_in, int stride_in,
		     int height_out, int width_out, int stride_out);
	void (*func2)(struct filter_s *fs,
		      unsigned short *frame_prev, unsigned short *frame_curr, unsigned short *frame_out,
		      int height_in, int width_in, int stride_in,
		      int height_out, int width_out, int stride_out);
};

/* Helper functions */
int filter_type_register(struct filter_tbl *ft, struct filter_s *fs);
struct filter_s *filter_type_get_obj(struct filter_tbl *ft, unsigned int i);
int filter_type_match(struct filter_s *fs, const char *str);
void filter_type_set_mode(struct filter_s *fs, filter_mode mode);

/* Partial reconfig functions */
int filter_type_prefetch_bin(struct filter_s *fs);
int filter_type_free_bin(struct filter_s *fs);
int filter_type_config_bin(struct filter_s *fs);

#ifdef __cplusplus
}
#endif

#endif
