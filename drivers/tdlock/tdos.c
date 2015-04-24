/*
 * TDOS functionalities for threft deterrent lock driver.
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
 * along with this program.
 */

#include <linux/kernel.h>
#include <linux/efi.h>
#include <linux/nls.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/reboot.h>
#include <linux/fs.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/sched.h>

#include "tdos.h"
#include "util.h"

#define REBOOT_REASON_LENGTH  32

static const char *TDOS_REBOOT_REASON = "tdos";

static const efi_guid_t OS_LOADER_GUID = EFI_GUID(0x4a67b082,
						  0x0a4c, 0x41cf,
						  0xb6, 0xc7, 0x44,
						  0x0b, 0x29, 0xbb,
						  0x8c, 0x4f);

static const char TARGET_VARNAME_ONESHOT[] = "LoaderEntryOneShot";
static const char TARGET_VARNAME_LAST[] = "LoaderEntryLast";

static struct efivar_entry *td_uefi_get_var_entry(wchar_t *varname)
{
	struct efivar_entry *entry;

	efivar_entry_iter_begin();
	entry = efivar_entry_find(varname, OS_LOADER_GUID,
				  &efivar_sysfs_list, false);
	efivar_entry_iter_end();

	return entry;
}

bool td_is_tdos(void)
{
	bool is_tdos;
	wchar_t *varname;
	wchar_t *name;
	char *reason;
	size_t name_size;
	u32 attributes;
	struct efivar_entry *entry;
	int err;

	k_func_enter();

	varname = kzalloc(sizeof(wchar_t) * sizeof(TARGET_VARNAME_LAST),
			  GFP_KERNEL);
	if (!varname) {
		k_err("TD_OS: memory alloc failed!\n");
		return false;
	}

	utf8s_to_utf16s(TARGET_VARNAME_LAST,
			sizeof(TARGET_VARNAME_LAST) - 1,
			UTF16_LITTLE_ENDIAN,
			varname,
			sizeof(*varname));

	name_size = sizeof(wchar_t) * (REBOOT_REASON_LENGTH + 1);

	name = kzalloc(name_size, GFP_KERNEL);
	if (!name) {
		kzfree(varname);
		k_err("TD_OS: memory alloc failed!\n");
		return false;
	}

	reason = kzalloc(REBOOT_REASON_LENGTH + 1, GFP_KERNEL);
	if (!reason) {
		kzfree(name);
		kzfree(varname);
		k_err("TD_OS: memory alloc failed!\n");
		return false;
	}

	entry = td_uefi_get_var_entry(varname);
	if (entry == NULL)
		k_err("TD_OS: Find efivar entry failed.\n");

	err = efivar_entry_get(entry, &attributes, &name_size, name);
	if (err < 0) {
		k_err("TD_OS: Get efivar entry failed, err = %d\n", err);
		is_tdos = false;
		goto end;
	}

	utf16s_to_utf8s(name,
			name_size / sizeof(wchar_t) - 1,
			UTF16_LITTLE_ENDIAN,
			reason,
			REBOOT_REASON_LENGTH);

	k_debug("TD_OS: reboot reason = %s\n", reason);
	is_tdos = (strcmp(reason, TDOS_REBOOT_REASON) == 0);
	k_debug("TD_OS: is_tdos = %s\n", is_tdos ? "yes" : "no");

end:
	kzfree(reason);
	kzfree(name);
	kzfree(varname);
	k_func_leave();
	return is_tdos;
}

static void td_migrate_to_reboot_cpu(void)
{
	/* The boot cpu is always logical cpu 0*/
	int cpu;

	cpu = 0;
	cpu_hotplug_disable();

	/* Make certain the cpu I'm about to reboot on is online*/
	if (!cpu_online(cpu))
		cpu = cpumask_first(cpu_online_mask);

	/* Prevent races with other tasks migrating this task*/
	current->flags |= PF_NO_SETAFFINITY;

	/* Make certain I only run on the appropriate processor*/
	set_cpus_allowed_ptr(current, cpumask_of(cpu));
}

static void td_restart(char *cmd)
{
	emergency_sync();
	emergency_remount();

	blocking_notifier_call_chain(&reboot_notifier_list, SYS_RESTART, cmd);
	device_shutdown();
	td_migrate_to_reboot_cpu();
	syscore_shutdown();

	if (!cmd)
		pr_emerg("Restarting system.\n");
	else
		pr_emerg("Restarting system with command '%s'.\n", cmd);

	machine_restart(cmd);
}

void td_reboot_to_tdos(void)
{
	k_info("reboot into tdos!");
	td_restart("tdos");
}
EXPORT_SYMBOL_GPL(td_reboot_to_tdos);

void td_reboot_to_aos(void)
{
	k_info("reboot back to android OS!");
	td_restart("android");
}
EXPORT_SYMBOL_GPL(td_reboot_to_aos);

