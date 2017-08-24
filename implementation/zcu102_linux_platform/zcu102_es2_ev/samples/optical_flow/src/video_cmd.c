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

/*****************************************************************************
*
* @file video_cmd.c
*
* This file implements command line interface for Zynq Base TRD .
* 
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00a RSP   14/07/14 Initial release
* </pre>
*
* @note
*
******************************************************************************/


#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <stdlib.h>

#include "video.h"
#include "video_int.h"

#if defined (SAMPLE_FILTER2D)
#include "filter2d.h"

static void filter_init(struct filter_tbl *ft)
{
	int ret;
	struct filter_s *fs;

	fs = filter2d_create();
	ret = filter_type_register(ft, fs);
	if (ret < 0)
		printf("Failed to register filter %s\n", fs->display_text);
}
#elif defined (SAMPLE_OPTICAL_FLOW)
#include "optical_flow.h"

static void filter_init(struct filter_tbl *ft)
{
	int ret;
	struct filter_s *fs;

	fs = optical_flow_create();
	ret = filter_type_register(ft, fs);
	if (ret < 0)
		printf("Failed to register filter %s\n", fs->display_text);
}
#elif defined (SAMPLE_STEREO_VISION)
#include "stereo_vision.h"

static void filter_init(struct filter_tbl *ft)
{
	int ret;
	struct filter_s *fs;

	fs = stereo_vision_create();
	ret = filter_type_register(ft, fs);
	if (ret < 0)
		printf("Failed to register filter %s\n", fs->display_text);
}
#else
static void filter_init(struct filter_tbl *ft) {};
#endif

static int getInput(void)
{
	int ch;
	int ret = -1;

	ch = getchar();
	if (ferror(stdin)) {
		perror("error reading input stream");
		abort();
	}

	if (ch >= '0' && ch <= '9')
		ret = ch - '0';

	while ((ch = getchar()) != '\n' && ch != EOF);

	return ret;
}

static void usage(const char *argv0)
{
	printf("Usage: %s [options]\n", argv0);
	printf("-d, --drm-module name        DRM module: 'xilinx' or 'xylon' (default: xylon)\n");
	printf("-h, --help                   Show this help screen\n");
	printf("-p, --partial-reconfig       Enable partial reconfiguration of image filter\n");
	printf("-i, --input-resolution WxH   Input Width'x'Height\n");
	printf("-o, --output-resolution WxH  Output Width'x'Height\n");
}

static struct option opts[] = {
	{ "drm-module", required_argument, NULL, 'd' },
	{ "help", no_argument, NULL, 'h' },
	{ "partial-reconfig", no_argument, NULL, 'p' },
	{ "input-resolution", required_argument, NULL, 'i' },
	{ "output-resolution", required_argument, NULL, 'o' },
	{ NULL, 0, NULL, 0 }
};

