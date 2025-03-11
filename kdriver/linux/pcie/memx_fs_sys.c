// SPDX-License-Identifier: GPL-2.0+
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/jiffies.h>
#include "memx_pcie.h"
#include "memx_xflow.h"
#include "memx_fs.h"
#include "memx_fs_sys.h"
#include "memx_fw_init.h"

struct kobj_memx_dev_entry {
	struct kobject *sys_kobj;
	struct memx_pcie_dev *memx_dev;
};

static struct kobj_memx_dev_entry g_kobj_memx_dev_map[8] = {
	{NULL, NULL},
	{NULL, NULL},
	{NULL, NULL},
	{NULL, NULL},
	{NULL, NULL},
	{NULL, NULL},
	{NULL, NULL},
	{NULL, NULL}
};

static char *g_usage[19] = {
	"Usage: echo \"fwlog chip_id[0-7] [hex_addr] [hex_val]\" > /sys/memx[dev_id 0-3]/cmd\n",
	"==========================================================================================\n",
	"Ex: Dump chip(0) firmware log\n",
	"echo \"fwlog 0\" > /sys/memx0/cmd\n",
	"Ex: Dump chip(1) firmware log\n",
	"echo \"fwlog 1\" > /sys/memx0/cmd\n\n",

	"Ex: Read chip(0) address 0x400A0000\n",
	"echo \"read 0 0x400A0000\" > /sys/memx0/debug; sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd;\n",
	"Ex: Write chip(0) address [0x400A0000]=0x12345678\n",
	"echo \"write 0 0x400A0000 0x12345678\" > /sys/memx0/debug; sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd;\n\n",

	"Ex: Issue chip(0) memx uart command\n",
	"echo \"memx0 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"echo \"memx1 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"echo \"memx2 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"echo \"memx3 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"echo \"memx6 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"echo \"memx7 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"echo \"memx8 0\" > /sys/memx0/cmd;sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"==========================================================================================\n"
};

static char *debug_usage[5] = {
	"Usage: echo \"rmtcmd chip_id[0-7] [hex_cmdcode] [hex_cmdparam]\" > /sys/memx[dev_id 0-3]/debug\n",
	"==========================================================================================\n",
	"Ex: Issue chip(0) command memx0\n",
	"echo \"rmtcmd 0 0x6d656d30 0x0\" > /sys/memx0/debug; sleep 1; echo \"fwlog 0\" > /sys/memx0/cmd; sudo dmesg\n",
	"==========================================================================================\n"
};

static ssize_t cmd_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0;
	s32 usgae_idx = 0;
	char *to_user_buf_pos = buf;
	int i;

	for (i = 0; i < 19; i++) {
		res += sprintf(to_user_buf_pos, "%s", g_usage[usgae_idx]);
		to_user_buf_pos += strlen(g_usage[usgae_idx++]);
	}
	return res;
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0;
	s32 usgae_idx = 0;
	char *to_user_buf_pos = buf;
	int i;

	for (i = 0; i < 5; i++) {
		res += sprintf(to_user_buf_pos, "%s", debug_usage[usgae_idx]);
		to_user_buf_pos += strlen(debug_usage[usgae_idx++]);
	}
	return res;
}

