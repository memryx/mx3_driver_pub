// SPDX-License-Identifier: GPL-2.0+
#include <linux/module.h>
#include <linux/version.h>
#include <linux/dmapool.h>
#include <linux/dma-mapping.h>
#include <linux/time.h>
#include "memx_pcie.h"
#include "memx_xflow.h"
#include "memx_pcie_dev_list_ctrl.h"

#define DATA_BEGIN_CHIP_0 (0)
#define DATA_END_CHIP_0 CQ_DATA_LEN

#define THROUGHPUT_DATA_BEGIN_CHIP_0 DATA_BEGIN_CHIP_0
#define THROUGHPUT_DATA_BEGIN_CHIP_LAST (4)
#define THROUGHPUT_DATA_END_CHIP_LAST (8)

#define MEMX_GET_CHIP_ADMIN_CMD_BASE_VIRTUAL_ADDR(memx_dev, chip_id) (((memx_dev)->mpu_data.rx_dma_coherent_buffer_virtual_base) + \
																		MEMX_ADMCMD_VIRTUAL_OFFSET + ((chip_id) * MEMX_ADMCMD_SIZE))

static void memx_feature_trigger(struct memx_pcie_dev *memx_dev, uint8_t chip_id, struct transport_cmd *pCmd)
{
	uint32_t *cmd =  (uint32_t *) (MEMX_GET_CHIP_ADMIN_CMD_BASE_VIRTUAL_ADDR(memx_dev, chip_id));
	memcpy((void *)cmd, pCmd, sizeof(struct transport_sq));
	cmd[U32_ADMCMD_STATUS_OFFSET] = STATUS_RECEIVE;
	dma_sync_single_for_device(&memx_dev->pDev->dev, (dma_addr_t)(memx_dev->mpu_data.hw_info.fw.rx_dma_coherent_buffer_base + MEMX_ADMCMD_VIRTUAL_PAGE_OFFSET), MEMX_ADMCMD_VIRTUAL_PAGE_SIZE, DMA_BIDIRECTIONAL);
}

static void memx_feature_data_from_device(struct memx_pcie_dev *memx_dev, uint8_t chip_id, struct transport_cmd *cmd)
{
	uint32_t index = 0;
	uint8_t read_data_start = 0;
	uint8_t read_data_end = 0;

	if (cmd->SQ.subOpCode == FID_DEVICE_THROUGHPUT) {
		read_data_start = (chip_id == CHIP_ID0) ? THROUGHPUT_DATA_BEGIN_CHIP_0 : THROUGHPUT_DATA_BEGIN_CHIP_LAST;
		read_data_end   = (chip_id == CHIP_ID0) ? THROUGHPUT_DATA_BEGIN_CHIP_LAST : THROUGHPUT_DATA_END_CHIP_LAST;
	} else {
		read_data_start = DATA_BEGIN_CHIP_0;
		read_data_end = DATA_END_CHIP_0;
	}

	dma_sync_single_for_cpu(&memx_dev->pDev->dev, (dma_addr_t)(memx_dev->mpu_data.hw_info.fw.rx_dma_coherent_buffer_base + MEMX_ADMCMD_VIRTUAL_PAGE_OFFSET), MEMX_ADMCMD_VIRTUAL_PAGE_SIZE, DMA_BIDIRECTIONAL);
	for (index = read_data_start; index < read_data_end; index++) {
		uint32_t *AdminCmd =  (uint32_t *) (MEMX_GET_CHIP_ADMIN_CMD_BASE_VIRTUAL_ADDR(memx_dev, chip_id));
		cmd->CQ.data[index] = AdminCmd[U32_ADMCMD_CQ_DATA_OFFSET + index];
	}
}

static enum CASCADE_PLUS_ADMINCMD_ERROR_STATUS memx_feature_fetch_result(struct memx_pcie_dev *memx_dev, uint8_t chip_id, struct transport_cmd *cmd)
{
	enum CASCADE_PLUS_ADMINCMD_STATUS device_status = STATUS_IDLE;
	enum CASCADE_PLUS_ADMINCMD_ERROR_STATUS error_status = ERROR_STATUS_NO_ERROR;
	unsigned long timeout;
	uint32_t *AdminCmd = NULL;
	uint8_t subOpCode = cmd->SQ.subOpCode;

	timeout = jiffies + (HZ * 3);
	AdminCmd =  (uint32_t *) (MEMX_GET_CHIP_ADMIN_CMD_BASE_VIRTUAL_ADDR(memx_dev, chip_id));

