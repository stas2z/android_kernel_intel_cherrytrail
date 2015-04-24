/*
 * IES PID Driver main entry.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uuid.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/mei.h>
#include <linux/efi.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <linux/pci.h>

#include "mei_dev.h"
#include "client.h"

/* Use this name to keep compatible with existing IESPID applications. */
#define IESPID_KOBJECT_NAME		"smbios"

#define ACD_FIELD_LENGTH		512
#define ACD_FIELD_INDEX			17
#define ACD_MAIN_OPCODE			9876
#define ACD_OPCODE_IA2CHAABI_ACD_READ	1

#define ACD_FLAG_LENGTH			8
#define ACD_DATA_LENGTH			64

#define iespid_wait_event_interruptible(wq, condition)\
({				\
	int __iespid_ret = 0;	\
	if (!(condition))	\
		__iespid_ret = __wait_event_interruptible(wq, condition);\
	__iespid_ret;		\
})

typedef unsigned char u8;

struct platform_info_table {
	u8 serial_number[32];
	u8 sysflags;
	u8 uuid[16];
	u8 bios_version[48];
	u8 system_product_name[32];
};

struct acd_data {
	u8 flag[ACD_FLAG_LENGTH];
	char data[ACD_DATA_LENGTH - ACD_FLAG_LENGTH];
};

struct acd_hdr_req {
	u16   main_opcode;
	u16   sub_opcode;
};

struct acd_hdr_ack {
	u16   main_opcode;
	u16   sub_opcode;
	u32   status;
};

struct acd_read_cmd_from_host {
	struct acd_hdr_req hdr_req;
	u32 index;
};

struct acd_read_cmd_to_host {
	struct acd_hdr_ack hdr_ack;
	u16 bytes_read;
	u32 acd_status;
	u8 buf[ACD_FIELD_LENGTH];
};

static const uuid_le UMIP_UUID =
UUID_LE(0xafa19346, 0x7459, 0x4f09, 0x9d, 0xad, 0x36, 0x61, 0x1f, 0xe4,
	0x28, 0x60);
static struct platform_info_table oembex_table = {{0},};

#define smbios_attr(_name) \
	static struct kobj_attribute _name##_attr = { \
		.attr = { \
			.name = __stringify(_name), \
			.mode = 0444, \
		}, \
		.show = _name##_show, \
	}

static ssize_t sysflags_show(struct kobject *kobj,
			     struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", oembex_table.sysflags);
}
smbios_attr(sysflags);

static ssize_t product_id_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table.uuid);
}
smbios_attr(product_id);

static ssize_t bios_version_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table.bios_version);
}
smbios_attr(bios_version);

static ssize_t system_product_name_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table.system_product_name);
}
smbios_attr(system_product_name);

static ssize_t serial_number_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", oembex_table.serial_number);
}
smbios_attr(serial_number);

static struct attribute *smbios_attrs[] = {
	&sysflags_attr.attr,
	&product_id_attr.attr,
	&bios_version_attr.attr,
	&system_product_name_attr.attr,
	&serial_number_attr.attr,
	NULL,
};

static struct attribute_group smbios_attr_group = {
	.attrs = smbios_attrs,
};

struct mei_info {
	struct mei_device *dev;
	struct mei_cl *cl;
	struct mei_connect_client_data *data;
	struct file *file;
};

static int mei_info_init(struct mei_info *info)
{
	struct mei_device *dev;
	struct mei_cl *cl;
	struct mei_connect_client_data *connect_data;
	int err;
	struct file *file;

	err = -ENODEV;

	/* dev */
	dev = get_mei_dev();
	if (!dev)
		return err;
	/* end of dev */

	/* cl */
	mutex_lock(&dev->device_lock);

	if (dev->dev_state != MEI_DEV_ENABLED)
		goto mei_dev_disable;

	cl = mei_cl_allocate(dev);
	if (!cl) {
		err = -ENOMEM;
		goto mei_cl_allocate_err;
	}

	/* open_handle_count check is handled in the mei_cl_link */
	err = mei_cl_link(cl, MEI_HOST_CLIENT_ID_ANY);
	if (err)
		goto mei_cl_link_err;

	/* end of cl */

	/* connect data buf */
	connect_data = devm_kzalloc(&dev->pdev->dev,
				    sizeof(struct mei_connect_client_data),
				    GFP_KERNEL);
	if (!connect_data) {
		err = -ENOMEM;
		goto connect_data_allocate_err;
	}

	connect_data->in_client_uuid = UMIP_UUID;
	/* end of connect data buf */

	/* file */
	file = devm_kzalloc(&dev->pdev->dev, sizeof(*file),
			    GFP_KERNEL);
	if (!file) {
		err = -ENOMEM;
		goto file_allocate_err;
	}

	file->private_data = cl;
	/* end of file */

	info->dev = dev;
	info->cl = cl;
	info->data = connect_data;
	info->file = file;

	mutex_unlock(&dev->device_lock);

	return 0;

file_allocate_err:
	devm_kfree(&dev->pdev->dev, connect_data);
connect_data_allocate_err:
mei_cl_link_err:
	kfree(cl);
mei_cl_allocate_err:
mei_dev_disable:
	mutex_unlock(&dev->device_lock);
	return err;
}

