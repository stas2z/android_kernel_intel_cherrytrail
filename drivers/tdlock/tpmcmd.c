/*
 * TPM command functions.
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

#include <linux/reboot.h>       /* kernel_restart() */
#include "tpmcmd.h"
#include "util.h"               /* k_func_enter(), k_func_leave() */

static const struct tpm_input_header tpm_getcap_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(22),
	.ordinal = TPM_ORD_GetCapability
};

static const struct tpm_input_header tpm_nvread_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(22),
	.ordinal = TPM_ORD_NV_ReadValue
};

static const struct tpm_input_header tpm_nvwrite_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.ordinal = TPM_ORD_NV_WriteValue
};

static const struct tpm_input_header tpm_physicalpresence_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(12),
	.ordinal = TSC_ORD_PhysicalPresence
};

static const struct tpm_input_header tpm_continueselftest_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(10),
	.ordinal = TPM_ORD_ContinueSelfTest
};

static const struct tpm_input_header tpm_physicalenable_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(10),
	.ordinal = TPM_ORD_PhysicalEnable
};

static const struct tpm_input_header tpm_physicalpresencelock_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(12),
	.ordinal = TSC_ORD_PhysicalPresence
};

static const struct tpm_input_header tpm_physicalsetdeactive_header = {
	.tag = TPM_TAG_RQU_COMMAND,
	.length = cpu_to_be32(11),
	.ordinal = TPM_ORD_PhysicalSetDeactivated
};

ssize_t td_tpm_getcap(__be32 subcap_id, union cap_t *cap)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	int length;

	tpm_cmd.header.in = tpm_getcap_header;
	if (subcap_id == CAP_VERSION_1_1 || subcap_id == CAP_VERSION_1_2) {
		tpm_cmd.params.getcap_in.cap = subcap_id;
		/*subcap field not necessary */
		tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(0);
		length = be32_to_cpu(tpm_cmd.header.in.length);
		length -= sizeof(__be32);
		tpm_cmd.header.in.length = cpu_to_be32(length);
	} else {
		if (subcap_id == TPM_CAP_FLAG_PERM ||
		    subcap_id == TPM_CAP_FLAG_VOL)
			tpm_cmd.params.getcap_in.cap = TPM_CAP_FLAG;
		else
			tpm_cmd.params.getcap_in.cap = TPM_CAP_PROP;
		tpm_cmd.params.getcap_in.subcap_size = cpu_to_be32(4);
		tpm_cmd.params.getcap_in.subcap = subcap_id;
	}
	rc = td_tpm_transmit((u8 *)&tpm_cmd, TPM_INTERNAL_RESULT_SIZE);
	if (rc > 0)
		*cap = tpm_cmd.params.getcap_out.cap;
	return rc;
}

/**
 * check if TPM is enabled
 *
 * @return - -1, run td_tpm_getcap failed
 *         - 0,  FALSE is not enabled
 *         - 1,  TRUE is enabled
 */
int td_tpm_show_enabled(void)
{
	union cap_t cap;
	ssize_t rc;

	rc = td_tpm_getcap(TPM_CAP_FLAG_PERM, &cap);
	if (rc < 0) {
		k_debug("td_tpm_show_enabled, error, rc=%zi", rc);
		return -1;
	}

	k_debug("td_tpm_show_enabled, flag=0x%x\n",
		!cap.perm_flags.disable);
	return !cap.perm_flags.disable;
}

/**
 * test TPM_PERMANENT_FLAGS->deactivated to check if TPM is active
 *
 * @return - -1, error when read the cap
 *         - 0, FALSE, deactived
 *         - 1, TRUE, actived
 */
int td_tpm_show_active(void)
{
	union cap_t cap;
	ssize_t rc;

	rc = td_tpm_getcap(TPM_CAP_FLAG_PERM, &cap);
	if (rc < 0)
		return -1;

	k_debug("td_tpm_show_active, flag=0x%x\n",
		!cap.perm_flags.deactivated);
	return !cap.perm_flags.deactivated;
}

/**
 *
 * test stclear_flags.deactivated to check if TPM is active, but not reboot yet
 *
 * @return - -1 error when read the cap
 *         - 0  temp deactivated, need reboot
 *         - 1  activated.
 */
int td_tpm_show_tempactivate(void)
{
	union cap_t cap;
	ssize_t rc;
	rc = td_tpm_getcap(TPM_CAP_FLAG_VOL, &cap);
	if (rc < 0)
		return -1;
	k_debug("td_tpm_show_tempactivate, flag=%#x\n",
		!cap.stclear_flags.deactivated);
	return !cap.stclear_flags.deactivated;
}

int td_tpm_show_nvlock(void)
{
	union cap_t cap;
	ssize_t rc;

	rc = td_tpm_getcap(TPM_CAP_FLAG_PERM, &cap);
	if (rc < 0)
		return -1;

	k_debug("td_tpm_show_nvlock, flag=0x%x\n",
		!cap.perm_flags.nvLocked);
	return cap.perm_flags.nvLocked;
}

