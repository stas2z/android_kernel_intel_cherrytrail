/*
 * Threft Deterrent Driver main entry.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/reboot.h>

#include "util.h"		/* k_debug, k_error */
#include "tpmcmd.h"
#include "sclock.h"		/* td_register_secure_clock_file */
#include "mei.h"		/* td_mei_is_td_supported(), td_mei_get_sc() */
#include "tdos.h"		/* td_is_tdos */

#define TD_BC_NV_INDEX		0x10001003
#define TD_BC_NV_SIZE		24

#define TD_PP_NV_INDEX		0x10001004
#define TD_PP_NV_SIZE		541
#define UNLOCK_CODE_LENGTH	10

#define TD_TICKS_PER_YEAR	     (60 * 60 * 24 * 365)

struct td_bc_nv_t {
	u32  bt;
	u32  rsc;
	u32  ed;
	u8  hwid[12];
};

/*
 * Judge whether TD is support in this platform.
 */
static bool td_is_supported(void)
{
	bool is_supported;
	if (td_mei_is_td_supported(&is_supported))
		return is_supported;
	return false;
}

/*
 * Judge whether TPM is provisioned in manufactory
 * return:  if < 0, TPM error
 *	    if = 0, NV is not locked
 *	    if > 0, NV is locked.
 */
static int td_check_nvlock(void)
{
	int rc;

	rc = td_tpm_show_nvlock();

	return rc;
}

static bool td_check_bc_valid(void)
{
	int rc;
	u8 buffer[TD_BC_NV_SIZE] = {0};
	u8 buffer_invalid[TD_BC_NV_SIZE] = {0};

	rc = td_tpm_read(TD_BC_NV_INDEX, 0, buffer, TD_BC_NV_SIZE);
	if (rc < 0) {
		k_err("Fail to read TPM BC NV!\n");
		return false;
	}

	if (memcmp(buffer, buffer_invalid, TD_BC_NV_SIZE) == 0)
		return false;

	memset(buffer_invalid, 0xFF, TD_BC_NV_SIZE);
	/*
	 * Check if RSC and ED are FF, if yes,
	 * that means TPM is not provisioned.
	 * RSC is stored from byte 4 - 7, ED is
	 * stored from byte 8 - 11. Totally 8 bytes.
	 */
	if (memcmp(buffer + 4, buffer_invalid, 8) == 0)
		return false;

	return true;
}

static bool td_check_trust_clock(void)
{
	u32 sclock;
	return td_mei_get_sc(&sclock);
}

/*
 * Check whether boot certificate is expired.
 * return:  if < 0, TPM error
 *	    if > 0, expired
 *	    if = 0, not expired
 */
static int td_check_bc_expired(void)
{
	int rc;
	u32 curr;
	struct td_bc_nv_t *bc_nv;
	u8 buffer[TD_BC_NV_SIZE] = {0};

	rc = td_tpm_read(TD_BC_NV_INDEX, 0, buffer, TD_BC_NV_SIZE);
	if (rc < 0) {
		k_err("Fail to read TPM BC NV!\n");
		goto out;
	}

	bc_nv = (struct td_bc_nv_t *)buffer;

	td_mei_get_sc(&curr);

	/*When use permanent CT, should always return 0(never expired).*/
	if ((bc_nv->ed - bc_nv->rsc) > 50 * TD_TICKS_PER_YEAR) {
		if (curr < bc_nv->rsc)
			bc_nv->ed = curr + (bc_nv->ed - bc_nv->rsc);
		bc_nv->rsc = curr;
		rc = td_tpm_write(TD_BC_NV_INDEX,
				  0, (u8 *)bc_nv, TD_BC_NV_SIZE);
		if (rc < 0) {
			k_err("Fail to save update BC!\n");
			goto out;
		}
	} else {
		/* if valid range is 0, that means devices has been locked. */
		if (bc_nv->rsc == bc_nv->ed) {
			rc = 1;
			goto out;
		}
		/* give user one chance if curr < bc_nv->rsc */
		if (curr < bc_nv->rsc) {
			bc_nv->rsc = curr;
			bc_nv->ed = curr;
			rc = td_tpm_write(TD_BC_NV_INDEX,
					  0, (u8 *)bc_nv, TD_BC_NV_SIZE);
			if (rc < 0) {
				k_err("Fail to save update BC!\n");
				goto out;
			}
			rc = 0;
			goto out;
		}
		if (curr > bc_nv->ed) {
			/* set valid range to 0 if device is expired. */
			bc_nv->rsc = bc_nv->ed;
			rc = td_tpm_write(TD_BC_NV_INDEX,
					  0, (u8 *)bc_nv, TD_BC_NV_SIZE);
			if (rc < 0) {
				k_err("Fail to save update BC!\n");
				goto out;
			}
			rc = 1;
			goto out;
		}

		/* if not expired, update the rsc as current SRTC. */
		bc_nv->rsc = curr;
		rc = td_tpm_write(TD_BC_NV_INDEX,
				  0, (u8 *)bc_nv, TD_BC_NV_SIZE);
		if (rc < 0) {
			k_err("Fail to save update BC!\n");
			goto out;
		}
	}
	rc = 0;
out:
	return rc;
}

