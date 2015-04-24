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

#ifndef TD_LOCK_DRIVER_SCLOCK_H
#define TD_LOCK_DRIVER_SCLOCK_H

#include <linux/module.h>
#include <linux/kernel.h>

extern void td_register_secure_clock_file(void);
extern void td_register_reboot_file(void);
#endif /* TD_LOCK_DRIVER_SCLOCK_H */

