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

#ifndef TD_LOCK_DRIVER_MEI_DRIVER_H
#define TD_LOCK_DRIVER_MEI_DRIVER_H

#include <linux/kernel.h>
#include <linux/uuid.h>

struct td_mei_client;

struct device *td_mei_get_device(struct td_mei_client *client);
struct td_mei_client *td_mei_open_client(const uuid_le *client_uuid);
void td_mei_close_client(struct td_mei_client *client);

ssize_t td_mei_read(struct td_mei_client *client, u8 *message, size_t length);
ssize_t td_mei_write(struct td_mei_client *client, u8 *message,	size_t length);

#endif /* TD_LOCK_DRIVER_MEI_DRIVER_H */

