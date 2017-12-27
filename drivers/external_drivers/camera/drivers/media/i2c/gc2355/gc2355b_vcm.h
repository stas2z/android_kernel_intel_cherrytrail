#ifndef __VCM_H__
#define __VCM_H__

#include "unicam_ext.h"

struct gc2355b_vcm {
	int (*power_up)(struct v4l2_subdev *sd);
	int (*power_down)(struct v4l2_subdev *sd);
	int (*init)(struct v4l2_subdev *sd);
	int (*t_focus_vcm)(struct v4l2_subdev *sd, u16 val);
	int (*t_focus_abs)(struct v4l2_subdev *sd, s32 value);
	int (*t_focus_rel)(struct v4l2_subdev *sd, s32 value);
	int (*q_focus_status)(struct v4l2_subdev *sd, s32 *value);
	int (*q_focus_abs)(struct v4l2_subdev *sd, s32 *value);
};

struct gc2355b_vcm_mapping {
	char name[32];
	int  id;
	struct gc2355b_vcm * vcm;
};

const struct gc2355b_vcm_mapping gc2355b_supported_vcms[] = {
	{},
};
#endif