int main(int argc, char *argv[])
{
	int ret = 0;
	int i, j, k;
	const char *s, *t;
	int c, choice, current_choice = -1;
	int pr_enable = 0;
	int preferred_mode = 1;
	unsigned int width_out = MAX_WIDTH;
	unsigned int height_out = MAX_HEIGHT;
	unsigned int width_in = 0;
	unsigned int height_in;
	struct filter_s *fs = NULL;
	struct filter_tbl ft = {};
	vlib_config config = {}, current_config;

	/* Register filters */
	filter_init(&ft);

	/* Parse command line arguments */
	while ((c = getopt_long(argc, argv, "d:hpi:o:", opts, NULL)) != -1) {
		switch (c) {
		case 'd':
			module_g = vlib_drm_get_module_type(optarg);
			if (module_g == DRM_MODULE_NONE) {
				printf("Invalid DRM module '%s'\n", optarg);
				return 1;
			}
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		case 'p':
			pr_enable = 1;
			break;
		case 'i':
			ret = sscanf(optarg, "%ux%u", &width_in, &height_in);
			if (ret != 2) {
				printf("Invalid size '%s'\n", optarg);
				return 1;
			}
			break;
		case 'o':
			ret = sscanf(optarg, "%ux%u", &width_out, &height_out);
			if (ret != 2) {
				printf("Invalid size '%s'\n", optarg);
				return 1;
			}
			preferred_mode = 0;
			break;
		default:
			printf("Invalid option -%c\n", c);
			printf("Run %s -h for help\n", argv[0]);
			return 1;
		}
	}

	/* Set input resolution equal to output resolution */
	if (!width_in) {
		width_in = width_out;
		height_in = height_out;
	}

	/* Initialize video library */
	vlib_init(&ft, pr_enable, width_in, height_in);
	if (preferred_mode)
		vlib_drm_init_pref(module_g);
	else
		vlib_drm_init(module_g, width_out, height_out);
	vlib_drm_set_layer0_state(0); // Disable Layer0 (used for graphics only)

	/* Print application status */
	printf("Video Control application:\n");
	printf("------------------------\n");
	printf("DRM module name: %s\n", vlib_drm_get_module_name(module_g));
	printf("HDMI output resolution: %dx%d\n", vlib_get_active_width(), vlib_get_active_height());

	/* Find first enabled video source and make it default */
	for (i = 0; i < VIDEO_SRC_CNT; i++) {
		if (vlib_video_src_get_enabled((video_src) i)) {
			config.src = i;
			break;
		}
	}

	/* Start default video src, filter type, filter mode */
	vlib_change_mode(&config);

	/* Main control menu */
	do {
		/* start with menu index 1 since 0 is used for exit */
		j = 1;
		printf("\n--------------- Select Video Source ---------------\n");
		for (i = 0; i < VIDEO_SRC_CNT; i++) {
			if (vlib_video_src_get_enabled((video_src) i)) {
				s = vlib_video_src_display_text((video_src) i);
				vlib_video_src_set_index((video_src) i, j);
				if (config.src == i)
					t = "(*)";
				else
					t = "";
				printf("%d : %s  %s\n", j++, s, t);
			}
		}
		k = j;
		if (!ft.size)
			goto menu_exit;
		printf("\n--------------- Select Filter Type ----------------\n");
		for (i = 0; i < ft.size; i++) {
			fs = filter_type_get_obj(&ft, i);
			if (config.type == i)
				t = "(*)";
			else
				t = "";
			printf("%d : %s  %s\n", k++, fs->display_text, t);
		}
		printf("\n--------------- Toggle Filter Mode ----------------\n");
		switch (config.mode) {
			case FILTER_MODE_OFF:
				t = "(OFF)";
				break;
//			case FILTER_MODE_SW:
//				t = "(SW)";
//				break;
			case FILTER_MODE_HW:
				t = "(ON)";
				break;
			default:
				t = "(invalid)";
				break;
		}
		printf("%d : Filter OFF/ON  %s\n", k, t);
menu_exit:
		printf("\n--------------- Exit Application ------------------\n");
		printf("0 : Exit\n\n");
		printf("\nEnter your choice : ");

		choice = getInput();

		/* Same option selected */
		if (current_choice == choice && choice < k)
			continue;

		current_config = config;

		if (choice == 0) {
			/* exit application */
			vlib_pipeline_stop();
			vlib_drm_set_layer0_state(1);
			vlib_drm_uninit();
			vlib_uninit();
			exit(0);
		} else if (choice > 0 && choice < j) {
			/* input source select */
			config.src = vlib_video_src_from_index(choice);
		} else if (choice >= j && choice < k) {
			/* filter select */
			config.type = choice - j;
		} else if (choice == k) {
			/* toggle filter mode between off and hw */
//			config.mode++;
//			config.mode %= FILTER_MODE_CNT;
			config.mode = (config.mode == FILTER_MODE_OFF) ? FILTER_MODE_HW : FILTER_MODE_OFF;
		} else {
			printf("\n\n********* Invalid input, Please try Again ***********\n");
			continue;
		}

		/* restore previous mode */
		if (pr_enable && fs && fs->pr_buf == NULL && config.mode == FILTER_MODE_HW) {
			printf("\n\n********* HW module for filter '%s' not available *********\n", fs->display_text);
			config = current_config;
			continue;
		}

		/* Switch to selected video src, filter type, filter mode */
		ret = vlib_change_mode(&config);
		if (ret) {
			printf("ERROR: %s\n", vlib_errstr);
			config = current_config;
			vlib_change_mode(&config);
		} else {
			current_choice = choice;
		}

	} while (choice);

	return 0;
}