int td_tpm_show_owned(void)
{
	union cap_t cap;
	ssize_t rc;

	rc = td_tpm_getcap(TPM_CAP_PROP_OWNER, &cap);
	if (rc < 0)
		return -1;

	k_debug("td_tpm_show_owned, flag=0x%x\n", cap.owned);
	return cap.owned;
}

ssize_t td_tpm_write(u32 index, u32 offset, u8 *buf, size_t size)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	size_t in_size;

	k_func_enter();
	in_size = sizeof(tpm_cmd.header.in) + sizeof(index) +
		sizeof(offset) + sizeof(u32) + size;
	tpm_cmd.header.in = tpm_nvwrite_header;
	tpm_cmd.header.in.length = cpu_to_be32(in_size);
	tpm_cmd.params.nvwrite_in.index  = cpu_to_be32(index);
	tpm_cmd.params.nvwrite_in.offset = cpu_to_be32(offset);
	tpm_cmd.params.nvwrite_in.size   = cpu_to_be32(size);
	memcpy(tpm_cmd.params.nvwrite_in.buffer, buf, size);

	rc = td_tpm_transmit((u8 *)&tpm_cmd, in_size);

	k_func_leave();
	return rc;
}

ssize_t td_tpm_read(u32 index, u32 offset, u8 *buf, size_t size)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;

	k_func_enter();
	tpm_cmd.header.in = tpm_nvread_header;
	tpm_cmd.params.nvread_in.index  = cpu_to_be32(index);
	tpm_cmd.params.nvread_in.offset = cpu_to_be32(offset);
	tpm_cmd.params.nvread_in.size   = cpu_to_be32(size);

	rc = td_tpm_transmit((u8 *)&tpm_cmd, sizeof(tpm_cmd));
	if (rc > 0) {
		rc = be32_to_cpu(tpm_cmd.params.nvread_out.size);
		memcpy(buf, tpm_cmd.params.nvread_out.buffer, rc);
	}
	k_func_leave();

	return rc;
}

ssize_t td_tpm_physicalpresence(void)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	size_t in_size;

	k_func_enter();

	tpm_cmd.header.in = tpm_physicalpresence_header;
	tpm_cmd.params.physicalpresence_in.body = TPM_PHYSICAL_PRESENCE_PRESENT;
	in_size = sizeof(tpm_cmd.header.in) +
		sizeof(tpm_cmd.params.physicalpresence_in.body);
	rc = td_tpm_transmit((u8 *)&tpm_cmd, in_size);
	if (rc < 0)
		return -1;

	k_func_leave();

	return rc;
}

ssize_t td_tpm_continueselftest(void)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	size_t in_size;

	k_func_enter();
	tpm_cmd.header.in = tpm_continueselftest_header;

	in_size = sizeof(tpm_cmd.header.in);

	rc = td_tpm_transmit((u8 *)&tpm_cmd, in_size);
	if (rc < 0)
		return -1;

	k_func_leave();

	return rc;
}

ssize_t td_tpm_physicalenable(void)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	size_t in_size;

	k_func_enter();

	tpm_cmd.header.in = tpm_physicalenable_header;
	in_size = sizeof(tpm_cmd.header.in);
	rc = td_tpm_transmit((u8 *)&tpm_cmd, in_size);

	k_debug("td_tpm_physicalenable, rc, %d",
		be32_to_cpu(tpm_cmd.header.out.return_code));
	k_func_leave();

	return rc;
}

ssize_t td_tpm_lock_pp(void)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	size_t in_size;

	k_func_enter();

	tpm_cmd.header.in = tpm_physicalpresencelock_header;
	tpm_cmd.params.physicalpresence_in.body = TPM_PHYSICAL_PRESENCE_LOCK;
	in_size = sizeof(tpm_cmd.header.in) +
		sizeof(tpm_cmd.params.physicalpresence_in.body);
	rc = td_tpm_transmit((u8 *)&tpm_cmd, in_size);
	if (rc < 0)
		return -1;

	k_func_leave();
	return rc;
}

ssize_t  td_tpm_physicalsetdeactive(bool deactived)
{
	struct tpm_cmd_t tpm_cmd;
	int rc;
	size_t in_size;

	k_func_enter();

	tpm_cmd.header.in = tpm_physicalsetdeactive_header;
	tpm_cmd.params.physicalsetdeactive_in.body = deactived;
	in_size = sizeof(tpm_cmd.header.in) +
		sizeof(tpm_cmd.params.physicalsetdeactive_in.body);
	rc = td_tpm_transmit((u8 *)&tpm_cmd, in_size);
	if (rc < 0)
		return -1;

	/* after TPM is actived/deactived, system need reboot */
	kernel_restart(NULL);
	k_func_leave();
	return rc;
}
