/*
 * Threft Deterrent Driver utility functions.
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

#ifndef TD_LOCK_DRIVER_UTIL_H
#define TD_LOCK_DRIVER_UTIL_H

#include <linux/module.h>
#include <linux/kernel.h>

#define TD_DEBUG

#ifdef TD_DEBUG
#define k_info(fmt, ...) \
	pr_info(pr_fmt(fmt), ##__VA_ARGS__)
#define k_debug(fmt, ...) \
	pr_info(pr_fmt(fmt), ##__VA_ARGS__)
#define k_warn(fmt, ...) \
	pr_warn(pr_fmt(fmt), ##__VA_ARGS__)
#define k_func_enter() \
	k_debug("[%s] >>>\n", __func__)
#define k_func_leave() \
	k_debug("[%s] <<<\n", __func__)
#define k_print_hexbuf(prefix, buf, len) \
	print_hex_dump(KERN_DEBUG, prefix, DUMP_PREFIX_OFFSET, 16, 1, \
		       buf, len, true)
#else
#define k_info(fmt, ...)
#define k_debug(fmt, ...)
#define k_warn(fmt, ...)
#define k_func_enter()
#define k_func_leave()
#define k_print_hexbuf(prefix, buf, len)
#endif

#define k_err(fmt, ...) \
	pr_err(pr_fmt(fmt), ##__VA_ARGS__)
#define k_crit(fmt, ...) \
	pr_crit(pr_fmt(fmt), ##__VA_ARGS__)

struct td_date {
	u16 year;
	u8  month;
	u8  day;
};

struct uuid {
	u32 time_low;
	u16 time_mid;
	u16 time_hi_and_version;
	u16 clock_seq;
	u8 node[6];
};

extern ssize_t td_tpm_transmit(u8 *buf, size_t size);
extern void td_tpm_select_chip_num(int chip_num);
extern bool td_arr_to_uuid(u8 *in, size_t in_size, struct uuid *uu);
/* tpm_send() is implemented by
 * drivers/char/tpm driver.
 */
int tpm_send(u32 chip_num, void *cmd, size_t buflen);

#endif
