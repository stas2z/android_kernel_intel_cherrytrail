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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/tpm.h>
#include "tpmcmd.h"
#include "util.h"

static int td_tpm_chip_num;

void td_tpm_select_chip_num(int chip_num)
{
	td_tpm_chip_num = 0; /*Use /dev/tpm0 as default*/
	if (chip_num < 0)
		k_err("Invalid TPM chip number select!\n");
	else
		td_tpm_chip_num = chip_num;
}

ssize_t td_tpm_transmit(u8 *buf, size_t size)
{
#ifdef TCG_TPM_MODULE
	k_err("Unsupport TDLOCK driver!\n");
	return -1;
#else
	union tpm_cmd_header *header_ptr;
	int rc;

	k_func_enter();

	if (size < sizeof(*header_ptr)) {
		rc = -1;
		k_err("Invalid input buffer size! size=%d\n", (int)size);
		goto out;
	}

	header_ptr = (union tpm_cmd_header *)buf;
	k_debug("[Command 0x%02x input buffer(size:0x%x)]\n",
		be32_to_cpu(header_ptr->in.ordinal),
		be32_to_cpu(header_ptr->in.length));
	k_print_hexbuf("", buf, be32_to_cpu(header_ptr->in.length));

	rc = tpm_send(td_tpm_chip_num, buf, size);
	if (rc < 0) {
		k_err("Fail to execute command! rc=%d\n", rc);
		goto out;
	}
	if (header_ptr->out.return_code != 0) {
		rc = -EIO;
		k_err("Fail to execute command! TPM return code=0x%x\n",
		      be32_to_cpu(header_ptr->out.return_code));
		goto out;
	}
	k_debug("[Command output buffer(size:0x%x)]\n",
		be32_to_cpu(header_ptr->out.length));
	k_print_hexbuf("", buf, be32_to_cpu(header_ptr->out.length));

out:
	k_func_leave();
	return rc;
#endif
}

bool td_arr_to_uuid(u8 *in, size_t in_size, struct uuid *uu)
{
	int index;
	if (in_size < 16)
		return false;
	uu->time_low = (in[0]<<24) | (in[1]<<16) | (in[2]<<8) | in[3];
	uu->time_mid = (in[4]<<8) | in[5];
	uu->time_hi_and_version = (in[6]<<8) | in[7];
	uu->clock_seq = (in[8]<<8) | in[9];
	for (index = 0; index < 6; index++)
		uu->node[index] = in[10 + index];
	return true;
}
