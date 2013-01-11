/**
 *
 *  Copyright (C) 2012 - Aditya Patange
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Fixes reboot recovery, reboot download.
 *
 *  Adapted from sec_common.c in Samsung 2.6.35 Source. 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/err.h>
#include <linux/device.h>
#include <mach/hardware.h>
#include <plat/io.h>
#include <mach/board-latona.h>

/* Debug */
#define R_DEBUG 0

struct class *reboot_class;

char latona_androidboot_mode[16];
EXPORT_SYMBOL(latona_androidboot_mode);

static __init int setup_androidboot_mode(char *opt)
{
	strncpy(latona_androidboot_mode, opt, 15);
	return 0;
}

__setup("androidboot.mode=", setup_androidboot_mode);

u32 latona_bootmode;
EXPORT_SYMBOL(latona_bootmode);

static __init int setup_boot_mode(char *opt)
{
	latona_bootmode = (u32) memparse(opt, &opt);
	return 0;
}

__setup("bootmode=", setup_boot_mode);

struct latona_reboot_code {
	char *cmd;
	int mode;
};

static int __latona_reboot_call(struct notifier_block *this,
				    unsigned long code, void *cmd)
{
	int mode = REBOOT_MODE_NONE;
	int temp_mode;

	struct latona_reboot_code reboot_tbl[] = {
		{"arm11_fota", REBOOT_MODE_ARM11_FOTA},
		{"arm9_fota", REBOOT_MODE_ARM9_FOTA},
		{"recovery", REBOOT_MODE_RECOVERY},
		{"cp_crash", REBOOT_MODE_CP_CRASH},
	};
	size_t i, n;

	if ((code == SYS_RESTART) && cmd) {
		n = ARRAY_SIZE(reboot_tbl);
		for (i = 0; i < n; i++) {
			if (!strcmp((char *)cmd, reboot_tbl[i].cmd)) {
				mode = reboot_tbl[i].mode;
				break;
			}
		}
	}

	return NOTIFY_DONE;
}				/* end fn __latona_reboot_call */

static struct notifier_block __latona_reboot_notifier = {
	.notifier_call = __latona_reboot_call,
};

int __init latona_reboot_init(void)
{
#ifdef R_DEBUG
	printk("[%s]\n",__func__);
#endif
	reboot_class = class_create(THIS_MODULE, "latona_reboot");
	if (IS_ERR(reboot_class))
		pr_err("Class(latona_reboot) Creating Fail!!!\n");

	return 0;
}				/* end fn latona_reboot_init */

int __init latona_reboot_post_init(void)
{
#ifdef R_DEBUG
	printk("[%s]\n",__func__);
#endif
	register_reboot_notifier(&__latona_reboot_notifier);
	return 0;
}				/* end fn latona_reboot_post_init */

struct latona_reboot_mode {
	char *cmd;
	char mode;
};

static __inline char __latona_convert_reboot_mode(char mode,
						      const char *cmd)
{
	char new_mode = mode;
	struct latona_reboot_mode mode_tbl[] = {
		{"arm11_fota", 'f'},
		{"arm9_fota", 'f'},
		{"recovery", 'r'},
		{"download", 'd'},
		{"cp_crash", 'C'}
	};
	size_t i, n;
	if (cmd == NULL)
		goto __return;
	n = ARRAY_SIZE(mode_tbl);
	for (i = 0; i < n; i++) {
		if (!strcmp(cmd, mode_tbl[i].cmd)) {
			new_mode = mode_tbl[i].mode;
			goto __return;
		}
	}

__return:
	return new_mode;
}

#define LATONA_REBOOT_MODE_ADDR		(OMAP343X_CTRL_BASE + 0x0918)
#define LATONA_REBOOT_FLAG_ADDR		(OMAP343X_CTRL_BASE + 0x09C4)

int latona_update_reboot_reason(char mode, const char *cmd)
{
	u32 scpad = 0;
	const u32 scpad_addr = LATONA_REBOOT_MODE_ADDR;
	u32 reason = REBOOTMODE_NORMAL;
	char *szRebootFlag = "RSET";

	scpad = omap_readl(scpad_addr);

	omap_writel(*(u32 *)szRebootFlag, LATONA_REBOOT_FLAG_ADDR);
	
	/* for the compatibility with LSI chip-set based products */
	mode = __latona_convert_reboot_mode(mode, cmd);

	switch (mode) {
	case 'r':		/* reboot mode = recovery */
		reason = REBOOTMODE_RECOVERY;
		break;
	case 'f':		/* reboot mode = fota */
		reason = REBOOTMODE_FOTA;
		break;
	case 'L':		/* reboot mode = Lockup */
		reason = REBOOTMODE_KERNEL_PANIC;
		break;
	case 'F':
		reason = REBOOTMODE_FORCED_UPLOAD;
		break;
	case 'U':		/* reboot mode = Lockup */
		reason = REBOOTMODE_USER_PANIC;
		break;
	case 'C':		/* reboot mode = Lockup */
		reason = REBOOTMODE_CP_CRASH;
		if (!strcmp(cmd, "Checkin scheduled forced"))
			reason = REBOOTMODE_NORMAL;
		break;
	case 't':		/* reboot mode = shutdown with TA */
	case 'u':		/* reboot mode = shutdown with USB */
	case 'j':		/* reboot mode = shutdown with JIG */
		reason = REBOOTMODE_SHUTDOWN;
		break;
	case 'd':		/* reboot mode = download */
		reason = REBOOTMODE_DOWNLOAD;
		break;
	default:		/* reboot mode = normal */
		reason = REBOOTMODE_NORMAL;
		break;
	}

	omap_writel(scpad | reason, scpad_addr);
	
	return (int)mode;
}				
