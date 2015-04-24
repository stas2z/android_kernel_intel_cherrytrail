/*
 * SRTC functions for threft deterrent lock driver.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>	/* create_proc_entry() */
#include <linux/errno.h>
#include <linux/uaccess.h>

#include "util.h"		/* k_err(), k_func_enter(), k_func_leave() */
#include "sclock.h"		/* declaration */
#include "mei.h"		/* td_mei_get_sc() */
#include "tdos.h"

#define TD_SCLOCK_CMD_REBOOT_TO_AOS   _IO('M', 0x01)
#define TD_SCLOCK_CMD_REBOOT_TO_TDOS  _IO('X', 0x02)

static ssize_t td_sclock_procfile_read(struct file *filp,
				       char __user *buf, size_t count,
				       loff_t *offp)
{
	u32 sclock;
	unsigned long size;

	size = sizeof(u32);
	if (count < size) {
		k_err("Invalid count of secure clock!\n");
		return 0;
	}

	if (td_mei_get_sc(&sclock))
		return copy_to_user(buf, &sclock, size);

	return 0;
}
static long td_reboot_procfile_ioctl(struct file *filp,
				     unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case TD_SCLOCK_CMD_REBOOT_TO_AOS:
		td_reboot_to_aos();
		return 0;
	case TD_SCLOCK_CMD_REBOOT_TO_TDOS:
		td_reboot_to_tdos();
		return 0;
	default:
		return -EINVAL;
		break;
	}
}


static const struct file_operations td_proc_file_fops = {
	.owner = THIS_MODULE,
	.read  = td_sclock_procfile_read,
};

void td_register_secure_clock_file(void)
{
	struct proc_dir_entry *proc_sclock;

	k_func_enter();

	proc_sclock = proc_create("sclock", S_IRUGO|S_IFREG, NULL,
				  &td_proc_file_fops);
	if (proc_sclock == NULL)
		k_err("Fail to create proc entry for secure clock!\n");
	k_func_leave();
}

static const struct file_operations td_reboot_proc_file_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = td_reboot_procfile_ioctl,
};

void td_register_reboot_file(void)
{
	struct proc_dir_entry *proc_reboot;

	k_func_enter();

	proc_reboot = proc_create("reboot", S_IRUGO|S_IFREG, NULL,
				  &td_reboot_proc_file_fops);
	if (proc_reboot == NULL)
		k_err("Fail to create proc entry for reboot!\n");
	k_func_leave();
}
