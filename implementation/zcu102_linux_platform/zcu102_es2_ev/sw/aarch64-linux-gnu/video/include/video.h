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

#ifndef VDF_LIB_H
#define VDF_LIB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "filter.h"
#include <stdlib.h>
#include <stdarg.h>

/* Common interface  for video library*/
typedef enum {
	DRM_MODULE_XILINX,
	DRM_MODULE_XYLON,
	DRM_MODULE_NONE = -1,
} drm_module;

typedef enum {
	VIDEO_SRC_TPG,
	VIDEO_SRC_HDMII,
	VIDEO_SRC_UVC,
	VIDEO_SRC_VIVID,
	VIDEO_SRC_FIXME,
	VIDEO_SRC_CNT
} video_src;

#define FILTER_MODE_CNT 3

typedef enum {
	VIDEO_CTRL_OFF,
	VIDEO_CTRL_ON,
	VIDEO_CTRL_AUTO
} video_ctrl;

typedef enum {
	CAPTURE,
	DISPLAY,
	PROCESS_IN,
	PROCESS_OUT,
	NUM_EVENTS
} pipeline_event;

typedef struct {
	video_src src;
	unsigned int type;
	filter_mode mode;
} vlib_config;

/**
 * Error codes. Most vlib functions return 0 on success or one of these
 * codes on failure.
 * User can call vlib_error_name() to retrieve a string representation of an
 * error code or vlib_strerror() to get an end-user suitable description of
 * an error code.
*/

/* Total number of error codes in enum vlib_error */
#define VLIB_ERROR_COUNT 6

typedef enum {
	VLIB_SUCCESS = 0,
	VLIB_ERROR_INTERNAL = -1,
	VLIB_ERROR_CAPTURE = -2,
	VLIB_ERROR_INVALID_PARAM = -3,
	VLIB_ERROR_FILE_IO = -4,
	VLIB_ERROR_OTHER = -99
} vlib_error;

/* Character-array to store string-representation of the error-codes */
#define VLIB_ERRSTR_SIZE 256
extern char vlib_errstr[VLIB_ERRSTR_SIZE];

/**
 *  Log message levels.
 *  - VLIB_LOG_LEVEL_NONE (0)
 *  - VLIB_LOG_LEVEL_ERROR (1)
 *  - VLIB_LOG_LEVEL_WARNING (2)
 *  - VLIB_LOG_LEVEL_INFO (3)
 *  - VLIB_LOG_LEVEL_DEBUG (4)
 *  All the messages are printed on stdout.
 */
typedef enum {
	VLIB_LOG_LEVEL_NONE = 0,
	VLIB_LOG_LEVEL_ERROR,
	VLIB_LOG_LEVEL_WARNING,
	VLIB_LOG_LEVEL_INFO,
	VLIB_LOG_LEVEL_DEBUG,
} vlib_log_level;

struct video_resolution {
	unsigned int height;
	unsigned int width;
	unsigned int stride;
};

/* Maximum number of bytes in a log line */
#define VLIB_LOG_SIZE 256

/* The following is used to silence warnings for unused variables */
#define UNUSED(var)		do { (void)(var); } while(0)

void vlib_log(vlib_log_level level, const char *format, ...)
		__attribute__((__format__(__printf__, 2, 3)));
void vlib_log_v(vlib_log_level level, const char *format, va_list args);

#define _vlib_log(level, ...) vlib_log(level, __VA_ARGS__)

#ifdef DEBUG_MODE
#define INFO_MODE
#define WARN_MODE
#define ERROR_MODE
#define vlib_dbg(...) _vlib_log(VLIB_LOG_LEVEL_DEBUG, __VA_ARGS__)
#else
#define vlib_dbg(...) do {} while(0)
#endif

#ifdef INFO_MODE
#define WARN_MODE
#define ERROR_MODE
#define vlib_info(...) _vlib_log(VLIB_LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define vlib_info(...) do {} while(0)
#endif

#ifdef WARN_MODE
#define ERROR_MODE
#define vlib_warn(...) _vlib_log(VLIB_LOG_LEVEL_WARNING, __VA_ARGS__)
#else
#define vlib_warn(...) do {} while(0)
#endif

#ifdef ERROR_MODE
#define vlib_err(...) _vlib_log(VLIB_LOG_LEVEL_ERROR, __VA_ARGS__)
#else
#define vlib_err(...) do {} while(0)
#endif

/* Platform specific setup */
extern drm_module module_g;

/* video source helper functions */
const char *vlib_video_src_display_text(video_src src);
const char *vlib_video_src_entity_name(video_src src);
unsigned int vlib_video_src_get_enabled(video_src src);
void vlib_video_src_set_index(video_src src, int index);
int vlib_video_src_get_index(video_src src);
video_src vlib_video_src_from_index(int index);

/* drm helper functions */
int vlib_drm_set_layer0_state(int);
int vlib_drm_set_layer0_transparency(int);
int vlib_drm_set_layer0_position(int, int);
int vlib_drm_get_module_type(const char *str);
const char *vlib_drm_get_module_name(drm_module module);
int vlib_drm_try_mode(drm_module module, int width, int height);

/* video resolution functions */
int vlib_get_active_height();
int vlib_get_active_width();

/* init/uninit functions */
int vlib_init(struct filter_tbl *ft, int pr_enable, int width, int height);
int vlib_uninit();
int vlib_drm_init_pref(drm_module module);
int vlib_drm_init(drm_module module, int width, int height);
int vlib_drm_uninit();

/* video pipeline control functions */
int vlib_pipeline_stop();
int vlib_change_mode(vlib_config *config);

/* set event-log function */
int vlib_set_event_log(int state);
/* Query pipeline events*/
int vlib_get_event_cnt(pipeline_event event);

/* return the string representation of the error code */
const char *vlib_error_name(vlib_error error_code);
/* return user-readable description of the error-code */
char *vlib_strerror();

#ifdef __cplusplus
}
#endif

#endif /* VDF_LIB_H */
