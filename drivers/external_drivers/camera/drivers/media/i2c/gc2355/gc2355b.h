/*
 *
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
*/

#ifndef __UNICAM_H__
#define __UNICAM_H__


#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/spinlock.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-device.h>
//#include <media/v4l2-chip-ident.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>

#include <linux/atomisp_platform.h>

#define BUILDID 52
#include "unicam_ext_linux.h"
#include "unicam_ext.h"

#define MATCH_NAME_LEN 30
#define RATIO_SHIFT_BITS		13
#define LARGEST_ALLOWED_RATIO_MISMATCH	320


struct match_data {
		u8 name[MATCH_NAME_LEN];
		S_UNI_DEVICE * uni;
};

struct gc2355b_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

struct sensor_data {
	struct v4l2_subdev sd;
	struct media_pad pad;

	struct mutex input_lock; // lock i2c access, and fmt_idx group

	struct camera_sensor_platform_data *platform_data;

	struct gc2355b_vcm *vcm_driver;

	struct v4l2_ctrl_handler ctrl_handler;

	struct v4l2_ctrl *run_mode;

	struct v4l2_mbus_framefmt format;

	// lock protected
	int fmt_idx;
	int mode;
	int parm_mode;
	S_UNI_RESOLUTION *cur_res ;
	int n_res;
	//

	S_UNI_DEVICE * uni;
};

#define to_sensor_data(x) container_of(x, struct sensor_data, sd)

#endif
