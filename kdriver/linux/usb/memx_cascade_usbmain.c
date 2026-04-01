// SPDX-License-Identifier: GPL-2.0+
#include <linux/module.h>	// included for all kernel modules
#include <linux/kernel.h>	// included for KERN_INFO
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include "../include/memx_ioctl.h"
#include "memx_cascade_usb.h"

static unsigned int frame_size = 252;
module_param(frame_size, uint, 0);
MODULE_PARM_DESC(frame_size, "frame size, default is 252");

static u32 g_drv_fs_type = MEMX_FS_HIF_SYS;
static u32 fs_debug_en;
static u32 pcie_lane_no	= 2;
static u32 pcie_lane_speed = 3;
static u32 pcie_aspm;
static void *device_link[MAX_CHIP_NUM];
static DEFINE_MUTEX(device_mutex);
static struct class *memx_feature_class;

dev_t g_feature_devno;
struct usb_driver memx_ai_driver;

module_param(g_drv_fs_type, uint, 0);
MODULE_PARM_DESC(g_drv_fs_type, "debugfs control:: 0-Disable debugfs  1-proc filesys  2-sysfs filesys(default)");
module_param(fs_debug_en, uint, 0);
MODULE_PARM_DESC(fs_debug_en, "debugfs's debug option:: 0-Disable(default)  1-Enable");
module_param(pcie_lane_no, uint, 0);
MODULE_PARM_DESC(pcie_lane_no, "Internal chip2chip pcie link lane number. ValidRange: 1/2. 2 is default");
module_param(pcie_lane_speed, uint, 0);
MODULE_PARM_DESC(pcie_lane_speed, "Internal chip2chip pcie link speed. ValidRange: 1/2/3. 3 is default means GEN3");
module_param(pcie_aspm, uint, 0);
MODULE_PARM_DESC(pcie_aspm, "Internal chip2chip pcie link aspm control:: 0-FW_default(default) 1-L0_only 2-L0sL1 3-L0sL1.1");

ktime_t tx_start_time = 0, tx_end_time = 0;
ktime_t rx_start_time = 0, rx_end_time = 0;
u32 tx_time_us = 0, rx_time_us = 0;
u32 tx_size = 0, rx_size = 0;
struct memx_throughput_info udrv_throughput_info = {0};
#define THROUGHPUT_ADD(current_size, additional_size) \
	do { \
		if ((current_size) > 0xffffffff - (additional_size)) { \
			tx_time_us = 0; \
			rx_time_us = 0; \
			tx_size = 0; \
			tx_size = 0; \
		} else { \
			(current_size) += (additional_size); \
		} \
	} while (0)

static const struct usb_device_id memx_table[] = {
	/* MemryX CHIP-3 */
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G0_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G0_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G0_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G1_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G1_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G1_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G2_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G2_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G2_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G3_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G3_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_G3_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_MX3PLUS_ZSBL_DEV) },

	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G0_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G0_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G0_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G1_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G1_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G1_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G2_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G2_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G2_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G3_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G3_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_G3_MULTI_LAST) }, /*xN last*/
	{ USB_DEVICE(DEVICE_VENDOR_ID_MEMRYX, ROLE_MX3PLUS_ZSBL_DEV) },

	{ }	/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, memx_table);

struct DFP_MPU_WT_MEM {
	uint32_t mpu_cg_wt_mem_data_adr;
	uint32_t mpu_cg_wt_mem_data_reg;
	uint32_t mpu_cg_wt_mem_data_sze;
	uint32_t mpu_cg_wt_mem_data_crc;
	uint32_t mpu_cg_wt_memflg_data_adr;
	uint32_t mpu_cg_wt_memflg_data_reg;
	uint32_t mpu_cg_wt_memflg_data_sze;
	uint32_t mpu_cg_wt_memflg_data_crc;
} __packed;

#define MAX_WT_CNT		 6
#define DFP_CFG_SZ		 0xD0
#define DFP_OFS(N)		 (0x10 + N*0xD0)
#define MAX_CFG_SZ		 MAX_OPS_SIZE
#define SEP_NEXT_OFS	   4
#define SEP_LEN_OFS		12

struct DFP_DATA {
	uint32_t mpu_rg_cfg_adr;
	uint32_t mpu_rg_cfg_sze;
	uint32_t mpu_cg_wt_mem_count;
	uint32_t reserved0;
	struct DFP_MPU_WT_MEM mpu_cg_wt_mem_data[MAX_WT_CNT];
} __packed;

#define MEMX_XFER_STATE_NORMAL 0
#define MEMX_XFER_STATE_ABORT  1

#if  KERNEL_VERSION(6, 2, 0) > _LINUX_VERSION_CODE_
static char *memx_usb_devnode(struct device *dev, umode_t *mode)
#else
static char *memx_usb_devnode(const struct device *dev, umode_t *mode)
#endif
{
	if (mode)
		*mode = DEVICE_NODE_DEFAULT_ACCESS_RIGHT;

	return NULL;
}


void memx_complete(struct urb *urb)
{
	struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&data->fw_comp);
}

static void memx_txcomplete(struct urb *urb)
{
	struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&data->tx_comp);
}

static void memx_rxcomplete(struct urb *urb)
{
	struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&data->rx_comp);
	wake_up_interruptible(&data->read_wq);
}

void memx_fwrxcomplete(struct urb *urb)
{
	struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&data->fwrx_comp);
}

