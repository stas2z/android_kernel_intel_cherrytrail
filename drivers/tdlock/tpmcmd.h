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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program..
 */

#ifndef TD_LOCK_DRIVER_TPMCMD_H
#define TD_LOCK_DRIVER_TPMCMD_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>

#define TPM_INTERNAL_RESULT_SIZE		200

#define TPM_TAG_RQU_COMMAND			cpu_to_be16(0x00C1)

#define TPM_ORD_ContinueSelfTest		cpu_to_be32(0x00000053)
#define TPM_ORD_GetCapability			cpu_to_be32(0x00000065)
#define TPM_ORD_NV_ReadValue			cpu_to_be32(0x000000cf)
#define TPM_ORD_NV_WriteValue			cpu_to_be32(0x000000cd)
#define TPM_ORD_PhysicalEnable			cpu_to_be32(0x0000006f)
#define TPM_ORD_PhysicalSetDeactivated		cpu_to_be32(0x00000072)
#define TSC_ORD_PhysicalPresence		cpu_to_be32(0x4000000a)

#define TPM_PHYSICAL_PRESENCE_PRESENT  cpu_to_be16(0x0008)
#define TPM_PHYSICAL_PRESENCE_LOCK     cpu_to_be16(0x0004)

struct stclear_flags_t {
	__be16 tag;
	u8 deactivated;
	u8 disableForceClear;
	u8 physicalPresence;
	u8 physicalPresenceLock;
	u8 bGlobalLock;
} __packed;

struct tpm_version_t {
	u8 Major;
	u8 Minor;
	u8 revMajor;
	u8 revMinor;
} __packed;

struct tpm_version_1_2_t {
	__be16 tag;
	u8 Major;
	u8 Minor;
	u8 revMajor;
	u8 revMinor;
} __packed;

struct permanent_flags_t {
	__be16 tag;
	u8 disable;
	u8 ownership;
	u8 deactivated;
	u8 readPubek;
	u8 disableOwnerClear;
	u8 allowMaintenance;
	u8 physicalPresenceLifetimeLock;
	u8 physicalPresenceHWEnable;
	u8 physicalPresenceCMDEnable;
	u8 CEKPUsed;
	u8 TPMpost;
	u8 TPMpostLock;
	u8 FIPS;
	u8 operator;
	u8 enableRevokeEK;
	u8 nvLocked;
	u8 readSRKPub;
	u8 tpmEstablished;
	u8 maintenanceDone;
	u8 disableFullDALogicInfo;
} __packed;

struct timeout_t {
	__be32 a;
	__be32 b;
	__be32 c;
	__be32 d;
} __packed;

struct duration_t {
	__be32 tpm_short;
	__be32 tpm_medium;
	__be32 tpm_long;
} __packed;

union cap_t {
	struct permanent_flags_t perm_flags;
	struct stclear_flags_t stclear_flags;
	bool owned;
	__be32 num_pcrs;
	struct tpm_version_t tpm_version;
	struct tpm_version_1_2_t tpm_version_1_2;
	__be32 manufacturer_id;
	struct timeout_t  timeout;
	struct duration_t duration;
};

struct tpm_input_header {
	__be16 tag;
	__be32 length;
	__be32 ordinal;
} __packed;

struct tpm_output_header {
	__be16 tag;
	__be32 length;
	__be32 return_code;
} __packed;

union tpm_cmd_header {
	struct tpm_input_header in;
	struct tpm_output_header out;
};

struct tpm_getcap_params_in {
	__be32 cap;
	__be32 subcap_size;
	__be32 subcap;
} __packed;

struct tpm_getcap_params_out {
	__be32 cap_size;
	union cap_t cap;
} __packed;

struct tpm_nvread_params_in {
	__be32 index;
	__be32 offset;
	__be32 size;
} __packed;

#define TD_TPM_NV_MAX_SIZE 800
struct tpm_nvread_params_out {
	__be32 size;
	u8 buffer[TD_TPM_NV_MAX_SIZE];
} __packed;

struct tpm_nvwrite_params_in {
	__be32 index;
	__be32 offset;
	__be32 size;
	u8 buffer[TD_TPM_NV_MAX_SIZE];
};

struct tpm_physicalpresence_params_in {
	__be16 body;
} __packed;

struct tpm_physicalsetdeactive_params_in {
	u8  body;
} __packed;

union tpm_cmd_params {
	struct tpm_getcap_params_out getcap_out;
	struct tpm_getcap_params_in getcap_in;
	struct tpm_nvread_params_in nvread_in;
	struct tpm_nvread_params_out nvread_out;
	struct tpm_nvwrite_params_in nvwrite_in;
	struct tpm_physicalpresence_params_in physicalpresence_in;
	struct tpm_physicalsetdeactive_params_in physicalsetdeactive_in;
};

struct tpm_cmd_t {
	union tpm_cmd_header  header;
	union tpm_cmd_params  params;
} __packed;

#define TPM_DIGEST_SIZE		20
#define TPM_ERROR_SIZE		10
#define TPM_RET_CODE_IDX	6

enum tpm_capabilities {
	TPM_CAP_FLAG = cpu_to_be32(4),
	TPM_CAP_PROP = cpu_to_be32(5),
	CAP_VERSION_1_1 = cpu_to_be32(0x06),
	CAP_VERSION_1_2 = cpu_to_be32(0x1A)
};

enum tpm_sub_capabilities {
	TPM_CAP_PROP_PCR = cpu_to_be32(0x101),
	TPM_CAP_PROP_MANUFACTURER = cpu_to_be32(0x103),
	TPM_CAP_FLAG_PERM = cpu_to_be32(0x108),
	TPM_CAP_FLAG_VOL = cpu_to_be32(0x109),
	TPM_CAP_PROP_OWNER = cpu_to_be32(0x111),
	TPM_CAP_PROP_TIS_TIMEOUT = cpu_to_be32(0x115),
	TPM_CAP_PROP_TIS_DURATION = cpu_to_be32(0x120),

};

extern ssize_t td_tpm_getcap(__be32 subcap_id, union cap_t *cap);
extern int td_tpm_show_enabled(void);
extern int td_tpm_show_active(void);
/**
 *
 * test stclear_flags.deactivated to check if TPM is active, but not reboot yet
 *
 * @return - -1 error when read the cap
 *	   - 0  temp deactivated, need reboot
 *	   - 1  activated.
 */
extern int td_tpm_show_tempactivate(void);
extern int td_tpm_show_owned(void);
extern ssize_t td_tpm_write(u32 index, u32 offset, u8 *buf, size_t size);
extern ssize_t td_tpm_read(u32 index, u32 offset, u8 *buf, size_t size);
extern int td_tpm_show_nvlock(void);
extern ssize_t td_tpm_physicalpresence(void);
extern ssize_t td_tpm_lock_pp(void);
extern ssize_t td_tpm_continueselftest(void);
extern ssize_t td_tpm_physicalenable(void);

/**
 * physical set deactive TPM
 *
 * @param deactived - 0, active TPM
 *		    - 1, deactived TPM
 *
 * @return
 */
extern ssize_t td_tpm_physicalsetdeactive(bool deactived);

#endif