/*
 * Check whether provision packets from TD server is put into TPM
 * temporary storage.
 * return: < 0 TPM error
 *	   = 0 no provision packet
 *	   > 0 has provision packet
 */
static int td_has_provision_packet(void)
{
	u8 buffer[TD_PP_NV_SIZE] = {0};
	int rc;
	int i, j;

	rc = td_tpm_read(TD_PP_NV_INDEX, 0, buffer, TD_PP_NV_SIZE);
	if (rc < 0) {
		k_err("Fail to read TPM PP NV!\n");
		goto out;
	}
	/*
	 * Unlock code: Type(1 Byte) + Code(10 Byte),
	 * we need to judge wether these 11Byte value are
	 * all 0 or 0xFF to know there is no provision
	 * packet. In factory deployment stage, the forth
	 * patition will be all F but not 0.
	 */
	for (i = 0; i < UNLOCK_CODE_LENGTH + 1; i++) {
		if ((buffer[i] != 0) && (buffer[i] != 0xFF)) {
			goto out;
		} else if (buffer[i] == 0) {
			/* for the case: the 11Byte are not all
			 * '00', "...00 00 FF ..."*/
			for (j = 0; (j != i) &&
			     (j <= UNLOCK_CODE_LENGTH + 1); j++) {
				if (buffer[j] != 0)
					goto out;
			}
		} else if (buffer[i] == 0xFF) {
			/* for the case: the 11Byte are not all
			 *'FF', "...FF FF 00 ..."*/
			for (j = 0; (j != i) &&
			     (j <= UNLOCK_CODE_LENGTH + 1); j++) {
				if (buffer[j] != 0xFF)
					goto out;
			}
		}
	}
	return 0;
out:
	return rc;
}

/*
 * Lock security storage: TPM NV 1,2,3
 */
static void td_tpm_lock_nv_index(void)
{
	td_tpm_read(0x10001001, 0, NULL, 0);
	td_tpm_write(0x10001001, 0, NULL, 0);
	td_tpm_write(0x10001002, 0, NULL, 0);
	td_tpm_write(0x10001003, 0, NULL, 0);
}

/*
 * Main entry for TD lock driver
 */
