/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _MEMX_FILE_SYSTEM_H_
#define _MEMX_FILE_SYSTEM_H_

#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/init.h>	  // included for __init and __exit macros
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include "../include/memx_ioctl.h"
#include "memx_cascade_usb.h"

#define MEMX_MAX_FS_SYS_CMD_NAME_LEN (8)

struct memx_data;

struct memx_fs_cmd_cb {
	char name[MEMX_MAX_FS_SYS_CMD_NAME_LEN];
	u32 expected_argc;
	s32 (*exec)(struct memx_data *memx_dev, u8 argc, char **argv);
};

enum memx_fs_cmd_cb_argc {
	FS_CMD_FWLOG_ARGC = 2,
	FS_CMD_READ_ARGC  = 3,
	FS_CMD_WRITE_ARGC = 4,

	FS_CMD_MEMX0_ARGC = 0x12,
	FS_CMD_MEMX1_ARGC = 0x22,
	FS_CMD_MEMX2_ARGC = 0x32,
	FS_CMD_MEMX3_ARGC = 0x42,
	FS_CMD_MEMX4_ARGC = 0x52,
	FS_CMD_MEMX5_ARGC = 0x62,
	FS_CMD_MEMX6_ARGC = 0x72,
	FS_CMD_MEMX7_ARGC = 0x82,
	FS_CMD_MEMX8_ARGC = 0x92,
	FS_CMD_RMTCMD_ARGC = 0x14,

	MAX_FS_CMD_ARGC_NUM = FS_CMD_WRITE_ARGC,
};


s32 memx_fs_init(struct memx_data *memx_dev);
void memx_fs_deinit(struct memx_data *memx_dev);

s32 memx_fs_cmd_handler(struct memx_data *memx_dev, u8 argc, char **argv);
s32 memx_fs_parse_cmd_and_exec(struct memx_data *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size);
s32 memx_fs_parse_i2ctrl_and_exec(struct memx_data *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size);
s32 memx_fs_parse_gpioctrl_and_exec(struct memx_data *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size);
#endif