static ssize_t update_flash_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0, len;
	char *to_user_buf_pos = buf;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;
	const struct firmware *firmware = NULL;
	u32 *firmware_buffer_pos = NULL;
	u32 firmware_size = 0;
	u32 i, base_addr = MXCNST_DATASRAM_BASE;
	unsigned long timeout;
	u32 crc_value = 0, crc_check = 1;
	u32 type_value = 0, type_check = 0;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}

	len = sprintf(to_user_buf_pos, "%s", "================================================================================================\n"); to_user_buf_pos += len; res += len;

	if (request_firmware(&firmware, FIRMWARE_BIN_NAME, &memx_dev->pDev->dev) < 0) {
		len = sprintf(to_user_buf_pos, "%s", "download_fw: request_firmware for cascade.bin failed\n"); to_user_buf_pos += len; res += len;
		return 0;
	}
	firmware_buffer_pos = (u32 *)firmware->data;
	firmware_size = firmware->size;

	if (*((u32 *)((u8 *)firmware_buffer_pos + 0x6F08)) == 0) {
		crc_value = *((u32 *)((u8 *)firmware_buffer_pos + firmware_size - 8));
		crc_check = memx_crc32((u8 *)firmware_buffer_pos + 4, firmware_size - 12);
		type_value = *((u32 *)((u8 *)firmware_buffer_pos + MXCNST_IMG_TYPE_OFS));
		type_check = memx_sram_read(memx_dev, MXCNST_FW_TYPE_OFS); //skip first 4bytes of bin length
	} else if (*((u32 *)((u8 *)firmware_buffer_pos + 0x6F08)) == 1) {
		crc_value = *((u32 *)((u8 *)firmware_buffer_pos + firmware_size - 4));
		crc_check = memx_crc32((u8 *)firmware_buffer_pos, firmware_size - 4);
		type_value = *((u32 *)((u8 *)firmware_buffer_pos + MXCNST_IMG_TYPE_OFS));
		type_check = memx_sram_read(memx_dev, MXCNST_FW_TYPE_OFS);
	}

	if (crc_value != crc_check) {
		len = sprintf(to_user_buf_pos, "CHECK CRC(%#08x, %#08x) FAILED!!\n", crc_value, crc_check); to_user_buf_pos += len; res += len;
		release_firmware(firmware);
		return res;
	}
	if (type_value != type_check) {
		len = sprintf(to_user_buf_pos, "CHECK TYPE(%#08x, %#08x) FAILED!!\n", type_value, type_check); to_user_buf_pos += len; res += len;
		release_firmware(firmware);
		return res;
	}

	for (i = 0; i < firmware_size; i += 4)
		memx_sram_write(memx_dev, base_addr+i, firmware_buffer_pos[i>>2]);


	memx_sram_write(memx_dev, MXCNST_RMTCMD_PARAM, 0);
	memx_sram_write(memx_dev, MXCNST_RMTCMD_COMMD, MXCNST_MEMXQ_CMD);

	timeout = jiffies + (HZ*5);
	while (memx_sram_read(memx_dev, MXCNST_RMTCMD_PARAM) == 0) {
		if (time_after(jiffies, timeout)) {
			len = sprintf(to_user_buf_pos, "%s", "Update QSPI FLASH TIMEOUT FAILED!!\n"); to_user_buf_pos += len; res += len;
			release_firmware(firmware);
			return res;
		}
	};

	if (memx_sram_read(memx_dev, MXCNST_RMTCMD_PARAM) == 1) {
		len = sprintf(to_user_buf_pos, "Update QSPI FLASH PASS!! Verion:0x%08X DateCode:0x%08X (Please reboot to activate new fw)\n", firmware_buffer_pos[0x6F0C>>2], firmware_buffer_pos[0x6F10>>2]); to_user_buf_pos += len; res += len;
	} else {
		len = sprintf(to_user_buf_pos, "%s", "Update QSPI FLASH FAILED!!\n"); to_user_buf_pos += len; res += len;
	}

	len = sprintf(to_user_buf_pos, "%s", "================================================================================================\n"); to_user_buf_pos += len; res += len;
	release_firmware(firmware);
	return res;
}