static int mei_info_release(struct mei_info *info)
{
	struct mei_cl *cl;
	struct file *file = info->file;
	struct mei_cl_cb *cb;
	struct mei_device *dev;
	int rets;

	rets = 0;
	cl = info->cl;
	dev = cl->dev;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	mutex_lock(&dev->device_lock);
	if (cl == &dev->iamthif_cl) {
		rets = mei_amthif_release(dev, file);
		goto out;
	}
	if (cl->state == MEI_FILE_CONNECTED) {
		cl->state = MEI_FILE_DISCONNECTING;
		rets = mei_cl_disconnect(cl);
	}
	mei_cl_flush_queues(cl);

	mei_cl_unlink(cl);


	/* free read cb */
	cb = NULL;
	if (cl->read_cb) {
		cb = mei_cl_find_read_cb(cl);
		/* Remove entry from read list */
		if (cb)
			list_del(&cb->list);

		cb = cl->read_cb;
		cl->read_cb = NULL;
	}

	file->private_data = NULL;

	mei_io_cb_free(cb);

	kfree(cl);
out:
	mutex_unlock(&dev->device_lock);
	info->dev = NULL;
	devm_kfree(&dev->pdev->dev, info->data);
	devm_kfree(&dev->pdev->dev, info->file);
	return rets;
}

static int mei_ioctl_connect_client(struct mei_info *info)
{
	struct mei_device *dev;
	struct mei_client *client;
	struct mei_cl *cl;
	struct mei_connect_client_data *data;
	int i;
	int rets;

	data = info->data;
	if (WARN_ON(!data))
		return -ENODEV;

	cl = info->cl;
	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	if (dev->dev_state != MEI_DEV_ENABLED) {
		rets = -ENODEV;
		goto end;
	}

	if (cl->state != MEI_FILE_INITIALIZING &&
	    cl->state != MEI_FILE_DISCONNECTED) {
		rets = -EBUSY;
		goto end;
	}

	/* find ME client we're trying to connect to */
	i = mei_me_cl_by_uuid(dev, &data->in_client_uuid);
	if (i < 0 || dev->me_clients[i].props.fixed_address) {
		rets = -ENOTTY;
		goto end;
	}

	cl->me_client_id = dev->me_clients[i].client_id;
	cl->state = MEI_FILE_CONNECTING;

	/* if we're connecting to amthif client then we will use the
	 * existing connection
	 */
	if (uuid_le_cmp(data->in_client_uuid, mei_amthif_guid) == 0) {
		if (dev->iamthif_cl.state != MEI_FILE_CONNECTED) {
			rets = -ENODEV;
			goto end;
		}
		mei_cl_unlink(cl);

		kfree(cl);
		cl = NULL;
		dev->iamthif_open_count++;
		info->cl = &dev->iamthif_cl;
		info->file->private_data = cl;

		client = &data->out_client_properties;
		client->max_msg_length =
			dev->me_clients[i].props.max_msg_length;
		client->protocol_version =
			dev->me_clients[i].props.protocol_version;
		rets = dev->iamthif_cl.status;

		goto end;
	}

	/* prepare the output buffer */
	client = &data->out_client_properties;
	client->max_msg_length = dev->me_clients[i].props.max_msg_length;
	client->protocol_version = dev->me_clients[i].props.protocol_version;

	rets = mei_cl_connect(cl, info->file);

end:
	return rets;
}