static int memx_firmware_init(struct memx_data *data)
{
	struct memx_firmware_bin memx_fw_bin;
	const struct firmware *firmware = NULL;
	u32 firmware_size = 0;
	u8 *firmware_buffer_pos = NULL;
	u8 *tbuffer;
	u8 ImgFmt = 0;

	memx_fw_bin.request_firmware_update_in_linux = true;
	strscpy(&memx_fw_bin.name[0], FIRMWARE_BIN_NAME, FILE_NAME_LENGTH - 1);
	memx_fw_bin.buffer = NULL;
	memx_fw_bin.size = 0;

	if (memx_fw_bin.request_firmware_update_in_linux) {
		if (request_firmware(&firmware, memx_fw_bin.name, &data->udev->dev) < 0) {
			pr_err("downlaod_fw: request_firmware for %s failed\n", memx_fw_bin.name);
			return -ENODEV;
		}
		firmware_buffer_pos = (u8 *)firmware->data;
		firmware_size = firmware->size-4;
	} else {
		pr_err("%s user path not implemented\n", __func__);
		return -ENODEV;
	}

	if (firmware_size >= 0x7004)
		ImgFmt = *(u32 *)(firmware_buffer_pos+0x6F08);

	if (ImgFmt == 1)
		firmware_size = firmware_size+8;

	tbuffer = kzalloc(256*1024, GFP_KERNEL);
	if (!tbuffer) {
		//pr_err("Can't allocate memory for fw tx");
		if (memx_fw_bin.request_firmware_update_in_linux)
			release_firmware(firmware);

		return -ENOMEM;
	}

	memcpy(tbuffer, &firmware_size, 4);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_OUT_EP),
													tbuffer, 4, memx_complete, data);
	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit fw sz urb");
		kfree(tbuffer);
		if (memx_fw_bin.request_firmware_update_in_linux)
			release_firmware(firmware);

		return -ENODEV;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT3S))) {
		pr_err("wait urb timeout 1\n");
		kfree(tbuffer);
		if (memx_fw_bin.request_firmware_update_in_linux)
			release_firmware(firmware);

		return -ENODEV;
	}

	memcpy(tbuffer, firmware_buffer_pos+4, firmware_size);

	if (ImgFmt == 1) {
		*(u32 *)(tbuffer + firmware_size - 8) = *(u32 *)(firmware_buffer_pos);
		*(u32 *)(tbuffer + firmware_size - 4) = 1;
	}

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_OUT_EP),
													tbuffer, firmware_size, memx_complete, data);
	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit fw context urb");
		kfree(tbuffer);
		if (memx_fw_bin.request_firmware_update_in_linux)
			release_firmware(firmware);

		return -ENODEV;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT3S))) {
		pr_err("wait urb timeout 2\n");
		kfree(tbuffer);
		if (memx_fw_bin.request_firmware_update_in_linux)
			release_firmware(firmware);

		return -ENODEV;
	}

	kfree(tbuffer);
	if (memx_fw_bin.request_firmware_update_in_linux)
		release_firmware(firmware);

	pr_info("FW download successfully(size %u)", firmware_size+4);
	return 0;
}

static void clear_fw_id(struct memx_data *data)
{
	uint32_t cfg_header[2] = {0};

	cfg_header[0] = FWCFG_ID_CLR;
	cfg_header[1] = 0;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 0, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr zlp");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}
}

static void set_dfp_chip_id(struct memx_data *data, uint32_t id)
{
	uint32_t cfg_header[2] = {0};
	uint32_t chip_id = id;

	cfg_header[0] = FWCFG_ID_DFP_CHIPID;
	cfg_header[1] = 4;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	memcpy(data->tbuffer, &chip_id, 4);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 4, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg dfp");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg dfp timeout\n");
		return;
	}

	clear_fw_id(data);
}

static void set_wtmem_addr(struct memx_data *data, uint32_t id, uint32_t addr)
{
	uint32_t cfg_header[2] = {0};
	uint32_t wtmem_addr = addr;

	cfg_header[0] = id;
	cfg_header[1] = 4;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);
	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	memcpy(data->tbuffer, &wtmem_addr, 4);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 4, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	clear_fw_id(data);
}

static void set_wtmem_size(struct memx_data *data, uint32_t size)
{
	uint32_t cfg_header[2] = {0};

	cfg_header[0] = FWCFG_ID_DFP_WTMEMSZ;
	cfg_header[1] = size;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}
}

static int memx_flash_download(struct memx_data *data, unsigned int cmd,
										struct memx_firmware_bin *memx_bin)
{
	uint32_t remaining_size_need_to_send = 0;
	uint32_t cfg_header[2] = {0};
	const struct firmware *firmware;
	uint32_t firmware_size = 0;
	uint8_t *firmware_buffer_pos = NULL;
	int32_t result = -ENOMEM;

	pr_info("firmware download binary from %s\n", memx_bin->request_firmware_update_in_linux ? "lib/firmware" : "userspace");
	/* use kernel firmware mechanism or download from user space */
	if (memx_bin->request_firmware_update_in_linux) {
		const char *name = memx_bin->name;

		name = ((name) ? (name) : ((cmd == MEMX_DOWNLOAD_FIRMWARE) ? "cascade.bin" : "cascade.dfp"));
		if (request_firmware(&firmware, name, &data->udev->dev) < 0) {
			pr_err("cascade bin request failed\n");
			return -ENODEV;
		}
		firmware_buffer_pos = (uint8_t *)firmware->data;
		firmware_size = firmware->size;
	} else {
		firmware_buffer_pos = kmalloc(memx_bin->size, GFP_KERNEL);
		if (!firmware_buffer_pos) {
			//pr_err("kmalloc for firmware failed\n");
			return -ENOMEM;
		}
		if (copy_from_user(firmware_buffer_pos, memx_bin->buffer, memx_bin->size)) {
			kfree(firmware_buffer_pos);
			pr_err("cascade bin request failed\n");
			return -ENOMEM;
		}
		firmware_size = memx_bin->size;
	}

	cfg_header[0] = (cmd == MEMX_DOWNLOAD_FIRMWARE) ? FWCFG_ID_FW : FWCFG_ID_DFP;
	cfg_header[1] = firmware_size;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
													data->tbuffer, 8, memx_complete, data);
	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg dfp");
		goto fail;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg dfp timeout\n");
		goto fail;
	}

	remaining_size_need_to_send = firmware_size;

	while (remaining_size_need_to_send > 0) {
		/* set up our urb */
		uint32_t transfered_size = (remaining_size_need_to_send > MAX_OPS_SIZE) ?
									MAX_OPS_SIZE : remaining_size_need_to_send;
		memcpy(data->tbuffer, firmware_buffer_pos, transfered_size);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
														data->tbuffer, transfered_size, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit TX URB");
			goto fail;
		}
		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait tx timeout\n");
			goto fail;
		}

		remaining_size_need_to_send -= transfered_size;
		firmware_buffer_pos += transfered_size;
	}

	if (remaining_size_need_to_send == 0) {
		usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
			data->fw_rbuffer, 4, memx_fwrxcomplete, data);

		/* get the data in the bulk port */
		if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit data read");
			goto fail;
		}

		if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait data read timeout\n");
			goto fail;
		}

		result = *((int32_t *)(data->fw_rbuffer));
		if (result) {
			pr_err("firmware download failed %d\n", result);
			result = -ENOMEM;
		} else {
			result = 0;
		}
	}