static ssize_t verinfo_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0, len;
	char *to_user_buf_pos = buf;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;
	char chip_version[4] = "N/A";
	uint32_t value;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}

	len = sprintf(to_user_buf_pos, "pcie intf device:\n");
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, "SDK version: %s\n", SDK_VERSION);
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, "kdriver version: %s\n", PCIE_VERSION);
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, "FW_CommitID=0x%08x DateCode=0x%08x\n", memx_sram_read(memx_dev, MXCNST_COMMITID), memx_sram_read(memx_dev, MXCNST_DATECODE));
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, "ManufacturerID=0x%08x%08x\n", memx_sram_read(memx_dev, MXCNST_MANUFACTID2), memx_sram_read(memx_dev, MXCNST_MANUFACTID1));
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, "Cold+Warm-RebootCnt=%d  Warm-RebootCnt=%d\n"
		, ((memx_sram_read(memx_dev, MXCNST_COLDRSTCNT_ADDR)&0xFFFF0000) == 0x9ABC0000) ? (memx_sram_read(memx_dev, MXCNST_COLDRSTCNT_ADDR)&0xFFFF) : 0,
		((memx_sram_read(memx_dev, MXCNST_WARMRSTCNT_ADDR)&0xFFFF0000) == 0x9ABC0000) ? (memx_sram_read(memx_dev, MXCNST_WARMRSTCNT_ADDR)&0xFFFF) : 0);
	to_user_buf_pos += len;
	res += len;

	value = memx_xflow_read(memx_dev, 0, MXCNST_CHIP_VERSION, 0, false);
	if ((value & 0xF) == 5)
		snprintf(chip_version, 4, "A1");
	else
		snprintf(chip_version, 4, "A0");

	value = memx_xflow_read(memx_dev, 0, MXCNST_BOOT_MODE, 0, false);
	switch ((value >> 7) & 0x3) {
	case 0: {
		len = sprintf(to_user_buf_pos, "BootMode=QSPI  Chip= %s\n", chip_version);
	} break;
	case 1: {
		len = sprintf(to_user_buf_pos, "BootMode=USB  Chip=%s\n", chip_version);
	} break;
	case 2: {
		len = sprintf(to_user_buf_pos, "BootMode=PCIe  Chip=%s\n", chip_version);
	} break;
	case 3: {
		len = sprintf(to_user_buf_pos, "BootMode=UART  Chip=%s\n", chip_version);
	} break;
	default:
		len = sprintf(to_user_buf_pos, "BootMode=N/A  Chip=%s\n", chip_version);
		break;
	}
	to_user_buf_pos += len;
	res += len;
	return res;
}

static ssize_t utilization_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0, len;
	u8 chip_id, grpid = 0;
	char *to_user_buf_pos = buf;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}

	for (chip_id = 0; chip_id < memx_dev->mpu_data.hw_info.chip.total_chip_cnt; chip_id++) {
		u32 data = memx_sram_read(memx_dev, (MXCNST_MPUUTIL_BASE+(chip_id<<2)));

		if (data != 0xFF) {
			len = sprintf(to_user_buf_pos, "chip%d(group%d):%u%% ", chip_id, grpid, data);
			to_user_buf_pos += len;
			res += len;
			grpid++;
		}
	}
	len = sprintf(to_user_buf_pos, "\n");
	to_user_buf_pos += len;
	res += len;

	return res;
}

static ssize_t temperature_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0, len;
	u8 chip_id;
	char *to_user_buf_pos = buf;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;
	u32 data;
	s16 temp_Celsius = 0;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}

	for (chip_id = 0; chip_id < memx_dev->mpu_data.hw_info.chip.total_chip_cnt; chip_id++) {
		data = memx_sram_read(memx_dev, (MXCNST_TEMP_BASE+(chip_id<<2)));
		temp_Celsius = (data&0xFFFF) - 273;
		len = sprintf(to_user_buf_pos, "CHIP(%d) PVT%d Temperature: Temperature: %d C (%u Kelvin) (ThermalThrottlingState: %d)\n", chip_id, (data>>16)&0xF, temp_Celsius, (data&0xFFFF), (data>>20)&0xF);
		to_user_buf_pos += len;
		res += len;
	}


	return res;
}

static ssize_t thermalthrottling_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0, len;
	char *to_user_buf_pos = buf;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}

	len = sprintf(to_user_buf_pos, "%s\n", memx_dev->ThermalThrottlingDisable ? "Disable":"Enable");
	to_user_buf_pos += len;
	res += len;

	return res;
}

