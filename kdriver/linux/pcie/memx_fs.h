/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _MEMX_FILE_SYSTEM_H_
#define _MEMX_FILE_SYSTEM_H_

#define MEMX_MAX_FS_SYS_CMD_NAME_LEN (8)

struct proc_dir_entry;
struct kobject;
struct memx_pcie_dev;


struct memx_fs_cmd_cb {
	char name[MEMX_MAX_FS_SYS_CMD_NAME_LEN];
	u32 expected_argc;
	s32 (*exec)(struct memx_pcie_dev *memx_dev, u8 argc, char **argv);
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
	FS_CMD_MEMX9_ARGC = 0xa2,
	FS_CMD_RMTCMD_ARGC = 0x14,

	MAX_FS_CMD_ARGC_NUM = FS_CMD_WRITE_ARGC,
};

enum memx_fs_hif_type {
	MEMX_FS_HIF_NONE,
	MEMX_FS_HIF_PROC,
	MEMX_FS_HIF_SYS,
};

union memx_fs_hif {
	struct {
		struct proc_dir_entry *root_dir;
		struct proc_dir_entry *cmd_entry;
		struct proc_dir_entry *verinfo_entry;
		struct proc_dir_entry *mpu_uti_entry;
		struct proc_dir_entry *temperature_entry;
		struct proc_dir_entry *debug_entry;
		struct proc_dir_entry *thermal_entry;
		struct proc_dir_entry *qspi_entry;
		struct proc_dir_entry *throughput_entry;
	} proc;
	struct {
		struct kobject *root_dir;
	} sys;
};


struct memx_file_sys {
	enum memx_fs_hif_type type;
	union memx_fs_hif      hif;
	u32	debug_en;
};

s32 memx_fs_init(struct memx_pcie_dev *memx_dev);
void memx_fs_deinit(struct memx_pcie_dev *memx_dev);

s32 memx_fs_cmd_handler(struct memx_pcie_dev *memx_dev, u8 argc, char **argv);
s32 memx_fs_parse_cmd_and_exec(struct memx_pcie_dev *memx_dev, const char __user *user_input_buf, size_t user_input_buf_size);
u32 memx_crc32(const uint8_t *data, size_t length);
#endif
