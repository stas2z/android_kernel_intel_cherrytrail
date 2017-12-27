#ifndef __UNICAM_HW_H__
#define __UNICAM_HW_H__

#ifndef UNI_NO_EXT
/*
#include "unicam_hm8131.h"
#include "unicam_hm2056.h"
#include "unicam_m1040.h"
#include "unicam_ar0543.h"
#include "unicam_hm2051.h"
#include "unicam_ov8865.h"
*/
#include "unicam_gc2355.h"
/*
#include "unicam_ov5648.h"
#include "unicam_hm5040.h"
#include "unicam_gc0310.h"
*/
enum gc2355_SENSOR_CAM_TYPE {
	gc2355_sensor_ppr_cam_type,
	gc2355_sensor_spr_cam_type,
};


static struct match_data gc2355_m_data[]={
//	{"ar0543",&ar0543_unidev},//aptina ar0543 5M
//	{"hm8131",&hm8131_unidev},//himax hm8131 8M
//	{"ov8865",&ov8865_unidev},//ov8865 8M
	{"gc2355",&gc2355_unidev},//galaxycore gc2355 2M
//	{"ov5648",&ov5648_unidev},//ov ov5648 5M
//	{"hm5040",&hm5040_unidev},//himax hm5040 5M
	{},
};

static struct match_data gc2355_m_data_front[]={
//	{"m1040",&m1040_unidev},//aptina m1040 1.2M
//	{"hm2051",&hm2051_unidev},//himax hm2056 2M
	{"gc2355",&gc2355_unidev},//galaxycore gc2355 2M
//	{"gc2355b",&gc2355b_unidev},//galaxycore gc2355 2M
//	{"gc0310",&gc0310_unidev},//galaxycore gc2355 2M

	{},
};

const struct i2c_device_id gc2355_cam[] = {
	{"gc2355", gc2355_sensor_ppr_cam_type},
	{}
};

#endif

#endif