static ssize_t throughput_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	s32 res = 0, len;
	char *to_user_buf_pos = buf;
	u32 tx_size_kb = tx_size / 1024;
	u32 rx_size_kb = rx_size / 1024;
	u32 udrv_w_quotient = udrv_throughput_info.stream_write_us ? (udrv_throughput_info.stream_write_kb * 976 / udrv_throughput_info.stream_write_us) : 0;
	u32 udrv_w_decimal = udrv_throughput_info.stream_write_us ? (udrv_throughput_info.stream_write_kb * 976 % udrv_throughput_info.stream_write_us) * 1000 / udrv_throughput_info.stream_write_us : 0;
	u32 udrv_r_quotient = udrv_throughput_info.stream_read_us ? (udrv_throughput_info.stream_read_kb * 976 / udrv_throughput_info.stream_read_us) : 0;
	u32 udrv_r_decimal = udrv_throughput_info.stream_read_us ? (udrv_throughput_info.stream_read_kb * 976 % udrv_throughput_info.stream_read_us) * 1000 / udrv_throughput_info.stream_read_us : 0;
	u32 kdrv_w_quotient = tx_time_us ? (tx_size_kb * 976 / tx_time_us) : 0;
	u32 kdrv_w_decimal = tx_time_us ? (tx_size_kb * 976 % tx_time_us) * 1000 / tx_time_us : 0;
	u32 kdrv_r_quotient = rx_time_us ? (rx_size_kb * 976 / rx_time_us) : 0;
	u32 kdrv_r_decimal = rx_time_us ? (rx_size_kb * 976 % rx_time_us) * 1000 / rx_time_us : 0;
	u32 kdrv_w_value = kdrv_w_quotient * 1000 + kdrv_w_decimal;
	u32 kdrv_r_value = kdrv_r_quotient * 1000 + kdrv_r_decimal;
	u32 udrv_w_value = udrv_w_quotient * 1000 + udrv_w_decimal;
	u32 udrv_r_value = udrv_r_quotient * 1000 + udrv_r_decimal;
	u32 write_quotient = udrv_w_value ? (kdrv_w_value * 100 / udrv_w_value) : 0;
	u32 write_decimal = udrv_w_value ? (kdrv_w_value * 100 % udrv_w_value) * 1000 / udrv_w_value : 0;
	u32 read_quotient = udrv_r_value ? (kdrv_r_value * 100 / udrv_r_value) : 0;
	u32 read_decimal = udrv_r_value ? (kdrv_r_value * 100 % udrv_r_value) * 1000 / udrv_r_value : 0;

	len = sprintf(to_user_buf_pos, "  Item  |  Period(us)  |   Data(KB)   |   TP(MB/s)   | Kdrv/Udrv\n");
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, "--------+--------------+--------------+--------------+-------------\n");
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, " Kdrv_W	|  %#10x  |  %#10x  | %6u.%03u\n", tx_time_us, tx_size_kb, kdrv_w_quotient, kdrv_w_decimal);
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, " Udrv_W |  %#10x  |  %#10x  | %6u.%03u   |  %3u.%03u %%\n", udrv_throughput_info.stream_write_us, udrv_throughput_info.stream_write_kb, udrv_w_quotient, udrv_w_decimal, write_quotient, write_decimal);
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, " Kdrv_R	|  %#10x  |  %#10x  | %6u.%03u\n", rx_time_us, rx_size_kb, kdrv_r_quotient, kdrv_r_decimal);
	to_user_buf_pos += len;
	res += len;
	len = sprintf(to_user_buf_pos, " Udrv_R |  %#10x  |  %#10x  | %6u.%03u   |  %3u.%03u %%\n", udrv_throughput_info.stream_read_us, udrv_throughput_info.stream_read_kb, udrv_r_quotient, udrv_r_decimal, read_quotient, read_decimal);
	to_user_buf_pos += len;
	res += len;

	udrv_throughput_info.stream_write_kb = 0;
	udrv_throughput_info.stream_read_kb = 0;
	udrv_throughput_info.stream_write_us = 0;
	udrv_throughput_info.stream_read_us = 0;
	tx_time_us = 0;
	tx_size = 0;
	rx_time_us = 0;
	rx_size = 0;

	return res;
}

