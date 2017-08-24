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
* @file video_lib.h
*
* This file provides video library interface.
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

#ifndef VIDEO_INT_H
#define VIDEO_INT_H

#include <video.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Misc configuration */
#define HRES_2160P      3840
#define VRES_2160P      2160
#define HRES_1080P      1920
#define VRES_1080P      1080
#define HRES_720P       1280
#define VRES_720P        720

#define BYTES_PER_PIXEL    2

#define BUFFER_CNT         4
#define MAX_BUFFER_CNT     5

#define OUTPUT_PIX_FMT v4l2_fourcc('Y','U','Y','V')
#define INPUT_PIX_FMT  v4l2_fourcc('Y','U','Y','V')

/* Platform specific configuration */
#if defined(PLATFORM_ZCU102) || defined(PLATFORM_ZC1751_DC1)
#define MAX_HEIGHT           VRES_2160P
#define MAX_WIDTH            HRES_2160P
#define MAX_STRIDE           MAX_WIDTH
#define DRM_MODULE           DRM_MODULE_XILINX
#define DRM_ALPHA_PROP       "alpha"
#define DRM_MAX_ALPHA        255
#define FSYNC_SEL_GPIO       2
#define VCAP_HDMI_HAS_SCALER 0
#elif defined(PLATFORM_ZC70X)
#define MAX_HEIGHT           VRES_1080P
#define MAX_WIDTH            HRES_1080P
#define MAX_STRIDE           2048 // Xylon has fixed stride (has to be power of 2 and greater or equal than MAX_WIDTH)
#define DRM_MODULE           DRM_MODULE_XYLON
#define DRM_ALPHA_PROP       "transparency"
#define DRM_MAX_ALPHA        255
#define FSYNC_SEL_GPIO       1
#define VCAP_HDMI_HAS_SCALER 0
#endif

struct video_pipeline;
struct vlib_vdev;
struct media_device;

struct matchtable {
	char *s;
	int (*init)(struct vlib_vdev *vdev_tbl, const struct matchtable *mte,
		    struct media_device *data);
};

struct vsrc_ops {
	int (*change_mode)(struct video_pipeline *video_setup,
			   vlib_config *config);
	int (*set_media_ctrl)(struct video_pipeline *video_setup,
			      const struct vlib_vdev *vdev);
};

struct vlib_vdev {
	video_src src;
	const char *display_text;
	const char *entity_name;
	unsigned int enabled;
	int index;
	struct media_device *mdev;
	const struct vsrc_ops *ops;
};

int vlib_video_src_init(void);
void vlib_video_src_uninit(void);
video_src vlib_vdev_get_vsrc(const struct vlib_vdev *vdev);
struct media_device *vlib_vdev_get_mdev(const struct vlib_vdev *vdev);
const struct vlib_vdev *vlib_video_src_get_vdev(video_src src);
void vlib_video_src_disable(video_src src);

#ifdef __cplusplus
}
#endif
#endif

