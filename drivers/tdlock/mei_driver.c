/*
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

#include <linux/kernel.h>
#include <linux/mei.h>
#include <linux/mei_cl_bus.h>
#include <linux/pci.h>

#include "mei_dev.h"
#include "client.h"

#include "mei_driver.h"
#include "util.h"

struct td_mei_client {
	struct mei_cl *cl;
	struct mei_cl_device *device;
	struct file *file;
};

static bool td_mei_init_client(struct mei_device *dev,
			       struct td_mei_client *client)
{
	struct mei_cl *cl;
	struct mei_cl_device *device;
	struct file *file;

	int err;
	bool r = false;

	k_func_enter();

	if (!dev) {
		k_err("TD_MEI: device not found.\n");
		r = false;
		goto end;
	}

	mutex_lock(&dev->device_lock);

	cl = NULL;
	device = NULL;
	file = NULL;

	if (dev->dev_state != MEI_DEV_ENABLED) {
		k_err("TD_MEI: dev_state != MEI_ENABLED\n");
		r = false;
		mutex_unlock(&dev->device_lock);
		goto clean;
	}

	cl = devm_kzalloc(&dev->pdev->dev,
			  sizeof(struct mei_cl), GFP_KERNEL);
	if (!cl) {
		k_err("TD_MEI: Alloc cl failed.\n");
		r = false;
		mutex_unlock(&dev->device_lock);
		goto clean;
	}

	mei_cl_init(cl, dev);

	err = mei_cl_link(cl, MEI_HOST_CLIENT_ID_ANY);
	if (err) {
		k_err("TD_MEI: cl link failed, err = %d", err);
		r = false;
		mutex_unlock(&dev->device_lock);
		goto clean;
	}

	mutex_unlock(&dev->device_lock);

	device = devm_kzalloc(&dev->pdev->dev,
			      sizeof(struct mei_cl_device), GFP_KERNEL);
	if (!device) {
		k_err("TD_MEI: Alloc cl_device failed.\n");
		r = false;
		goto clean;
	}
	memset(device, 0, sizeof(*device));
	device->cl = cl;

	file = devm_kzalloc(&dev->pdev->dev,
			    sizeof(struct file), GFP_KERNEL);
	if (!file) {
		k_err("TD_MEI: alloc file failed.\n");
		r = false;
		goto clean;
	}

	client->cl = cl;
	client->device = device;
	client->file = file;
	r = true;
	goto end;

clean:
	devm_kfree(&dev->pdev->dev, file);
	devm_kfree(&dev->pdev->dev, device);
	devm_kfree(&dev->pdev->dev, cl);

end:
	k_func_leave();
	return r;
}

static bool td_mei_ioctl_client(struct td_mei_client *client,
	const uuid_le *client_uuid)
{
	struct mei_device *dev;
	struct mei_cl *cl = client->cl;
	struct file *file = client->file;
	struct mei_connect_client_data *connect_data = NULL;
	int r = false;
	int i;
	int err;

	k_func_enter();

	dev = cl->dev;

	mutex_lock(&dev->device_lock);
	if (dev->dev_state != MEI_DEV_ENABLED) {
		k_err("TD_MEI: dev not enabled.\n");
		r = false;
		goto clean;
	}

	connect_data = devm_kzalloc(&dev->pdev->dev,
				    sizeof(struct mei_connect_client_data),
		GFP_KERNEL);
	if (!connect_data) {
		k_err("TD_MEI: Alloc connnect_data failed.\n");
		r = false;
		goto clean;
	}
	memset(connect_data, 0, sizeof(*connect_data));
	connect_data->in_client_uuid = *client_uuid;

	if (cl->state != MEI_FILE_INITIALIZING &&
	    cl->state != MEI_FILE_DISCONNECTED) {
		k_err("TD_MEI: cl is busy.\n");
		r = false;
		goto clean;
	}

	i = mei_me_cl_by_uuid(dev, &connect_data->in_client_uuid);
	if (i < 0 || dev->me_clients[i].props.fixed_address) {
		k_err("TD_MEI: Cannot connect to FW Client UUID = %pUl\n",
			&connect_data->in_client_uuid);
		r = false;
		goto clean;
	}

	cl->me_client_id = dev->me_clients[i].client_id;
	cl->state = MEI_FILE_CONNECTING;

	err = mei_cl_connect(cl, file);
	if (err < 0) {
		k_err("TD_MEI: mei_cl_connect failed, err = %d", err);
		r = false;
		goto clean;
	}

	r = true;

clean:
	devm_kfree(&dev->pdev->dev, connect_data);
	mutex_unlock(&dev->device_lock);

	k_func_leave();
	return r;
}

static bool td_mei_release_client(struct td_mei_client	*client)
{
	struct mei_cl *cl = client->cl;
	struct mei_cl_cb *cb;
	struct mei_device *dev;
	int err;
	bool r = false;

	k_func_enter();

	dev = cl->dev;

	mutex_lock(&dev->device_lock);

	if (cl->state == MEI_FILE_CONNECTED) {
		cl->state = MEI_FILE_DISCONNECTING;
		err = mei_cl_disconnect(cl);
		if (err < 0) {
			k_err("TD_MEI: Disconnect cl failed, err = %d\n", err);
			r = false;
		} else {
			r = true;
		}
	}
	mei_cl_flush_queues(cl);
	mei_cl_unlink(cl);

	cb = NULL;
	if (cl->read_cb) {
		cb = mei_cl_find_read_cb(cl);
		if (cb)
			list_del(&cb->list);

		cb = cl->read_cb;
		cl->read_cb = NULL;
	}

	client->file->private_data = NULL;

	mei_io_cb_free(cb);

	devm_kfree(&dev->pdev->dev, client->file);
	devm_kfree(&dev->pdev->dev, client->device);
	devm_kfree(&dev->pdev->dev, client->cl);

	mutex_unlock(&dev->device_lock);

	k_func_leave();
	return r;

}

struct device *td_mei_get_device(struct td_mei_client *client)
{
	return &client->cl->dev->pdev->dev;
}

struct td_mei_client *td_mei_open_client(const uuid_le *client_uuid)
{
	struct td_mei_client *client;
	struct mei_device *dev;

	k_func_enter();

	client = NULL;

	dev = get_mei_dev();
	if (dev == NULL) {
		k_err("TD_MEI: Get mei device failed.\n");
		goto end;
	}
	client = devm_kzalloc(&dev->pdev->dev, sizeof(struct td_mei_client),
			      GFP_KERNEL);
	if (!client) {
		k_err("TD_MEI: Alloc client failed.\n");
		goto clean;
	}
	memset(client, 0, sizeof(*client));

	if (!td_mei_init_client(dev, client))
		goto clean;

	if (!td_mei_ioctl_client(client, client_uuid))
		goto clean;

	goto end;

clean:
	devm_kfree(&dev->pdev->dev, client);
	client = NULL;

end:
	k_func_leave();
	return client;
}

void td_mei_close_client(struct td_mei_client *client)
{
	k_func_enter();

	td_mei_release_client(client);

	devm_kfree(&client->cl->dev->pdev->dev, client);

	k_func_leave();
}

ssize_t td_mei_read(struct td_mei_client *client, u8 *message, size_t length)
{
	ssize_t r;
	k_func_enter();

	r = mei_cl_recv(client->device, message, length);

	k_func_leave();
	return r;
}
ssize_t td_mei_write(struct td_mei_client *client, u8 *message, size_t length)
{
	ssize_t r;
	k_func_enter();

	r = mei_cl_send(client->device, message, length);

	k_func_leave();
	return r;

}