static ssize_t cmd_store(struct kobject *kobj, struct kobj_attribute *attr, const char *user_input_buf, size_t user_input_buf_size)
{
	s32 ret = -EINVAL;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}
	if (!memx_dev) {
		pr_err("memryx: memx_sys_write: memx_dev is NULL!\n");
		return ret;
	}
	if (!user_input_buf) {
		pr_err("memryx: memx_proc_write: user input buf is NULL!\n");
		return ret;
	}
	if (user_input_buf_size == 0) {
		pr_err("memryx: Command length is invalid!\n");
		return ret;
	}

	ret = memx_fs_parse_cmd_and_exec(memx_dev, user_input_buf, user_input_buf_size);
	if (ret != 0) {
		pr_err("memryx: memx_proc_write: parse or exec fail!, err(%d)\n", ret);
		return ret;
	}

	return user_input_buf_size;
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr, const char *user_input_buf, size_t user_input_buf_size)
{
	s32 ret = -EINVAL;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}
	if (!memx_dev) {
		pr_err("memryx: memx_sys_write: memx_dev is NULL!\n");
		return ret;
	}
	if (!user_input_buf) {
		pr_err("memryx: memx_proc_write: user input buf is NULL!\n");
		return ret;
	}
	if (user_input_buf_size == 0) {
		pr_err("memryx: Command length is invalid!\n");
		return ret;
	}

	ret = memx_fs_parse_cmd_and_exec(memx_dev, user_input_buf, user_input_buf_size);
	if (ret != 0) {
		pr_err("memryx: memx_proc_write: parse or exec failed, err(%d)\n", ret);
		return ret;
	}

	return user_input_buf_size;
}

static ssize_t thermalthrottling_store(struct kobject *kobj, struct kobj_attribute *attr, const char *user_input_buf, size_t user_input_buf_size)
{
	s32 ret = -EINVAL;
	struct memx_pcie_dev *memx_dev = NULL;
	u8 idx = 0;
	u32 chip_id;
	char *input_parser_buffer_ptr = NULL;

	for (idx = 0; idx < 8; idx++) {
		if (g_kobj_memx_dev_map[idx].sys_kobj && g_kobj_memx_dev_map[idx].sys_kobj == kobj) {
			memx_dev = g_kobj_memx_dev_map[idx].memx_dev;
			break;
		}
	}
	if (!memx_dev) {
		pr_err("memryx: memx_sys_write: memx_dev is NULL!\n");
		return ret;
	}
	if (!user_input_buf) {
		pr_err("memryx: memx_proc_write: user input buf is NULL!\n");
		return ret;
	}
	if (user_input_buf_size == 0) {
		pr_err("memryx: Command length is invalid!\n");
		return ret;
	}

	//ret = memx_fs_parse_cmd_and_exec(memx_dev, user_input_buf, user_input_buf_size);
	//if (ret != 0) { pr_err("memx_proc_write: parse or exec fail!, err(%d)\n", ret); return ret; }

	input_parser_buffer_ptr = kstrdup(user_input_buf, GFP_KERNEL);
	if (!input_parser_buffer_ptr) {
		//pr_err("thermal_store: kstrdup fail!\n");
		return ret;
	}

	if (strncmp(input_parser_buffer_ptr, "Enable", 6) == 0) {
		pr_warn("memryx: Set thermal throttling: Enabled\n");
		for (chip_id = 0; chip_id < memx_dev->mpu_data.hw_info.chip.total_chip_cnt; chip_id++) {
			*((_VOLATILE_ u32 *)(MEMX_GET_CHIP_RMTCMD_PARAM_VIRTUAl_ADDR(memx_dev, chip_id)))   = 1;
			*((_VOLATILE_ u32 *)(MEMX_GET_CHIP_RMTCMD_COMMAND_VIRTUAl_ADDR(memx_dev, chip_id))) = MXCNST_MEMXt_CMD;
		}
		memx_dev->ThermalThrottlingDisable = 0;
	} else if (strncmp(input_parser_buffer_ptr, "Disable", 7) == 0) {
		pr_warn("memryx: Set thermal throttling: Disabled\n");
		for (chip_id = 0; chip_id < memx_dev->mpu_data.hw_info.chip.total_chip_cnt; chip_id++) {
			*((_VOLATILE_ u32 *)(MEMX_GET_CHIP_RMTCMD_PARAM_VIRTUAl_ADDR(memx_dev, chip_id)))   = 0;
			*((_VOLATILE_ u32 *)(MEMX_GET_CHIP_RMTCMD_COMMAND_VIRTUAl_ADDR(memx_dev, chip_id))) = MXCNST_MEMXt_CMD;
		}
		memx_dev->ThermalThrottlingDisable = 1;
	} else {
		pr_err("memryx: Unsupported cmd:  %s(Only \"Enable\" and \"Disable\" are valid)\n", input_parser_buffer_ptr);
	}

	return user_input_buf_size;
}