static int mei_connect_client(struct mei_info *info)
{
	struct mei_device *dev;
	struct mei_cl *cl = info->cl;
	struct mei_connect_client_data *connect_data = info->data;
	int rets;

	if (WARN_ON(!connect_data))
		return -ENODEV;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	mutex_lock(&dev->device_lock);
	if (dev->dev_state != MEI_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	rets = mei_ioctl_connect_client(info);

out:
	mutex_unlock(&dev->device_lock);
	return rets;
}

/**
 * mei_write - the write function.
 *
 * @file: pointer to file structure
 * @ubuf: pointer to user buffer
 * @length: buffer length
 * @offset: data offset in buffer
 *
 * returns >=0 data length on success , <0 on error
 */
static ssize_t mei_write(struct mei_info *info, const char *kbuf,
			 size_t length, loff_t *offset)
{
	struct mei_cl *cl = info->cl;
	struct file *file = info->file;
	struct mei_cl_cb *write_cb = NULL;
	struct mei_device *dev;
	unsigned long timeout = 0;
	int rets;
	int id;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	mutex_lock(&dev->device_lock);

	if (dev->dev_state != MEI_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	id = mei_me_cl_by_id(dev, cl->me_client_id);
	if (id < 0) {
		rets = -ENODEV;
		goto out;
	}

	if (length == 0) {
		rets = 0;
		goto out;
	}

	if (length > dev->me_clients[id].props.max_msg_length) {
		rets = -EFBIG;
		goto out;
	}

	if (cl->state != MEI_FILE_CONNECTED) {
		rets = -ENODEV;
		goto out;
	}
	if (cl == &dev->iamthif_cl) {
		write_cb = mei_amthif_find_read_list_entry(dev, file);

		if (write_cb) {
			timeout = write_cb->read_time +
				mei_secs_to_jiffies(MEI_IAMTHIF_READ_TIMER);

			if (time_after(jiffies, timeout) ||
			    cl->reading_state == MEI_READ_COMPLETE) {
				*offset = 0;
				list_del(&write_cb->list);
				mei_io_cb_free(write_cb);
				write_cb = NULL;
			}
		}
	}

	/* free entry used in read */
	if (cl->reading_state == MEI_READ_COMPLETE) {
		*offset = 0;
		write_cb = mei_cl_find_read_cb(cl);
		if (write_cb) {
			list_del(&write_cb->list);
			mei_io_cb_free(write_cb);
			write_cb = NULL;
			cl->reading_state = MEI_IDLE;
			cl->read_cb = NULL;
		}
	} else if (cl->reading_state == MEI_IDLE)
		*offset = 0;


	write_cb = mei_io_cb_init(cl, file);
	if (!write_cb) {
		rets = -ENOMEM;
		goto out;
	}
	rets = mei_io_cb_alloc_req_buf(write_cb, length);
	if (rets)
		goto out;

	memcpy(write_cb->request_buffer.data, (const void *)kbuf, length);

	if (cl == &dev->iamthif_cl) {
		rets = mei_amthif_write(dev, write_cb);
		if (rets)
			goto out;
		mutex_unlock(&dev->device_lock);
		return length;
	}

	rets = mei_cl_write(cl, write_cb, false);
out:
	mutex_unlock(&dev->device_lock);
	if (rets < 0)
		mei_io_cb_free(write_cb);
	return rets;
}

/**
 * mei_read - the read function.
 *
 * @file: pointer to file structure
 * @ubuf: pointer to user buffer
 * @length: buffer length
 * @offset: data offset in buffer
 *
 * returns >=0 data length on success , <0 on error
 */
static ssize_t mei_read(struct mei_info *info, char *kbuf,
			size_t length, loff_t *offset)
{
	struct mei_cl *cl = info->cl;
	struct file *file = info->file;
	struct mei_cl_cb *cb_pos = NULL;
	struct mei_cl_cb *cb = NULL;
	struct mei_device *dev;
	int rets;
	int err;

	if (WARN_ON(!cl || !cl->dev))
		return -ENODEV;

	dev = cl->dev;

	mutex_lock(&dev->device_lock);
	if (dev->dev_state != MEI_DEV_ENABLED) {
		rets = -ENODEV;
		goto out;
	}

	if (length == 0) {
		rets = 0;
		goto out;
	}

	if (cl->read_cb) {
		cb = cl->read_cb;
		/* read what left */
		if (cb->buf_idx > *offset)
			goto copy_buffer;
		/* offset is beyond buf_idx we have no more data return 0 */
		if (cb->buf_idx > 0 && cb->buf_idx <= *offset) {
			rets = 0;
			goto free;
		}
		/* Offset needs to be cleaned for contiguous reads*/
		if (cb->buf_idx == 0 && *offset > 0)
			*offset = 0;
	} else if (*offset > 0)
		*offset = 0;

	err = mei_cl_read_start(cl, length);
	if (err && err != -EBUSY) {
		rets = err;
		goto out;
	}

	if (MEI_READ_COMPLETE != cl->reading_state &&
	    !waitqueue_active(&cl->rx_wait)) {
		if (file->f_flags & O_NONBLOCK) {
			rets = -EAGAIN;
			goto out;
		}

		mutex_unlock(&dev->device_lock);

		if (iespid_wait_event_interruptible(cl->rx_wait,
				MEI_READ_COMPLETE == cl->reading_state ||
				mei_cl_is_transitioning(cl))) {
			if (signal_pending(current))
				return -EINTR;
			return -ERESTARTSYS;
		}

		mutex_lock(&dev->device_lock);
		if (mei_cl_is_transitioning(cl)) {
			rets = -EBUSY;
			goto out;
		}
	}

	cb = cl->read_cb;

	if (!cb) {
		rets = -ENODEV;
		goto out;
	}
	if (cl->reading_state != MEI_READ_COMPLETE) {
		rets = 0;
		goto out;
	}
	/* now copy the data to user space */
copy_buffer:
	if (length == 0 || kbuf == NULL || *offset > cb->buf_idx) {
		rets = -EMSGSIZE;
		goto free;
	}

	/* length is being truncated to PAGE_SIZE,
	 * however buf_idx may point beyond that */
	length = min_t(size_t, length, cb->buf_idx - *offset);

	memcpy((void *)kbuf, cb->response_buffer.data + *offset, length);

	rets = length;
	*offset += length;
	if ((unsigned long)*offset < cb->buf_idx)
		goto out;

free:
	cb_pos = mei_cl_find_read_cb(cl);
	/* Remove entry from read list */
	if (cb_pos)
		list_del(&cb_pos->list);
	mei_io_cb_free(cb);
	cl->reading_state = MEI_IDLE;
	cl->read_cb = NULL;
out:
	mutex_unlock(&dev->device_lock);
	return rets;
}

static int mei_read_acd_area(int idx, struct acd_data *data)
{
	struct acd_read_cmd_from_host *req;
	struct acd_read_cmd_to_host *ack;
	struct mei_info info;
	int err = 0;
	loff_t off = 0;

	err = mei_info_init(&info);
	if (err)
		goto mei_info_init_err;

	err = mei_connect_client(&info);
	if (err)
		goto mei_connect_client_err;

	ack = kzalloc(sizeof(*ack), GFP_KERNEL);
	if (!ack) {
		err = -ENOMEM;
		goto ack_alloc_err;
	}
	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req) {
		err = -ENOMEM;
		goto req_alloc_err;
	}

	req->hdr_req.main_opcode = ACD_MAIN_OPCODE;
	req->hdr_req.sub_opcode = ACD_OPCODE_IA2CHAABI_ACD_READ;
	req->index = idx;

	if (mei_write(&info, (const char *)req, sizeof(*req), &off) < 0) {
		err = -1;
		goto mei_read_write_err;
	}

	off = 0;
	if (mei_read(&info, (char *)ack, sizeof(*ack), &off) < 0) {
		err = -1;
		goto mei_read_write_err;
	}

	memcpy(data, ack->buf, sizeof(*data));

mei_read_write_err:
	kfree(req);
req_alloc_err:
	kfree(ack);
ack_alloc_err:
mei_connect_client_err:
	mei_info_release(&info);
mei_info_init_err:
	return err;
}

static void fill_iespid_by_acd_area(struct platform_info_table *oemb)
{
	struct acd_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		goto out;

	if (mei_read_acd_area(ACD_FIELD_INDEX, data))
		goto mei_acd_get_data_err;

	oemb->sysflags = data->flag[0];
	strncpy(oemb->system_product_name, data->data,
		sizeof(oemb->system_product_name));

mei_acd_get_data_err:
	kzfree(data);
out:
	return;
}

static void iespid_probe(struct platform_info_table *oemb)
{
	fill_iespid_by_acd_area(oemb);
}

static int __init iespid_init(void)
{
	struct kobject *smbios_kobj;

	smbios_kobj = kobject_create_and_add(IESPID_KOBJECT_NAME, NULL);
	if (!smbios_kobj)
		return -1;

	if (sysfs_create_group(smbios_kobj, &smbios_attr_group))
		return -1;

	iespid_probe(&oembex_table);

	return 0;
}

late_initcall(iespid_init);