	do {
		dma_sync_single_for_cpu(&memx_dev->pDev->dev, (dma_addr_t)(memx_dev->mpu_data.hw_info.fw.rx_dma_coherent_buffer_base + MEMX_ADMCMD_VIRTUAL_PAGE_OFFSET), MEMX_ADMCMD_VIRTUAL_PAGE_SIZE, DMA_BIDIRECTIONAL);
		device_status = AdminCmd[U32_ADMCMD_STATUS_OFFSET];

		if (device_status == STATUS_COMPLETE) {
			error_status = AdminCmd[U32_ADMCMD_CQ_STATUS_OFFSET];

			if (error_status != ERROR_STATUS_NO_ERROR)
				pr_err("memryx: Admin error subOpCode %d\n", subOpCode);
			else
				memx_feature_data_from_device(memx_dev, chip_id, cmd);

		} else if (time_after(jiffies, timeout)) {
			error_status = ERROR_STATUS_TIMEOUT_FAIL;
			pr_err("memryx: Admin timeout device status %d subop %d chip %d\n", device_status, subOpCode, chip_id);
			break;
		}
	} while (device_status != STATUS_COMPLETE);

	AdminCmd[U32_ADMCMD_STATUS_OFFSET] = STATUS_IDLE;
	dma_sync_single_for_device(&memx_dev->pDev->dev, (dma_addr_t)(memx_dev->mpu_data.hw_info.fw.rx_dma_coherent_buffer_base + MEMX_ADMCMD_VIRTUAL_PAGE_OFFSET), MEMX_ADMCMD_VIRTUAL_PAGE_SIZE, DMA_BIDIRECTIONAL);

	return error_status;
}

