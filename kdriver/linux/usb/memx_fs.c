// SPDX-License-Identifier: GPL-2.0+
#include <linux/fs.h>
#include <linux/namei.h>

#include "memx_fs.h"
#include "memx_fs_proc.h"
#include "memx_fs_sys.h"
#include "memx_fs_hwmon.h"
#include "memx_fw_log.h"
#include "memx_cascade_debugfs.h"

#define MEMX_NUM_OF_FS_CMDS (sizeof(g_fs_cmd_tbl) / sizeof(struct memx_fs_cmd_cb))

static struct memx_fs_cmd_cb g_fs_cmd_tbl[] = {
	{ "fwlog", FS_CMD_FWLOG_ARGC, memx_fs_cmd_handler},
	{ "read",  FS_CMD_READ_ARGC,  memx_fs_cmd_handler},
	{ "write", FS_CMD_WRITE_ARGC, memx_fs_cmd_handler},
	{ "memx0", FS_CMD_MEMX0_ARGC, memx_fs_cmd_handler},
	{ "memx1", FS_CMD_MEMX1_ARGC, memx_fs_cmd_handler},
	{ "memx2", FS_CMD_MEMX2_ARGC, memx_fs_cmd_handler},
	{ "memx3", FS_CMD_MEMX3_ARGC, memx_fs_cmd_handler},
	{ "memx6", FS_CMD_MEMX6_ARGC, memx_fs_cmd_handler},
	{ "memx7", FS_CMD_MEMX7_ARGC, memx_fs_cmd_handler},
	{ "memx8", FS_CMD_MEMX8_ARGC, memx_fs_cmd_handler},
	{ "rmtcmd", FS_CMD_RMTCMD_ARGC, memx_fs_cmd_handler},
};

s32 memx_fs_cmd_handler(struct memx_data *memx_dev, u8 argc, char **argv)
{
	u8 chip_id  = 0;
	u32 reg_addr = 0, reg_value = 0;
	//bool is_access_mpu = false;

	if (!memx_dev) {
		pr_err("fs_cmd_handler: memx_dev is NULL\n");
		return -EINVAL;
	}
	if (argc < FS_CMD_FWLOG_ARGC) {
		pr_err("fs_cmd_handler: invalid argc(%u), should be 2-4\n", argc);
		return -EINVAL;
	}
	if (!argv) {
		pr_err("fs_cmd_handler: argv is NULL\n");
		return -EINVAL;
	}

	if (argc >= FS_CMD_FWLOG_ARGC &&
		kstrtou8(argv[FS_CMD_FWLOG_ARGC - 1], 0, &chip_id) &&
		chip_id >= memx_dev->chipcnt) {
		pr_err("fs_cmd_handler: invalid chip id(%u), should be 0 - %u\n", chip_id, memx_dev->chipcnt);
		return -EINVAL;
	}

	if (((argc == FS_CMD_READ_ARGC) || (argc == FS_CMD_WRITE_ARGC) || (argc == FS_CMD_RMTCMD_ARGC)) && kstrtou32(argv[FS_CMD_READ_ARGC - 1], 0, &reg_addr)) {
		pr_err("fs_cmd_handler: invalid reg_addr:%s\n", argv[2]);
		return -EINVAL;
	}

	if (((argc == FS_CMD_WRITE_ARGC) || (argc == FS_CMD_RMTCMD_ARGC)) && kstrtou32(argv[FS_CMD_WRITE_ARGC - 1], 0, &reg_value)) {
		pr_err("fs_cmd_handler: invalid reg_value:%s\n", argv[3]);
		return -EINVAL;
	}

	switch (argc) {
	case FS_CMD_FWLOG_ARGC: {
		s32 ret = memx_fw_log_dump(memx_dev, chip_id);
		//pr_err("FS_CMD_FWLOG_ARGC\n");
		if (ret) {
			pr_err("fs_cmd_handler: try to dump chip_id(%u)'s fw_log fail(%d)\n", chip_id, ret);
			return ret;
		}
		pr_info("fs_cmd_handler: dump of chip_id(%u)'s fw_log success\n", chip_id);
	} break;
	case FS_CMD_READ_ARGC: {
		//pr_err("FS_CMD_READ_ARGC\n");
		memx_send_memxcmd(memx_dev, chip_id, MXCNST_MEMXR_CMD, reg_addr, 0);
	} break;
	case FS_CMD_WRITE_ARGC: {
		//pr_err("FS_CMD_WRITE_ARGC\n");
		memx_send_memxcmd(memx_dev, chip_id, MXCNST_MEMXW_CMD, reg_addr, reg_value);
	} break;

	case FS_CMD_MEMX0_ARGC:
	case FS_CMD_MEMX1_ARGC:
	case FS_CMD_MEMX2_ARGC:
	case FS_CMD_MEMX3_ARGC:
	case FS_CMD_MEMX6_ARGC:
	case FS_CMD_MEMX7_ARGC:
	case FS_CMD_MEMX8_ARGC: {
		u32 cmdbase = MXCNST_MEMX0_CMD;

		cmdbase = cmdbase + ((argc>>4) - (FS_CMD_MEMX0_ARGC>>4));
		//pr_err("FS_CMD_MEMX_ARGC 0x%08X\n", cmdbase);
		memx_send_memxcmd(memx_dev, chip_id, cmdbase, 0, 0);
	} break;
	case FS_CMD_RMTCMD_ARGC: {
		//pr_err("FS_CMD_RMTCMD_ARGC 0x%08X\n", reg_addr);
		memx_send_memxcmd(memx_dev, chip_id, reg_addr, reg_value, 0);
	} break;

	default: {
		pr_err("fs_cmd_handler: expected argc[2-4] but (%u)\n", argc);
		return -EINVAL;
	}
	}
	return 0;
}