static int __init td_lock_driver_init(void)
{
	int rc;
	k_func_enter();

	/*
	 * Register proc file for reading secure clock via IPC
	 */
	td_register_secure_clock_file();
	td_register_reboot_file();

	/*
	 * check whether the platform support TD feature by judging the flag
	 * in UMIP header.
	 */
	if (!td_is_supported()) {
		/*
		 * If platform did not support TD but mini-OS is entered, kick
		 * user back to user OS but notcontinue mini-OS booting.
		 */
		if (td_is_tdos()) {
			k_warn("Current platform not support TD\n");
			goto BACK_TO_AOS;
		}
		k_warn("Current platform does not support TD feature.\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * If current boot is TD OS, bypass all lock checking path.
	 */
	if (td_is_tdos()) {
		k_info("I am TD OS!\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * Select /dev/tpm0 as default TPM chip.
	 */
	td_tpm_select_chip_num(0);

	/*
	 * Check whether TD data is provisioned, if not, continue boot.
	 */
	rc = td_check_nvlock();
	if (rc < 0) {
		k_err("TPM error when read nv lock flag!\n");
		goto ERROR_TO_MINIOS;
	}
	if (rc == 0) {
		k_warn("TPM has not been initialized in manufactory!\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * Check whether secure clock is correct, if incorrect, reboot to TD OS
	 */
	if (!td_check_trust_clock()) {
		k_err("Trust clock does not work! TD lock driver failed!\n");
		goto ERROR_TO_MINIOS;
	}

	if (td_tpm_physicalpresence() < 0) {
		k_err("Fail to indicate the assertion of physical presence.\n");
		goto ERROR_TO_MINIOS;
	}

	td_tpm_continueselftest();

	/* Check whether TPM is enabled. */
	if (td_tpm_show_enabled() == 0) {
		/* Check whether PP is enabled which is used to enable TPM */
		if (td_tpm_physicalenable() < 0)
			goto ERROR_TO_MINIOS;
	}

	/* Check whether TPM is acivated */
	if (td_tpm_show_active() > 0) {
		rc = td_tpm_show_tempactivate();
		if (rc < 0) {
			k_err("Fail to get stclear.deactivated.\n");
			goto ERROR_TO_MINIOS;
		} else if (rc == 0) {
			/*
			 * If TPM is under temp activate status,
			 * a cold reset is need to bring TPM back to active
			 * status.
			 */
			kernel_power_off();
		} else {
			/* Lock TPM pp interface. */
			td_tpm_lock_pp();
		}
	} else {
		/* Active TPM */
		td_tpm_physicalsetdeactive(0);
	}

	/*
	 * If TPM is disable or deactived, reboot to mini-OS.
	 */
	if (!((td_tpm_show_enabled() > 0) && (td_tpm_show_active() > 0))) {
		k_warn("TPM is not enabled! TD driver init failed!\n");
		goto ERROR_TO_MINIOS;
	}

	/*
	 * If TPM owner has not been taken or boot certificate is invalid,
	 * bypass TD logic and continue boot.
	 */
	rc = td_tpm_show_owned();
	if (rc < 0)
		goto ERROR_TO_MINIOS;

	if ((td_tpm_show_owned() == 0) ||
	    ((td_tpm_show_owned() > 0) && !td_check_bc_valid())) {
		k_debug("TPM owner is not taken or no valid TD data in TPM!\n");
		goto CONTINUE_BOOT;
	}

	/*
	 * If server provision packet is downloaded into TPM temporary storage,
	 *  reboot into mini-OS
	 */
	if (td_has_provision_packet() != 0) {
		/*
		 * if < 0, TPM error
		 * if > 0, has provision packet
		 */
		goto ERROR_TO_MINIOS;
	}

	/*
	 * If boot certiticate is expired, boot into TD OS
	 */
	rc = td_check_bc_expired();
	if (rc < 0) {
		k_err("Fail to check BC expired!");
		goto ERROR_TO_MINIOS;
	} else if (rc > 0) {
		k_debug("Boot certificate is expired, go to TD OS...\n");
		goto ERROR_TO_MINIOS;
	} else {
		td_tpm_lock_nv_index();
	}

CONTINUE_BOOT:
	k_debug("Success pass TD checking, continue boot!");
	k_func_leave();
	return 0;

BACK_TO_AOS:
	td_reboot_to_aos();

ERROR_TO_MINIOS:
	td_reboot_to_tdos();
	k_func_leave();
	return 0;
}

late_initcall(td_lock_driver_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TD lock kernel built-in driver");