fail:
	clear_fw_id(data);
	if (memx_bin->request_firmware_update_in_linux)
		release_firmware(firmware);
	else
		kfree(firmware_buffer_pos);


	return result;
}

static int checkpatch_avoid_leading_tabs(struct memx_data *data, uint8_t *dfp_wtmem_addr, uint32_t wtmem_sze)
{
	if (copy_from_user(data->tbuffer, dfp_wtmem_addr, wtmem_sze)) {
		pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
		return -ENOMEM;
	}
	return 0;
}

static int memx_seperate_dfp_download(struct memx_data *data, const unsigned char *buffer, uint32_t dfp_count, uint8_t dfp_src)
{
	uint32_t cfg_header[2] = {0};
	int i = 0, j = 0;
	uint8_t *dfp_len_buf = NULL;
	uint8_t *dfp_wtmem_count_buf = NULL;
	uint8_t *dfp_wtmem_size_buf = NULL;
	uint8_t *dfp_cfg_addr = NULL;
	uint8_t *dfp_wtmem_addr = NULL;
	uint32_t total_length = 0;
	uint32_t reg_write_addr = 0;
	uint32_t cfg_size, wtmem_count, wtmem_sze;
	uint32_t dfp_id = 0;

	dfp_len_buf = (void *)buffer;

	/*program each dfp data*/
	for (i = 0; i < dfp_count; i++) {

		dfp_len_buf += (total_length + SEP_NEXT_OFS);
		/*read dfp id value*/
		if (copy_from_user((void *)&dfp_id, (const void *)dfp_len_buf, 4)) {
			pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
			return -ENOMEM;
		}

		dfp_len_buf += SEP_NEXT_OFS;
		/*read total length value*/
		if (copy_from_user((void *)&total_length, (const void *)dfp_len_buf, 4)) {
			pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
			return -ENOMEM;
		}

		set_dfp_chip_id(data, dfp_id);

		if (dfp_src == DFP_FROM_SEPERATE_CONFIG) {
			/*Remove reset flow to reduce model swap time*/
			/*reset_mpu(data, i + MPU_CHIP_ID_BASE);*/

			cfg_size = total_length;
			dfp_cfg_addr = dfp_len_buf + 4;

			cfg_header[0] = FWCFG_ID_DFP_CFGSIZE;
			cfg_header[1] = cfg_size;

			memcpy(data->tbuffer, cfg_header, 8);

			usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
				data->tbuffer, 8, memx_complete, data);

			/* send the data out the bulk port */
			if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
				pr_err("Can't submit cfg size");
				return -ENODEV;
			}

			if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
				pr_err("wait cfg size timeout\n");
				return -ENODEV;
			}

			while (cfg_size > 0) {
				if (cfg_size > MAX_CFG_SZ) {
					if (copy_from_user(data->tbuffer, dfp_cfg_addr, MAX_CFG_SZ)) {
						pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
						return -ENOMEM;
					}

					usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
						data->tbuffer, MAX_CFG_SZ, memx_complete, data);

					dfp_cfg_addr += MAX_CFG_SZ;
					cfg_size -= MAX_CFG_SZ;
				} else {
					if (copy_from_user(data->tbuffer, dfp_cfg_addr, cfg_size)) {
						pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
						return -ENOMEM;
					}

					usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
						data->tbuffer, cfg_size, memx_complete, data);

					cfg_size = 0;
				}

				/* send the data out the bulk port */
				if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
					pr_err("Can't submit cfg data");
					return -ENODEV;
				}

				if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
					pr_err("wait cfg data timeout\n");
					return -ENODEV;
				}
			}

			clear_fw_id(data);
		} else {
			uint32_t wtmem_data[2] = {};

			dfp_wtmem_count_buf = dfp_len_buf + 4;

			/*program wt_mem*/
			if (copy_from_user((void *)&wtmem_count, (const void *)dfp_wtmem_count_buf, 4)) {
				pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
				return -ENOMEM;
			}

			if (wtmem_count == 0) {
				pr_err("invalid wtmem count %d\r\n", wtmem_count);
				return -ENODEV;
			}

			dfp_wtmem_size_buf = dfp_wtmem_count_buf + 4;

			for (j = 0; j < wtmem_count; j++) {
				if (copy_from_user((void *)wtmem_data, (const void *)dfp_wtmem_size_buf, 8)) {
					pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail %s:%d\n",  __func__, __LINE__);
					return -ENOMEM;
				}

				wtmem_sze = wtmem_data[0] - 8;
				reg_write_addr = wtmem_data[1];
				dfp_wtmem_addr = dfp_wtmem_size_buf + 8;

				set_wtmem_addr(data, FWCFG_ID_DFP_WMEMADR, reg_write_addr);
				set_wtmem_size(data, wtmem_sze);
				while (wtmem_sze > 0) {
					int ret;

					if (wtmem_sze > MAX_CFG_SZ)
						ret = checkpatch_avoid_leading_tabs(data, dfp_wtmem_addr, MAX_CFG_SZ);
					else
						ret = checkpatch_avoid_leading_tabs(data, dfp_wtmem_addr, wtmem_sze);

					if (ret < 0)
						return -ENOMEM;

					if (wtmem_sze > MAX_CFG_SZ) {
						usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
							data->tbuffer, MAX_CFG_SZ, memx_complete, data);

						dfp_wtmem_addr += MAX_CFG_SZ;

						wtmem_sze -= MAX_CFG_SZ;
					} else {
						usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
							data->tbuffer, wtmem_sze, memx_complete, data);

						wtmem_sze = 0;
					}

					/* send the data out the bulk port */
					if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
						pr_err("Can't submit wtmem data");
						return -ENODEV;
					}

					if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
						pr_err("wait wtmem data timeout\n");
						return -ENODEV;
					}
				}
				clear_fw_id(data);

				dfp_wtmem_size_buf += wtmem_data[0] + 4;
			}
		}
		dfp_len_buf += 4; /*0xFFFFFFFF fir the end of one chip's config*/
		/*Remove recfg flow to reduce model swap time*/
		/*recfg_mpu(data, i + MPU_CHIP_ID_BASE);*/
	}

	return 0;
}