s32 memx_fs_parse_cmd_and_exec(struct memx_data *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size)
{
	s32 ret = -EINVAL;
	char *found = NULL;
	const char delimiters[] = {' ', '\0'}; // space, \0
	char *argv[4] = {NULL, NULL, NULL, NULL}; // cmd_name, chip_id, addr, val
	u8 argc = 0;
	u8 cmd_idx = 0;
	char *input_parser_buffer_ptr = NULL;

	if (memx_dev->fs.type == MEMX_FS_HIF_SYS) {
		input_parser_buffer_ptr = kstrdup(user_input_buf, GFP_KERNEL);
		if (!input_parser_buffer_ptr) {
			//pr_err("memx_fs_parser: kstrdup fail!\n");
			return ret;
		}
	} else {
		input_parser_buffer_ptr = memdup_user_nul(user_input_buf, user_input_buf_size);
		if (IS_ERR(input_parser_buffer_ptr)) {
			pr_err("memx_fs_parser: memdup_user_nul fail!\n");
			return PTR_ERR(input_parser_buffer_ptr);
		}
	}

	// parse command string by deleimiters and get the final argc
	while ((found = strsep(&input_parser_buffer_ptr, delimiters)) != NULL)
		argv[argc++] = found;

	if (argc < FS_CMD_FWLOG_ARGC) {
		pr_err("memx_fs_parser: argc should be te range in 2-4 but (%u)\n", argc);
		return ret;
	}


	// check argc and name meet requirement
	for (cmd_idx = 0; cmd_idx < MEMX_NUM_OF_FS_CMDS; cmd_idx++) {
		if ((argc == (g_fs_cmd_tbl[cmd_idx].expected_argc & 0xF)) &&
			(strncmp(argv[0], g_fs_cmd_tbl[cmd_idx].name, strlen(g_fs_cmd_tbl[cmd_idx].name)) == 0)) {
			ret = g_fs_cmd_tbl[cmd_idx].exec(memx_dev, g_fs_cmd_tbl[cmd_idx].expected_argc, &argv[0]);
			break;
		}
	}

	kfree(input_parser_buffer_ptr);
	return ret;
}