static struct kobj_attribute g_memx_sysfs_attr		 = __ATTR_RW(cmd);
static struct kobj_attribute g_memx_sysfs_debug_attr   = __ATTR_RW(debug);
static struct kobj_attribute g_memx_sysfs_update_flash_attr	= __ATTR_RO(update_flash);
static struct kobj_attribute g_memx_sysfs_verinfo_attr = __ATTR_RO(verinfo);
static struct kobj_attribute g_memx_sysfs_mpuuti_attr  = __ATTR_RO(utilization);
static struct kobj_attribute g_memx_sysfs_temper_attr  = __ATTR_RO(temperature);
static struct kobj_attribute g_memx_sysfs_thermalthrottling_attr = __ATTR_RW(thermalthrottling);
static struct kobj_attribute g_memx_sysfs_throughput_attr = __ATTR_RO(throughput);


s32 memx_fs_sys_init(struct memx_pcie_dev *memx_dev)
{
	char root_dir_name[16];
	int minor = 0;
#ifndef ANDROID
	char name[128];
	struct file *fp;
#endif

	if (!memx_dev) {
		pr_err("memryx: memx_fs_sysfs_init: memx_dev is NULL\n");
		return -EINVAL;
	}

#ifndef ANDROID
	for (minor = 0; minor < 128; minor++) {
		sprintf(name, "/sys/memx%d/cmd", minor);
		fp = filp_open(name, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			pr_info("register for %s\n", name);
			break;

		} else {
			//pr_err("file existed %p\n", fp);
			filp_close(fp, NULL);
		}
	}
#endif

	sprintf(root_dir_name, DEVICE_NODE_NAME, minor);

	memx_dev->fs.hif.sys.root_dir = kobject_create_and_add(root_dir_name, NULL);
	if (!memx_dev->fs.hif.sys.root_dir) {
		pr_err("memryx: memx_fs_sysfs init: create sys root_dir failed!\n");
		return -ENOMEM;
	}

	if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_attr.attr)) {
		pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
		return -ENOMEM;
	}
	if (memx_dev->fs.debug_en) {
		if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_debug_attr.attr)) {
			pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
			return -ENOMEM;
		}
		if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_update_flash_attr.attr)) {
			pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
			return -ENOMEM;
		}

	}
	if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_verinfo_attr.attr)) {
		pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
		return -ENOMEM;
	}
	if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_mpuuti_attr.attr)) {
		pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
		return -ENOMEM;
	}
	if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_temper_attr.attr)) {
		pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
		return -ENOMEM;
	}
	if (memx_dev->fs.debug_en) {
		if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_thermalthrottling_attr.attr)) {
			pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
			return -ENOMEM;
		}
		if (sysfs_create_file(memx_dev->fs.hif.sys.root_dir, &g_memx_sysfs_throughput_attr.attr)) {
			pr_err("memryx: memx_fs_sysfs_init: create sysfs attr file failed!\n");
			return -ENOMEM;
		}
	}

	g_kobj_memx_dev_map[memx_dev->minor_index].sys_kobj = memx_dev->fs.hif.sys.root_dir;
	g_kobj_memx_dev_map[memx_dev->minor_index].memx_dev = memx_dev;

	return 0;
}

void memx_fs_sys_deinit(struct memx_pcie_dev *memx_dev);
void memx_fs_sys_deinit(struct memx_pcie_dev *memx_dev)
{
	if (!memx_dev) {
		pr_err("memryx: %s: memx_dev is NULL\n", __func__);
		return;
	}
	kobject_put(memx_dev->fs.hif.sys.root_dir);
}