static int memx_reg_operation(struct memx_data *data, unsigned char *user_buffer, unsigned long reg_start, unsigned long n_bytes, unsigned int cmd)
{
	uint32_t cfg_header[2] = {0};
	int ret = 0;
	int i = 0;

	if (cmd == MEMX_WRITE_REG) {
		unsigned char *buf = data->tbuffer;
		unsigned long xfer_total_size = n_bytes;
		size_t xfer_size;

		cfg_header[0] = FWCFG_ID_WREG;
		cfg_header[1] = xfer_total_size;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			return ret;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			return ret;
		}

		while (xfer_total_size > 0) {
			if (xfer_total_size > data->max_mpuout_size)
				xfer_size = data->max_mpuout_size;
			else
				xfer_size = xfer_total_size;

			if (copy_from_user(buf, user_buffer + i*data->max_mpuout_size, xfer_size)) {
				pr_err("Copy from user failed!");
				ret = -ENODEV;
				break;
			}

			/* set up our urb */
			usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
					buf, xfer_size, memx_txcomplete, data);

			/* send the data out the bulk port */
			if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
				pr_err("Can't submit TX URB");
				ret = -ENODEV;
				break;
			}

			if (!wait_for_completion_timeout(&data->tx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
				pr_err("wait tx timeout\n");
				ret = -ENODEV;
				break;
			}

			xfer_total_size -= xfer_size;
			i++;
		}

		clear_fw_id(data);
	} else {
		uint32_t xfer_total_size = n_bytes;
		size_t xfer_size;
		uint32_t buffer_addr = reg_start;

		cfg_header[0] = FWCFG_ID_RREG_ADR;
		cfg_header[1] = 4;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit reg addr hdr");
			ret = -ENOMEM;
			return ret;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait reg addr hdr timeout\n");
			ret = -ENOMEM;
			return ret;
		}

		memcpy(data->tbuffer, &buffer_addr, 4);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 4, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit reg addr");
			ret = -ENOMEM;
			return ret;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait reg addr timeout\n");
			ret = -ENOMEM;
			return ret;
		}

		clear_fw_id(data);

		cfg_header[0] = FWCFG_ID_RREG;
		cfg_header[1] = 4;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit read size hdr");
			ret = -ENOMEM;
			return ret;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait read size hdr timeout\n");
			ret = -ENOMEM;
			return ret;
		}

		memcpy(data->tbuffer, &xfer_total_size, 4);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 4, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit read size");
			ret = -ENOMEM;
			return ret;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait read size timeout\n");
			ret = -ENOMEM;
			return ret;
		}

		clear_fw_id(data);

		usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
			data->fw_rbuffer, MAX_OPS_SIZE, memx_fwrxcomplete, data);

		/* get the data in the bulk port */
		if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit data read");
			ret = -ENOMEM;
			return ret;
		}

		if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait data read timeout\n");
			ret = -ENOMEM;
			return ret;
		}

		xfer_size = xfer_total_size;

		if (copy_to_user(user_buffer, data->fw_rbuffer, xfer_size)) {
			pr_err("Copy to user failed!");
			ret = -ENOMEM;
			return ret;
		}
	}

	return ret;
}

static void memx_abort_transfer(struct memx_data *data)
{
	int unlink_status = 0;

	data->state = MEMX_XFER_STATE_ABORT;
	unlink_status = usb_unlink_urb(data->rxurb);

	if (unlink_status != -EINPROGRESS) {
		if (!IS_WARNING_SKIP(unlink_status)) {
			pr_info("Abort without pending device %d\r\n", unlink_status);
		}
	}
}

static int memx_get_fwupdate_status(struct memx_data *data, unsigned char *user_buffer)
{
	int ret = 0;
	size_t xfer_size = 0;

	usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		data->fw_rbuffer, MAX_OPS_SIZE, memx_fwrxcomplete, data);

	/* get the data in the bulk port */
	if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
		pr_err("%s: Can't submit data read", __func__);
		ret = -ENOMEM;
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("%s: Wait data read timeout\n", __func__);
		ret = -ENOMEM;
		return ret;
	}

	xfer_size = data->fw_rxurb->actual_length;

	if (copy_to_user(user_buffer, data->fw_rbuffer, xfer_size)) {
		pr_err("%s: Copy to user failed!", __func__);
		ret = -ENOMEM;
		return ret;
	}

	return ret;
}

static int memx_usb_config_mpu_group(struct memx_data *data, struct hw_info *hw_info)
{
	uint32_t cfg_header[2] = {0};
	uint32_t chip_roles[MAX_SUPPORT_CHIP_NUM] = {0};
	uint8_t chip_id = 0;
	int ret = 0;

	cfg_header[0] = FWCFG_ID_MPU_GROUP;
	cfg_header[1] = MAX_SUPPORT_CHIP_NUM * 4;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg dfp");
		ret = -ENOMEM;
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait cfg dfp timeout\n");
		ret = -ENOMEM;
		return ret;
	}

	for (chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++)
		chip_roles[chip_id] = hw_info->chip.roles[chip_id];

	memcpy(data->tbuffer, chip_roles, MAX_SUPPORT_CHIP_NUM * 4);

	/* set up our urb */
	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, MAX_SUPPORT_CHIP_NUM * 4, memx_txcomplete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit TX URB");
		ret = -ENODEV;
		return ret;
	}

	if (!wait_for_completion_timeout(&data->tx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait tx timeout\n");
		ret = -ENODEV;
		return ret;
	}

	clear_fw_id(data);
	return 0;
}

static int memx_usb_reset_device(struct memx_data *data)
{
	uint32_t cfg_header[2] = {0};
	int ret = 0;

	cfg_header[0] = FWCFG_ID_RESET_DEVICE;
	cfg_header[1] = 8;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit reset device");
		ret = -ENOMEM;
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
		pr_err("wait reset device timeout\n");
		ret = -ENOMEM;
		return ret;
	}

	clear_fw_id(data);
	return 0;
}