extern int memx_admin_trigger(struct transport_cmd *pCmd, struct memx_data *data);
s32 memx_fs_parse_i2ctrl_and_exec(struct memx_data *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size)
{
	s32 ret = -EINVAL;
	char *found = NULL;
	const char delimiters[] = {' ', '\0'}; // space, \0
	// [w-byte-cnt], wdata0, wdata1,..., [r-byte-cnt]
	u8 max_len = 17;
	char *argv[17] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	u8 argc = 0;
	char *input_parser_buffer_ptr = NULL;
	u32 wlen = 0, data, i;
	struct transport_cmd cmd = {0};
	u8 *pbuf8;

	if (memx_dev->fs.type == MEMX_FS_HIF_SYS) {
		input_parser_buffer_ptr = kstrdup(user_input_buf, GFP_KERNEL);
		if (!input_parser_buffer_ptr) {
			//pr_err("memx_fs_parser: kstrdup fail!\n");
			return ret;
		}
	} else {
		input_parser_buffer_ptr = memdup_user_nul(user_input_buf, user_input_buf_size);
		if (IS_ERR(input_parser_buffer_ptr)) {
			pr_err("memx_fs_parser: memdup_user_nul fail!\n");
			return PTR_ERR(input_parser_buffer_ptr);
		}
	}

	// parse command string by deleimiters and get the final argc
	while ((found = strsep(&input_parser_buffer_ptr, delimiters)) != NULL) {
		if (argc < max_len) {
			argv[argc++] = found;
			//pr_info("argv[%d]=%s\n", argc-1, argv[argc-1]);
		} else {
			argc++;
		}
	}

	if ((argc < FS_CMD_FWLOG_ARGC) || (argc > max_len)) {
		pr_err("memx_i2ctrl_parser: argc should be te range in 2-%d but (%u)\n", max_len, argc);
		return ret;
	}

	if (kstrtou32(argv[0], 0, &wlen)) {
		pr_err("kstrtou32 convert error 1\n");
		return ret;
	}

	if (wlen > max_len-1) {
		pr_err("memx_i2ctrl_parser: max write len is %d (%u)\n", max_len-1, wlen);
		return ret;
	}
	//pr_info("len=%u\r\n", wlen);
	wlen = wlen&0xFE;

	cmd.SQ.opCode = MEMX_ADMIN_CMD_SET_FEATURE;
	cmd.SQ.subOpCode = FID_DEVICE_I2C_TRANSCEIVE;
	cmd.SQ.reqLen = wlen>>1;
	pbuf8 = (u8 *) &(cmd.SQ.cdw3);
	for (i = 0; i < wlen; i++) {
		if (!kstrtou32(argv[i+1], 0, &data))
			pbuf8[i] = data & 0xFF;
	}

	mutex_lock(&memx_dev->cfglock);
	memx_admin_trigger(&cmd, memx_dev);
	mutex_unlock(&memx_dev->cfglock);
	
	ret = memx_read_chip0(memx_dev, (uint32_t *)&cmd, 0x400FD200, sizeof(struct transport_cmd));
	if (!ret) {
		pbuf8 = (u8 *) &(cmd.SQ.cdw3);
		pr_info("--------------------\n");
		for (i = 0; i < (wlen >> 1); i++) {
			if (pbuf8[(i<<1)+1] & 0x10)
				pr_info("R data[%d]=0x%02X - OK\n", i, pbuf8[(i<<1)]);
			else if (pbuf8[(i<<1)+1] & 0x4)
				pr_info("W data[%d]=0x%02X - I2C_NACK\n", i, pbuf8[(i<<1)]);
			else
				pr_info("W data[%d]=0x%02X - OK\n", i, pbuf8[(i<<1)]);
		}
	}

	kfree(input_parser_buffer_ptr);
	return 0;
}

