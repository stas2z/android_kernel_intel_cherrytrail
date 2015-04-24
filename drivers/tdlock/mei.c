/*
 * MEI functionalities for threft deterrent lock driver.
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program..
 */

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uuid.h>
#include <linux/uaccess.h>
#include <linux/mei.h>

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#include "mei_dev.h"
#include "mei.h"
#include "mei_driver.h"
#include "util.h"

#define TD_ACD_FIELD_LENGTH			512
#define TD_ACD_FIELD_INDEX			17
#define TD_ACD_MAIN_OPCODE			9876
#define TD_ACD_OPCODE_IA2CHAABI_ACD_READ	1

#define TD_ACD_FLAG_LENGTH			8
#define TD_ACD_DATA_LENGTH			64
#define TD_ACD_FLAG_BYTE			0x11
#define TD_ACD_FLAG_BYTE_TWO			0x01 /*To support PVT sample*/

#define TD_SC_GET_TICKS_CMD			0x1D
#define TD_SC_GET_TICKS_ACK_CMD			0x9D
#define MKHI_GEN_GROUP_ID			0xFF

#define BIOS_FIXED_HOST_ADDR			0
#define MEI_CORE_MESSAGE_ADDR			0x07

struct td_acd_data {
	u8 td_flag[TD_ACD_FLAG_LENGTH];
	char data[TD_ACD_DATA_LENGTH - TD_ACD_FLAG_LENGTH];
};

struct td_acd_hdr_req {
	u16 main_opcode;
	u16 sub_opcode;
};

struct td_acd_hdr_ack {
	u16 main_opcode;
	u16 sub_opcode;
	u32 status;
};

struct td_acd_read_cmd_from_host {
	struct td_acd_hdr_req hdr_req;
	u32 index;
};

struct td_acd_read_cmd_to_host {
	struct td_acd_hdr_ack hdr_ack;
	u16 bytes_read;
	u32 acd_status;
	u8 buf[TD_ACD_FIELD_LENGTH];
};

#pragma pack(1)

struct td_sc_tick {
	u32 ticks_counter;
	u32 ticks_valid;
};

union mkhi_message_header {
	u32 data;
	struct {
		u32 group_id:8;
		u32 command:7;
		u32 is_response:1;
		u32 reserved:8;
		u32 result:8;
	} fields;
};

struct td_sc_get_ticks_req {
	union mkhi_message_header header;
};

struct td_sc_get_ticks_ack {
	union mkhi_message_header header;
	struct td_sc_tick ticks;
};

#pragma pack()

static const uuid_le UMIP_UUID =
UUID_LE(0xafa19346, 0x7459, 0x4f09, 0x9d, 0xad, 0x36, 0x61,
	0x1f, 0xe4, 0x28, 0x60);

static const uuid_le MKHI_UUID =
UUID_LE(0x8e6a6715, 0x9abc, 0x4043, 0x88, 0xef, 0x9e, 0x39,
	0xc6, 0xf6, 0x3e, 0xf);