static ssize_t memx_read(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct memx_data *data = file->private_data;
	struct usb_interface *interface;
	size_t xfer_size;
	int ret = 0;

	if (data == NULL)
		return -ENODEV;

	if (data->state == MEMX_XFER_STATE_ABORT)
		return -EAGAIN;

	interface = data->interface;
	rx_start_time = ktime_get();
	mutex_lock(&data->readlock);

	/* set up our urb */
	usb_fill_bulk_urb(data->rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_IN_EP),
			data->rbuffer, MAX_READ_SIZE, memx_rxcomplete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit RX URB");
		mutex_unlock(&data->readlock);
		return -1;
	}

	/* Remove Timeout since there might be suspend in the middle*/
	ret = wait_event_interruptible(data->read_wq, completion_done(&data->rx_comp) || (data->state == MEMX_XFER_STATE_ABORT));

	if (data->state == MEMX_XFER_STATE_ABORT) {
		data->state = MEMX_XFER_STATE_NORMAL;
		usb_kill_urb(data->rxurb);
		reinit_completion(&data->rx_comp);
		mutex_unlock(&data->readlock);
		return -EAGAIN;
	}

	if (ret < 0) {
		/*Terminate by system, clear rxurb and reinit completion*/
		usb_kill_urb(data->rxurb);
		reinit_completion(&data->rx_comp);
		mutex_unlock(&data->readlock);
		return 0;
	}

	xfer_size = data->rxurb->actual_length;

	if (xfer_size != 0) {
		if (copy_to_user(user_buffer, data->rbuffer, xfer_size)) {
			mutex_unlock(&data->readlock);
			return -1;
		}
	}

	reinit_completion(&data->rx_comp);
	mutex_unlock(&data->readlock);
	rx_end_time = ktime_get();
	THROUGHPUT_ADD(rx_size, xfer_size);
	THROUGHPUT_ADD(rx_time_us, ktime_us_delta(rx_end_time, rx_start_time));
	return xfer_size;
}

static ssize_t memx_dummy_read(struct memx_data *data)
{
	int ret = 0;

	if (data == NULL)
		return -ENODEV;

	mutex_lock(&data->readlock);

	/* set up our urb */
	usb_fill_bulk_urb(data->rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_IN_EP),
			data->rbuffer, MAX_READ_SIZE, memx_rxcomplete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit RX URB");
		mutex_unlock(&data->readlock);
		return -1;
	}

	ret = wait_for_completion_timeout(&data->rx_comp, msecs_to_jiffies(100));

	if (!ret) {
		usb_kill_urb(data->rxurb);
		reinit_completion(&data->rx_comp);
		mutex_unlock(&data->readlock);
		return 0;
	}

	mutex_unlock(&data->readlock);
	return 1;
}

static ssize_t memx_write(struct file *file, const char __user *user_buffer, size_t n_bytes, loff_t *pos)
{
	struct memx_data *data = file->private_data;
	struct usb_interface *interface;
	unsigned char *buf = data->tbuffer;
	int xfer_total_size = n_bytes;
	size_t xfer_size;
	int i = 0;
	ssize_t ret_size = 0;

	if (data == NULL)
		return -ENODEV;

	interface = data->interface;

	tx_start_time = ktime_get();
	mutex_lock(&data->cfglock);

	/*Send the reset data*/
	while (xfer_total_size > 0) {
		if (xfer_total_size > data->max_mpuout_size)
			xfer_size = data->max_mpuout_size;
		else
			xfer_size = xfer_total_size;

		if (copy_from_user(buf, user_buffer + i*data->max_mpuout_size, xfer_size)) {
			pr_err("Copy from user failed!");
			ret_size = 0;
			break;
		}

		/* set up our urb */
		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_OUT_EP),
				buf, xfer_size, memx_txcomplete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit TX URB");
			ret_size = 0;
			break;
		}

		if (!wait_for_completion_timeout(&data->tx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait tx timeout\n");

			usb_kill_urb(data->txurb);
			reinit_completion(&data->tx_comp);
			ret_size = 0;
			break;
		}

		ret_size += xfer_size;
		xfer_total_size -= xfer_size;
		i++;
	}

	mutex_unlock(&data->cfglock);
	tx_end_time = ktime_get();
	THROUGHPUT_ADD(tx_size, ret_size);
	THROUGHPUT_ADD(tx_time_us, ktime_us_delta(tx_end_time, tx_start_time));

	return ret_size;
}

static int memx_release(struct inode *inode, struct file *file)
{
	struct memx_data *dev = file->private_data;

	if (dev == NULL)
		return -ENODEV;

	mutex_lock(&dev->cfglock);

	dev->reference_count--;

	mutex_unlock(&dev->cfglock);

	return 0;
}

static int memx_open(struct inode *inode, struct file *file)
{
	struct memx_data *dev;
	struct usb_interface *interface;

	/* get the interface from minor number and driver information */
	interface = usb_find_interface(&memx_ai_driver, iminor(inode));
	if (!interface)
		return -ENODEV;

	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	mutex_lock(&dev->cfglock);

	if (dev->reference_count == 0) {
		dev->state = MEMX_XFER_STATE_NORMAL;
	}

	dev->reference_count++;

	mutex_unlock(&dev->cfglock);

	file->private_data = dev;

	return 0;
}