s32 memx_fs_parse_gpioctrl_and_exec(struct memx_data *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size)
{
	s32 ret = -EINVAL;
	char *found = NULL;
	const char delimiters[] = {' ', '\0'}; // space, \0
	u8 max_len = 3;
	char *argv[3] = {NULL, NULL, NULL};
	u8 argc = 0;
	char *input_parser_buffer_ptr = NULL;
	struct transport_cmd cmd = {0};
	u32 gpio_number = 0, gpio_value = 0;

	if (memx_dev->fs.type == MEMX_FS_HIF_SYS) {
		input_parser_buffer_ptr = kstrdup(user_input_buf, GFP_KERNEL);
		if (!input_parser_buffer_ptr) {
			//pr_err("memx_fs_parser: kstrdup fail!\n");
			return ret;
		}
	} else {
		input_parser_buffer_ptr = memdup_user_nul(user_input_buf, user_input_buf_size);
		if (IS_ERR(input_parser_buffer_ptr)) {
			pr_err("memx_fs_parser: memdup_user_nul fail!\n");
			return PTR_ERR(input_parser_buffer_ptr);
		}
	}

	// parse command string by deleimiters and get the final argc
	while ((found = strsep(&input_parser_buffer_ptr, delimiters)) != NULL) {
		if (argc < max_len) {
			argv[argc++] = found;
			//pr_info("argv[%d]=%s\n", argc-1, argv[argc-1]);
		} else {
			argc++;
		}
	}

	if ((argc < FS_CMD_FWLOG_ARGC) || (argc > max_len)) {
		pr_err("memx_i2ctrl_parser: argc should be te range in 2-%d but (%u)\n", max_len, argc);
		return ret;
	}

	if (kstrtou32(argv[1], 0, &gpio_number)) {
		pr_err("%s: kstrtou32 convert error\n", __func__);
		return ret;
	}
	gpio_number = gpio_number & 0xFFF;

	if (strncmp(argv[0], "r", 1) == 0) {
		memx_dev->gpio_r = gpio_number;
		return 0;
	} else if ((strncmp(argv[0], "w", 1) == 0) && (argc == 3)) {
		if (kstrtou32(argv[2], 0, &gpio_value)) {
			pr_err("%s: kstrtou32 convert error\n", __func__);
			return ret;
		}
	} else {
		pr_err("%s: command error (%s)(%d)\n", __func__, argv[0], argc);
		return ret;
	}

	cmd.SQ.opCode = MEMX_ADMIN_CMD_SET_FEATURE;
	cmd.SQ.subOpCode = FID_DEVICE_GPIO;
	cmd.SQ.cdw2      = (gpio_number >> 8) & 0xF; // chip id
	cmd.SQ.cdw3      = (gpio_number >> 0) & 0xFF;
	cmd.SQ.cdw4      = gpio_value > 0;

	mutex_lock(&memx_dev->cfglock);
	memx_admin_trigger(&cmd, memx_dev);
	mutex_unlock(&memx_dev->cfglock);

	kfree(input_parser_buffer_ptr);
	return 0;
}

s32 memx_fs_init(struct memx_data *memx_dev)
{
	s32 ret = -EINVAL;
	int minor = 0;

	if (!memx_dev) {
		pr_err("%s: memx_dev is NULL\n", __func__);
		return ret;
	}

	/* Decide minor number by file open */

	switch (memx_dev->fs.type) {
	case MEMX_FS_HIF_PROC: {
#ifndef ANDROID
		char name[128];
		struct path path;

		for (minor = 0; minor < 128; minor++) {
			sprintf(name, "/proc/memx%d/cmd", minor);

			if (kern_path(name, LOOKUP_FOLLOW, &path)) {
				pr_info("memryx: register for %s\n", name);
				memx_dev->minor_index = minor;
				break;

			} else {
				path_put(&path);
			}
		}
#endif
		ret = memx_fs_proc_init(memx_dev);
	} break;

	case MEMX_FS_HIF_SYS: {
#ifndef ANDROID
		char name[128];
		struct path path;

		for (minor = 0; minor < 128; minor++) {
			sprintf(name, "/sys/memx%d/cmd", minor);

			if (kern_path(name, LOOKUP_FOLLOW, &path)) {
				pr_info("memryx: register for %s\n", name);
				memx_dev->minor_index = minor;
				break;

			} else {
				path_put(&path);
			}
		}
#endif
		ret = memx_fs_sys_init(memx_dev);
	} break;

	default:
		pr_err("%s: non support file system type(%u)\n", __func__, memx_dev->fs.type);
		return ret;
	}

	if (!ret)
		ret = memx_fs_hwmon_init(memx_dev);

	return ret;
}

void memx_fs_deinit(struct memx_data *memx_dev)
{
	if (!memx_dev) {
		pr_err("%s: memx_dev is NULL\n", __func__);
		return;
	}
	switch (memx_dev->fs.type) {
	case MEMX_FS_HIF_PROC: {
		memx_fs_proc_deinit(memx_dev);
	} break;
	case MEMX_FS_HIF_SYS: {
		memx_fs_sys_deinit(memx_dev);
	} break;
	default:
		pr_err("%s: non support file system type(%u)\n", __func__, memx_dev->fs.type);
	}

	memx_fs_hwmon_deinit(memx_dev);
}