static bool td_mei_get_acd_data(struct td_acd_data *data)
{
	mm_segment_t fs;
	struct td_acd_read_cmd_from_host *req;
	struct td_acd_read_cmd_to_host *ack;
	struct td_mei_client *client;
	struct device *dev;
	bool r;
	int cnt;

	k_func_enter();

	client = td_mei_open_client(&UMIP_UUID);
	if (!client) {
		k_err("TD_MEI_ACD: Open client failed.\n");
		r = false;
		goto end;
	}

	dev = td_mei_get_device(client);
	if (!dev) {
		k_err("TD_MEI_ACD: Get td mei device failed.\n");
		r = false;
		goto end;
	}

	ack = devm_kzalloc(dev,
			   sizeof(*ack), GFP_KERNEL);
	if (!ack) {
		k_err("TD_MEI_ACD: Alloc ack failed.\n");
		r = false;
		goto end;
	}
	req = devm_kzalloc(dev,
			   sizeof(*req), GFP_KERNEL);
	if (!req) {
		k_err("TD_MEI_ACD: Alloc req failed.\n");
		r = false;
		goto end;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	memset(req, 0, sizeof(*req));

	req->hdr_req.main_opcode = TD_ACD_MAIN_OPCODE;
	req->hdr_req.sub_opcode = TD_ACD_OPCODE_IA2CHAABI_ACD_READ;
	req->index = TD_ACD_FIELD_INDEX;

	cnt = td_mei_write(client, (u8 *)req, sizeof(*req));
	if (cnt != sizeof(*req)) {
		k_err("TD_MEI_ACD: Send get data req failed, %d.\n", cnt);
		r = false;
		goto clean;
	}

	memset(ack, 0, sizeof(*ack));

	cnt = td_mei_read(client, (u8 *)ack, sizeof(*ack));
	if (cnt <= 0) {
		k_err("TD_MEI_ACD: Receive get data ack failed, %d.\n", cnt);
		r = false;
		goto clean;
	}
	if (ack->hdr_ack.status) {
		k_err("TD_MEI_ACD: Read data failed, status %d.\n",
		      ack->hdr_ack.status);
		r = false;
		goto clean;
	}
	if (ack->bytes_read < TD_ACD_FLAG_LENGTH) {
		k_err("TD_MEI_ACD: TD flag not found, length %d.\n",
		      ack->bytes_read);
		r = false;
		goto clean;
	}

	memcpy(data, ack->buf, sizeof(*data));
	r = true;

clean:
	set_fs(fs);
	devm_kfree(dev, req);
	devm_kfree(dev, ack);
	td_mei_close_client(client);

end:
	k_func_leave();
	return r;
}

bool td_mei_is_td_supported(bool *is_supported)
{
	bool r;
	int flag_marked;
	int i;
	struct td_acd_data *data;

	k_func_enter();

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!td_mei_get_acd_data(data)) {
		r = false;
		goto end;
	}

	r = true;
	flag_marked = 0;
	for (i = 0; i < TD_ACD_FLAG_LENGTH; ++i) {
		/* To support PVT sample */
		if (data->td_flag[i] == TD_ACD_FLAG_BYTE
		    || data->td_flag[i] == TD_ACD_FLAG_BYTE_TWO)
			flag_marked++;
	}

	*is_supported = (flag_marked >= TD_ACD_FLAG_LENGTH);

end:
	kzfree(data);

	k_func_leave();
	return r;
}

bool td_mei_get_sc(u32 *tick)
{
	mm_segment_t fs;
	struct td_sc_get_ticks_req *req;
	struct td_sc_get_ticks_ack *ack;
	struct td_mei_client *client;
	struct device *dev;
	bool r;
	ssize_t cnt;

	k_func_enter();

	client = td_mei_open_client(&MKHI_UUID);
	if (!client) {
		k_err("TD_MEI_SC: Open client failed.\n");
		r = false;
		goto end;
	}

	dev = td_mei_get_device(client);
	if (!dev) {
		k_err("TD_MEI_GET_SC: Get td mei device failed.\n");
		r = false;
		goto end;
	}

	ack = devm_kzalloc(dev,
			   sizeof(*ack), GFP_KERNEL);
	if (!ack) {
		k_err("TD_MEI_ACD: Alloc ack failed.\n");
		r = false;
		goto end;
	}

	req = devm_kzalloc(dev,
			   sizeof(*req), GFP_KERNEL);
	if (!req) {
		k_err("TD_MEI_ACD: Alloc req failed.\n");
		r = false;
		goto end;
	}

	memset(req, 0, sizeof(*req));
	req->header.fields.group_id = MKHI_GEN_GROUP_ID;
	req->header.fields.command = TD_SC_GET_TICKS_CMD;
	req->header.fields.is_response = 0;

	fs = get_fs();
	set_fs(KERNEL_DS);

	cnt = td_mei_write(client, (u8 *)req, sizeof(*req));
	if (cnt != sizeof(*req)) {
		k_err("TD_MEI_SC: Send get tick req failed, %d.\n", (int)cnt);
		r = false;
		goto clean;
	}

	memset(ack, 0, sizeof(*ack));

	cnt = td_mei_read(client, (u8 *)ack, sizeof(*ack));
	if (cnt <= 0) {
		k_err("TD_MEI_SC: Receive get tick ack failed, %d.\n",
		      (int)cnt);
		r = false;
		goto clean;
	}

	*tick = ack->ticks.ticks_counter;
	r = true;

clean:
	set_fs(fs);
	devm_kfree(dev, req);
	devm_kfree(dev, ack);
	td_mei_close_client(client);

end:
	k_func_leave();
	return r;
}