static long memx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct memx_data *data = file->private_data;
	struct usb_interface *interface = data->interface;
	struct memx_mpu_size mpu_size;
	struct memx_reg memx_reg;
	struct memx_flow memx_flow;
	struct memx_chip_id memx_chip_id;
	struct hw_info hw_info = {0};
	uint32_t cfg_header[2] = {0};
	long ret = 0;
	int i;

	if (!interface)
		return -ENODEV;

	mutex_lock(&data->cfglock);

	switch (cmd) {
	case MEMX_DRIVER_MPU_IN_SIZE: {
		uint32_t mpu_in_transfer_size = sizeof(uint32_t) * (MEMX_TOTAL_FLOW_COUNT + 2);

		if (copy_from_user(&mpu_size, (struct memx_mpu_size *)arg, sizeof(struct memx_mpu_size))) {
			pr_err("MEMX_DRIVER_MPU_IN_SIZE, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}

		for (i = 0; i < MEMX_TOTAL_FLOW_COUNT; i++) {
			data->flow_size[i] = mpu_size.flow_size[i];
			data->buffer_size[i] = mpu_size.buffer_size[i];
		}

		data->usb_first_chip_pipeline_flag = mpu_size.usb_first_chip_pipeline_flag;
		data->usb_last_chip_pingpong_flag = mpu_size.usb_last_chip_pingpong_flag;
		data->max_mpuin_size = data->buffer_size[0];

		cfg_header[0] = FWCFG_ID_MPU_INSIZE;
		cfg_header[1] = mpu_in_transfer_size;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			break;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		memcpy(data->tbuffer, data->buffer_size, mpu_in_transfer_size);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, mpu_in_transfer_size, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			break;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		clear_fw_id(data);
    }
	break;
	case MEMX_DRIVER_MPU_OUT_SIZE:
		if (copy_from_user(&mpu_size, (struct memx_mpu_size *)arg, sizeof(struct memx_mpu_size))) {
			pr_err("MEMX_DRIVER_MPU_IN_SIZE, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		data->max_mpuout_size = mpu_size.cfg_size;
	break;
	case MEMX_FW_MPU_OUT_SIZE:
		if (copy_from_user(&mpu_size, (struct memx_mpu_size *)arg, sizeof(struct memx_mpu_size))) {
			pr_err("MEMX_DRIVER_MPU_IN_SIZE, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		cfg_header[0] = FWCFG_ID_MPU_OUTSIZE;
		cfg_header[1] = 4;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			break;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		memcpy(data->tbuffer, &mpu_size.cfg_size, 4);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 4, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			break;
		}

		if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		clear_fw_id(data);
		break;
	case MEMX_DOWNLOAD_FIRMWARE: {
		struct memx_firmware_bin memx_bin = {0};

		if (copy_from_user(&memx_bin, (struct memx_firmware_bin *)arg, sizeof(struct memx_firmware_bin))) {
			pr_err("MEMX_DOWNLOAD_BIN, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		if (memx_flash_download(data, cmd, &memx_bin)) {
			pr_err("download failed!\n");
			ret = -ENODEV;
		}
	}
	break;
	case MEMX_DOWNLOAD_DFP: {
		struct memx_firmware_bin memx_bin = {0};

		if (copy_from_user(&memx_bin, (struct memx_firmware_bin *)arg, sizeof(struct memx_firmware_bin))) {
			pr_err("MEMX_DOWNLOAD_BIN, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		if (memx_flash_download(data, cmd, &memx_bin)) {
			pr_err("download failed!\n");
			ret = -ENODEV;
		}
	}
	break;
	case MEMX_RUNTIMEDWN_DFP:
	{
		struct memx_bin memx_bin = {};

		if (copy_from_user(&memx_bin, (struct memx_bin *)arg, sizeof(struct memx_bin))) {
			pr_err("MEMX_RUNTIMEDWN_DFP, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}

		if ((memx_bin.dfp_src == DFP_FROM_SEPERATE_WTMEM) || (memx_bin.dfp_src == DFP_FROM_SEPERATE_CONFIG)) {
			if (memx_bin.dfp_src == DFP_FROM_SEPERATE_WTMEM)
				pr_debug("DFP From SEPERATE WTMEM\n");
			else
				pr_debug("DFP From SEPERATE CONFIG\n");

			if (memx_seperate_dfp_download(data, memx_bin.buf + SEP_LEN_OFS, memx_bin.dfp_cnt, memx_bin.dfp_src)) {
				pr_err("runtime dfp download seperately failed!\n");
				ret = -ENODEV;
			}
		}
	}
	break;
	case MEMX_WRITE_REG:
	case MEMX_READ_REG:
		if (copy_from_user(&memx_reg, (struct memx_reg *)arg, sizeof(struct memx_reg))) {
			pr_err("MEMX_WRITE_REG, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}

		ret = memx_reg_operation(data, memx_reg.buf_addr, memx_reg.reg_start, memx_reg.size, cmd);
	break;
	case MEMX_IFMAP_FLOW:
		if (copy_from_user(&memx_flow, (struct memx_flow *)arg, sizeof(struct memx_flow))) {
			pr_err("MEMX_IFMAP_FLOW, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}

		if (memx_flow.flow_id > MEMX_TOTAL_FLOW_COUNT) {
			pr_err("MEMX_IFMAP_FLOW, invalid flow_id %d\n", memx_flow.flow_id);
			ret = -ENOMEM;
			break;
		}

		data->flow_id = memx_flow.flow_id;
	break;
	case MEMX_ABORT_TRANSFER:
		memx_abort_transfer(data);
	break;
	case MEMX_READ_CHIP_ID:
		if (copy_from_user(&memx_reg, (struct memx_reg *)arg, sizeof(struct memx_reg))) {
			pr_err("MEMX_READ_CHIP_ID, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		ret = memx_reg_operation(data, memx_reg.buf_addr, memx_reg.reg_start, memx_reg.size, MEMX_READ_REG);
	break;
	case MEMX_SET_CHIP_ID:
		if (copy_from_user(&memx_chip_id, (struct memx_chip_id *)arg, sizeof(memx_chip_id))) {
			pr_err("MEMX_SET_CHIP_ID, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		set_dfp_chip_id(data, memx_chip_id.chip_id);
	break;
	case MEMX_GET_FWUPDATE_STATUS:
		if (copy_from_user(&memx_reg, (struct memx_reg *)arg, sizeof(struct memx_reg))) {
			pr_err("MEMX_GET_FWUPDATE_STATUS, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		ret = memx_get_fwupdate_status(data, memx_reg.buf_addr);
	break;
	case MEMX_CONFIG_MPU_GROUP:
		if (copy_from_user(&hw_info, (struct hw_info *)arg, sizeof(struct hw_info))) {
			pr_err("MEMX_CONFIG_MPU_GROUP, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		if (memx_usb_config_mpu_group(data, &hw_info)) {
			pr_err("USB MEMX_CONFIG_MPU_GROUP failed!\n");
			ret = -EIO;
			break;
		}
		if (copy_to_user((void __user *)arg, &hw_info, sizeof(struct hw_info))) {
			pr_err("MEMX_CONFIG_MPU_GROUP, copy_to_user fail\n");
			ret = -ENOMEM;
			break;
		}
	break;
	case MEMX_RESET_DEVICE:
		ret = memx_usb_reset_device(data);
	break;
	case MEMX_SET_THROUGHPUT_INFO: {
		if (copy_from_user(&udrv_throughput_info, (struct memx_throughput_info *)arg, sizeof(struct memx_throughput_info))) {
			pr_err("MEMX_SET_THROUGHPUT_INFO, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
	}
	break;
	case MEMX_DUMMY_READ: {
		ret = memx_dummy_read(data);
	}
	break;
	default:
		ret = -ENOTTY;
	break;
	}

		mutex_unlock(&data->cfglock);

		return ret;
}

static const struct file_operations memx_fops = {
	.owner		  = THIS_MODULE,
	.read		   = memx_read,
	.write		  = memx_write,
	.open		   = memx_open,
	.unlocked_ioctl = memx_ioctl,
	.release		= memx_release,
	.llseek		 = default_llseek,
};

static struct usb_class_driver memxchip_g0class = {
	.name =		 "memxchip%d",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
	.devnode = memx_usb_devnode,
};

static struct usb_class_driver memxchip_g0classo = {
	.name =		 "memxchipo0",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g0classi = {
	.name =		 "memxchipi0",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g1class = {
	.name =		 "memxchip1",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g1classo = {
	.name =		 "memxchipo1",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g1classi = {
	.name =		 "memxchipi1",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g2class = {
	.name =		 "memxchip2",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g2classo = {
	.name =		 "memxchipo2",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g2classi = {
	.name =		 "memxchipi2",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g3class = {
	.name =		 "memxchip3",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g3classo = {
	.name =		 "memxchipo3",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_g3classi = {
	.name =		 "memxchipi3",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static void add_device_link(u32 device_idx, void *pDevice)
{
	device_link[device_idx] = pDevice;
}

static void delete_device_link(u32 device_idx)
{
	device_link[device_idx] = 0;
}

static bool check_device_all_empty(void)
{
	u32 index = 0;
	bool is_empty = true;

	for (index = 0; index < MAX_CHIP_NUM; index++) {
		if (device_link[index] != 0) {
			is_empty = false;
			break;
		}
	}

	return is_empty;
}

static int memx_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct memx_data *data;
	struct device *pDevice;

	s32 ret = 0;

	//printk("match_flags		0x%X\n",id->match_flags);
	//printk("idVendor		   0x%X\n",id->idVendor);
	//printk("idProduct		  0x%X\n",id->idProduct);
	//printk("bcdDevice_lo	   0x%X\n",id->bcdDevice_lo);
	//printk("bcdDevice_hi	   0x%X\n",id->bcdDevice_hi);
	//printk("bDeviceClass	   0x%X\n",id->bDeviceClass);
	//printk("bDeviceSubClass	0x%X\n",id->bDeviceSubClass);
	//printk("bDeviceProtocol	0x%X\n",id->bDeviceProtocol);
	//printk("bInterfaceClass	0x%X\n",id->bInterfaceClass);
	//printk("bInterfaceSubClass 0x%X\n",id->bInterfaceSubClass);
	//printk("bInterfaceProtocol 0x%X\n",id->bInterfaceProtocol);
	//printk("bInterfaceNumber   0x%X\n",id->bInterfaceNumber);
	//printk("CurEP Number	   %d\n",intf->cur_altsetting->desc.bNumEndpoints);

	data = devm_kzalloc(&intf->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->state = MEMX_XFER_STATE_NORMAL;
	data->udev  = udev;

	data->txurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!data->txurb)
		return -ENOMEM;

	data->txurb->transfer_flags = URB_ZERO_PACKET;

	data->tbuffer = kzalloc(MAX_OPS_SIZE, GFP_KERNEL);
	if (!data->tbuffer) {
		//pr_err("Can't allocate memory for tx");
		usb_free_urb(data->txurb);
		return -ENOMEM;
	}

	if (id->idProduct != ROLE_MX3PLUS_ZSBL_DEV) {
		data->rxurb = usb_alloc_urb(0, GFP_KERNEL);
		if (!data->rxurb)
			return -ENOMEM;

		data->fw_txurb = usb_alloc_urb(0, GFP_KERNEL);
		if (!data->fw_txurb)
			return -ENOMEM;

		data->fw_rxurb = usb_alloc_urb(0, GFP_KERNEL);
		if (!data->fw_rxurb)
			return -ENOMEM;

		data->rbuffer = kzalloc(MAX_READ_SIZE, GFP_KERNEL);
		if (!data->rbuffer) {
			//pr_err("Can't allocate memory for rx");
			usb_free_urb(data->rxurb);
			return -ENOMEM;
		}

		data->fw_wbuffer = kzalloc(MAX_OPS_SIZE, GFP_KERNEL);
		if (!data->fw_wbuffer) {
			//pr_err("Can't allocate memory for tx");
			usb_free_urb(data->fw_txurb);
			return -ENOMEM;
		}

		data->fw_rbuffer = kzalloc(MAX_OPS_SIZE, GFP_KERNEL);
		if (!data->fw_rbuffer) {
			//pr_err("Can't allocate memory for fwrx");
			usb_free_urb(data->fw_rxurb);
			return -ENOMEM;
		}

		init_completion(&data->rx_comp);
		init_completion(&data->fwrx_comp);

		mutex_init(&data->cfglock);
		mutex_init(&data->readlock);

		init_waitqueue_head(&data->read_wq);
	}

	init_completion(&data->fw_comp);
	init_completion(&data->tx_comp);

	data->max_mpuin_size = MAX_MPUIN_SIZE;
	data->max_mpuout_size = MAX_MPUOUT_SIZE;
	data->interface = usb_get_intf(intf);

	usb_set_intfdata(intf, data);

	data->product_id = id->idProduct;

	switch (id->idProduct) {
	case ROLE_G0_MULTI_FIRST:
		usb_register_dev(intf, &memxchip_g0classo);
	break;
	case ROLE_G1_MULTI_FIRST:
		usb_register_dev(intf, &memxchip_g1classo);
	break;
	case ROLE_G2_MULTI_FIRST:
		usb_register_dev(intf, &memxchip_g2classo);
	break;
	case ROLE_G3_MULTI_FIRST:
		usb_register_dev(intf, &memxchip_g3classo);
	break;
	case ROLE_G0_MULTI_LAST:
		usb_register_dev(intf, &memxchip_g0classi);
	break;
	case ROLE_G1_MULTI_LAST:
		usb_register_dev(intf, &memxchip_g1classi);
	break;
	case ROLE_G2_MULTI_LAST:
		usb_register_dev(intf, &memxchip_g2classi);
	break;
	case ROLE_G3_MULTI_LAST:
		usb_register_dev(intf, &memxchip_g3classi);
	break;
	case ROLE_G1_SINGLE_DEV:
		usb_register_dev(intf, &memxchip_g1class);
	break;
	case ROLE_G2_SINGLE_DEV:
		usb_register_dev(intf, &memxchip_g2class);
	break;
	case ROLE_G3_SINGLE_DEV:
		usb_register_dev(intf, &memxchip_g3class);
	break;
	case ROLE_MX3PLUS_ZSBL_DEV:
		return memx_firmware_init(data);
	break;
	default:
		usb_register_dev(intf, &memxchip_g0class);
	break;
	}

	mutex_lock(&device_mutex);
	if (check_device_all_empty() == true) {
		ret = alloc_chrdev_region(&g_feature_devno, 0, MAX_CHIP_NUM, USB_FEATURE_NAME);
		if (ret < 0) {
			pr_err("Can't allocate dev %d\n", ret);
			mutex_unlock(&device_mutex);
			return -ENOMEM;
		}

#if  KERNEL_VERSION(6, 4, 0) > _LINUX_VERSION_CODE_
		memx_feature_class = class_create(THIS_MODULE, USB_FEATURE_NAME);
#else
		memx_feature_class = class_create(USB_FEATURE_NAME);
#endif
		if (memx_feature_class == NULL) {
			pr_err("Can't create memx_feature_class %d\n", ret);
			mutex_unlock(&device_mutex);
			return -ENOMEM;
		}
		memx_feature_class->devnode = memx_usb_devnode;
	}

	cdev_init(&data->feature_cdev, &memx_feature_fops);
	cdev_add(&data->feature_cdev, MKDEV(MAJOR(g_feature_devno), intf->minor), 1);
	pDevice = device_create(memx_feature_class, NULL, MKDEV(MAJOR(g_feature_devno), intf->minor), NULL, DEVICE_NODE_NAME_USB "_feature", intf->minor);
	if (IS_ERR(pDevice)) {
		pr_err("Can't create feature device %d\n", IS_ERR(pDevice));
		return -ENOMEM;
	}
	add_device_link(intf->minor, pDevice);
	mutex_unlock(&device_mutex);

	data->fs.type = g_drv_fs_type;
	data->fs.debug_en = fs_debug_en;

	data->reference_count = 0;

	if ((data->fs.type != MEMX_FS_HIF_NONE) && (!memx_dbgfs_enable(data))) {
		s32 err = 0;

		data->fs.dbgfs_en = 1;
		err = memx_fs_init(data);
		if (err) {
			pr_err("Probing: creating file system failed.\n");
			data->fs.dbgfs_en = 0;
		}

		if (data->fs.dbgfs_en) {
			int i;

			if ((pcie_lane_no > 2) || (pcie_lane_no < 1) || (pcie_lane_speed > 3) || (pcie_lane_speed < 1)) {
				pcie_lane_no = 2;
				pcie_lane_speed = 3;
			}
			for (i = 0; i < data->chipcnt; i++)
				memx_send_memxcmd(data, i, MXCNST_MEMXW_CMD, MXCNST_PCIEDYNMCTRL, (0x80010000 + ((0x1 << pcie_lane_no)-1) + ((pcie_lane_speed-1)<<24)));

			if (pcie_aspm) {
				for (i = data->chipcnt-1; i >= 0; i--)
					memx_send_memxcmd(data, i, MXCNST_MEMXW_CMD, MXCNST_ASPMCTRL, pcie_aspm&0xF);
			}
		}

	} else {
		pr_info("None dbgfs\r\n");
	}

	return 0;
}
static void memx_disconnect(struct usb_interface *intf)
{
	struct memx_data *data = usb_get_intfdata(intf);

	if (data->fs.dbgfs_en) {
		memx_fs_deinit(data);
		data->fs.dbgfs_en = 0;
	}

	usb_kill_urb(data->txurb);
	usb_kill_urb(data->rxurb);
	usb_kill_urb(data->fw_txurb);
	usb_kill_urb(data->fw_rxurb);

	if (data->product_id != ROLE_MX3PLUS_ZSBL_DEV) {
		device_destroy(memx_feature_class, MKDEV(MAJOR(g_feature_devno), intf->minor));
		cdev_del(&data->feature_cdev);
		mutex_lock(&device_mutex);
		delete_device_link(intf->minor);
		if (check_device_all_empty() == true) {
			class_destroy(memx_feature_class);
			memx_feature_class = NULL;
			unregister_chrdev_region(g_feature_devno, MAX_CHIP_NUM);
		}
		mutex_unlock(&device_mutex);
	}


	switch (data->product_id) {
	case ROLE_G0_MULTI_FIRST:
		usb_deregister_dev(intf, &memxchip_g0classo);
	break;
	case ROLE_G1_MULTI_FIRST:
		usb_deregister_dev(intf, &memxchip_g1classo);
	break;
	case ROLE_G2_MULTI_FIRST:
		usb_deregister_dev(intf, &memxchip_g2classo);
	break;
	case ROLE_G3_MULTI_FIRST:
		usb_deregister_dev(intf, &memxchip_g3classo);
	break;
	case ROLE_G0_MULTI_LAST:
		usb_deregister_dev(intf, &memxchip_g0classi);
	break;
	case ROLE_G1_MULTI_LAST:
		usb_deregister_dev(intf, &memxchip_g1classi);
	break;
	case ROLE_G2_MULTI_LAST:
		usb_deregister_dev(intf, &memxchip_g2classi);
	break;
	case ROLE_G3_MULTI_LAST:
		usb_deregister_dev(intf, &memxchip_g3classi);
	break;
	case ROLE_G1_SINGLE_DEV:
		usb_deregister_dev(intf, &memxchip_g1class);
	break;
	case ROLE_G2_SINGLE_DEV:
		usb_deregister_dev(intf, &memxchip_g2class);
	break;
	case ROLE_G3_SINGLE_DEV:
		usb_deregister_dev(intf, &memxchip_g3class);
	break;
	case ROLE_MX3PLUS_ZSBL_DEV:
	break;
	default:
		usb_deregister_dev(intf, &memxchip_g0class);
	break;
	}

	usb_free_urb(data->txurb);
	kfree(data->tbuffer);
	if (data->product_id != ROLE_MX3PLUS_ZSBL_DEV) {
		usb_free_urb(data->fw_txurb);
		usb_free_urb(data->fw_rxurb);
		usb_free_urb(data->rxurb);
		kfree(data->fw_wbuffer);
		kfree(data->fw_rbuffer);
		kfree(data->rbuffer);
		wake_up_interruptible(&data->read_wq);
	}

	usb_set_intfdata(intf, NULL);
}

struct usb_driver memx_ai_driver = {
	.name		= "memx_ai_chip",
	.probe		= memx_probe,
	.disconnect	= memx_disconnect,
	.id_table	= memx_table,
	.disable_hub_initiated_lpm = 0,
};

module_usb_driver(memx_ai_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Howard Chang");
MODULE_VERSION(VERSION);
MODULE_DESCRIPTION("MemryX Cascade Chip Driver");