static long memx_feature_ioctl(struct file *filp, u32 cmd, unsigned long arg)
{
	long ret = 0;
	struct memx_pcie_dev *memx_dev = NULL;
	u32 major = 0;
	u32 minor = 0;

	if (!filp) {
		pr_err("memryx: feature_ioctl: Invalid parameters\n");
		return -ENODEV;
	}
	major = imajor(filp->f_inode);
	minor = iminor(filp->f_inode);
#ifdef DEBUG
	pr_info("memryx: feature_ioctl: device(major(%d)-minor(%d)), cmd:0x%x\n", major, minor, _IOC_NR(cmd));
#endif
	if (_IOC_TYPE(cmd) != MEMX_IOC_MAJOR) {
		pr_err("memryx: feature_ioctl: _IOC_TYPE(cmd) != MEMX_IOC_MAGIC\n");
		return -ENOTTY;
	}
	if (_IOC_NR(cmd) > MEMX_IOC_MAXNR) {
		pr_err("memryx: feature_ioctl: _IOC_NR(cmd) > MEMX_IOC_MAXNR(%d)\n", MEMX_IOC_MAXNR);
		return -ENOTTY;
	}

	memx_dev = (struct memx_pcie_dev *)filp->private_data;
	if (!memx_dev || !memx_dev->pDev) {
		pr_err("memryx: feature_ioctl: No Opened Device!\n");
		return -ENODEV;
	}

	switch (cmd) {
	case MEMX_GET_DEVICE_FEATURE: {
		struct transport_cmd cmd = {0};
		uint32_t subOpCode = 0;

		if (copy_from_user(&cmd, (struct transport_cmd *)arg, sizeof(struct transport_cmd))) {
			pr_err("memryx: MEMX_GET_DEVICE_FEATURE, copy_from_user failed\n");
			ret = -ENOMEM;
			goto done;
		}

		subOpCode = cmd.SQ.subOpCode;

		if (subOpCode == FID_DEVICE_FW_INFO) {
			cmd.CQ.data[0] = memx_sram_read(memx_dev, MXCNST_CQDATA0_ADDR);
			cmd.CQ.data[1] = memx_sram_read(memx_dev, MXCNST_COMMITID);
			cmd.CQ.data[2] = memx_sram_read(memx_dev, MXCNST_DATECODE);
			cmd.CQ.data[3] = memx_xflow_read(memx_dev, 0, MXCNST_BOOT_MODE, 0, false);
			cmd.CQ.data[4] = memx_xflow_read(memx_dev, 0, MXCNST_CHIP_VERSION, 0, false);
			cmd.CQ.status  = ERROR_STATUS_NO_ERROR;
		} else if (subOpCode == FID_DEVICE_THROUGHPUT) {
			cmd.CQ.data[0]  = tx_time_us;
			cmd.CQ.data[1]  = tx_size / KBYTE;
			cmd.CQ.data[2] = rx_time_us;
			cmd.CQ.data[3] = rx_size / KBYTE;
			cmd.CQ.data[4] = udrv_throughput_info.stream_write_us;
			cmd.CQ.data[5] = udrv_throughput_info.stream_write_kb;
			cmd.CQ.data[6] = udrv_throughput_info.stream_read_us;
			cmd.CQ.data[7] = udrv_throughput_info.stream_read_kb;
			tx_time_us = 0;
			tx_size = 0;
			rx_time_us = 0;
			rx_size = 0;
			udrv_throughput_info.stream_write_us = 0;
			udrv_throughput_info.stream_write_kb = 0;
			udrv_throughput_info.stream_read_us = 0;
			udrv_throughput_info.stream_read_kb = 0;
		} else if ((subOpCode == FID_DEVICE_POWERMANAGEMENT) || (subOpCode == FID_DEVICE_FREQUENCY)) {
			uint8_t chip_id = cmd.SQ.cdw2;

			if ((chip_id < MAX_CHIP_NUM) && (memx_dev->mpu_data.hw_info.chip.roles[chip_id] != ROLE_UNCONFIGURED)) {
				memx_feature_trigger(memx_dev, chip_id, &cmd);
				cmd.CQ.status = memx_feature_fetch_result(memx_dev, chip_id, &cmd);
			} else {
				cmd.CQ.status = ERROR_STATUS_PARAMETER_FAIL;
			}
		} else {
			memx_feature_trigger(memx_dev, CHIP_ID0, &cmd);
			cmd.CQ.status = memx_feature_fetch_result(memx_dev, CHIP_ID0, &cmd);
		}

		if (subOpCode == FID_DEVICE_INFO) {
			char version[8] = PCIE_VERSION;

			memcpy(&cmd.CQ.data[7], version, sizeof(unsigned int));
			memcpy(&cmd.CQ.data[8], &version[sizeof(unsigned int)], sizeof(unsigned int));
		}

		if (copy_to_user((void __user *)arg, &cmd, sizeof(struct transport_cmd))) {
			pr_err("memryx: feature_ioctl: MEMX_GET_DEVICE_FEATURE, copy_to_user failed\n");
			ret = -ENOMEM;
			goto done;
		}
	}
	break;
	case MEMX_SET_DEVICE_FEATURE: {
		struct transport_cmd cmd = {0};
		uint8_t chip_id = 0;

		if (copy_from_user(&cmd, (struct transport_cmd *)arg, sizeof(struct transport_cmd))) {
			pr_err("memryx: MEMX_GET_DEVICE_FEATURE, copy_from_user failed\n");
			ret = -ENOMEM;
			goto done;
		}

		chip_id = cmd.SQ.cdw2;
		if (chip_id < memx_dev->mpu_data.hw_info.chip.total_chip_cnt) {
			memx_feature_trigger(memx_dev, chip_id, &cmd);
			cmd.CQ.status = memx_feature_fetch_result(memx_dev, chip_id, &cmd);
		} else {
			cmd.CQ.status = ERROR_STATUS_PARAMETER_FAIL;
		}

		if (copy_to_user((void __user *)arg, &cmd, sizeof(struct transport_cmd))) {
			pr_err("memryx: feature_ioctl: MEMX_SET_DEVICE_FEATURE, copy_to_user failed\n");
			ret = -ENOMEM;
			goto done;
		}
	}
	break;
	default:
		ret = -EFAULT;
		pr_err("memryx: feature_ioctl: (%u-%u): unsupported ioctl cmd(%u)\n", major, minor, cmd);
	}
done:
#ifdef DEBUG
	pr_info(" feature_ioctl: (%u-%u): finish\n", major, minor);
#endif
	return ret;
}

static s32 memx_feature_open(struct inode *inode, struct file *filp)
{
	u32 minor = iminor(filp->f_inode);
#ifdef DEBUG
	u32 major = imajor(filp->f_inode);
#endif

	struct memx_pcie_dev *memx_dev = memx_get_device_by_index(minor);

	if (!memx_dev) {
		pr_err("memryx: feature_open: PCIe device not found for /dev/memx%d node.\n", minor);
		return -ENODEV;
	}
	filp->private_data = memx_dev;

#ifdef DEBUG
	pr_info("memryx: feature_open: open on /dev/memx%d_feature (%d-%d), vendor_id(%0x), devid_id(%0x))\n",
		memx_dev->minor_index, major, minor, (memx_dev ? memx_dev->pDev->vendor : 0), (memx_dev ? memx_dev->pDev->device : 0));
#endif
	return 0;
}

static s32 memx_feature_release(struct inode *inode, struct file *filp)
{
#ifdef DEBUG
	u32 major = imajor(filp->f_inode);
	u32 minor = iminor(filp->f_inode);
#endif

#ifdef DEBUG
	pr_info("memryx: feature_close: (%d-%d) success.\n", major, minor);
#endif
	return 0;
}

struct file_operations memx_feature_fops = {
owner: THIS_MODULE,
unlocked_ioctl : memx_feature_ioctl,
open : memx_feature_open,
release : memx_feature_release,
};
