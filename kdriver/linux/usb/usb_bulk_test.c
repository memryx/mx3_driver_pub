// SPDX-License-Identifier: GPL-2.0+
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
#include "../include/memx_ioctl.h"
#define VERSION "1.10"

#ifdef CONFIG_USB_DYNAMIC_MINORS
#define USB_MEMXCHIP3_MINOR_BASE 0
#else
#define USB_MEMXCHIP3_MINOR_BASE 192
#endif

#define MEMX_IN_EP			0x81
#define MEMX_OUT_EP		   0x01
#define MEMX_FW_OUT_EP		0x02
#define MAX_OPS_SIZE		  (64*1024)
// #define MAX_MPUIN_SIZE  (19200)
// #define MAX_MPUOUT_SIZE (57600)
#define FWCFG_ID_CLR		  0x952700
#define FWCFG_ID_FW		   0x952701
#define FWCFG_ID_DFP		  0x952702
#define FWCFG_ID_MPU_INSIZE   0x952703
#define FWCFG_ID_MPU_OUTSIZE  0x952704
#define FWCFG_ID_DFP_CHIPID   0x952705
#define FWCFG_ID_DFP_CFGSIZE  0x952706
#define FWCFG_ID_DFP_WMEMADR  0x952707
#define FWCFG_ID_DFP_WTMEMSZ  0x952708
#define FWCFG_ID_DFP_RESETMPU 0x952709
#define FWCFG_ID_DFP_RECFGMPU 0x95270A
#define DFP_FLASH_OFFSET	  0x20000
#define MPU_CHIP_ID_BASE	  1
static unsigned long MAX_MPUIN_SIZE = 18000;
static unsigned long MAX_MPUOUT_SIZE = 54000;

static unsigned int dfp_dwn;
module_param(dfp_dwn, uint, 0);
MODULE_PARM_DESC(dfp_dwn, "Process DFP Download if dfp_dwn = 1, default is 0");

static unsigned int fw_dwn;
module_param(fw_dwn, uint, 0);
MODULE_PARM_DESC(fw_dwn, "Process FW Download if fw_dwn = 1, default is 0");

static unsigned int loop;
module_param(loop, uint, 0);
MODULE_PARM_DESC(loop, "Process bulk loopback if loop = 1, default is 0");

static unsigned int emu;
module_param(emu, uint, 0);
MODULE_PARM_DESC(emu, "Process emulation flow if emu = 1, default is 0");

static unsigned int dfp_dir;
module_param(dfp_dir, uint, 0);
MODULE_PARM_DESC(dfp_dir, "Process DFP Direct Write if dfp_dir = 1, default is 0");

static unsigned int frame_size = 252;
module_param(frame_size, uint, 0);
MODULE_PARM_DESC(fw_dwn, "frame size, default is 252");

DECLARE_COMPLETION(fw_comp);
DECLARE_COMPLETION(tx_comp);
DECLARE_COMPLETION(rx_comp);

static struct usb_driver memx_ai_driver;

#define ROLE_SINGLE_DEV  0x4006
#define ROLE_MULTI_FIRST 0x4007
#define ROLE_MULTI_LAST  0x4008

static const struct usb_device_id memx_table[] = {
	/* MemryX CHIP-3 */
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_SINGLE_DEV) }, /*x1 case*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_MULTI_FIRST) }, /*xN first*/
	{ USB_DEVICE(DEVICE_VENDOR_ID, ROLE_MULTI_LAST) }, /*xN last*/

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

#define MAX_WT_CNT   6
#define DFP_CFG_SZ   0xD0
#define DFP_OFS(N)   (0x10 + N*0xD0)
#define MAX_CFG_SZ   MAX_OPS_SIZE

struct DFP_DATA {
	uint32_t mpu_rg_cfg_adr;
	uint32_t mpu_rg_cfg_sze;
	uint32_t mpu_cg_wt_mem_count;
	uint32_t reserved0;
	struct DFP_MPU_WT_MEM mpu_cg_wt_mem_data[MAX_WT_CNT];
} __packed;

struct memx_data {
	struct usb_interface *interface;
	struct usb_device	*udev;
	unsigned long		state;
	unsigned long		tx_size;
	unsigned long		rx_size;
	struct urb		   *txurb;
	struct urb		   *rxurb;
	unsigned char		*tbuffer;
	unsigned char		*rbuffer;
	struct work_struct   fw_work;
	struct work_struct   tx_work;
	struct work_struct   rx_work;
	uint32_t			 flow_size[MEMX_TOTAL_FLOW_COUNT];
	uint32_t			 buffer_size[MEMX_TOTAL_FLOW_COUNT];
	struct mutex		 cfglock;
};

static void memx_complete(struct urb *urb)
{
	// struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&fw_comp);
}

static void memx_txcomplete(struct urb *urb)
{
	// struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&tx_comp);
}

static void memx_rxcomplete(struct urb *urb)
{
	// struct memx_data *data = urb->context;
	// struct usb_device *udev = urb->dev;
	complete(&rx_comp);
}


static void tx_thread(struct work_struct *work)
{
	struct memx_data *data =
		container_of(work, struct memx_data, tx_work);
	unsigned long size = data->tx_size;
	int i = 0;

	if (emu) {
		size = frame_size*2;
		for (i = 0; i < frame_size; i++)
			data->tbuffer[i] = i & 0xFF;

		for (i = frame_size; i < frame_size*2; i++)
			data->tbuffer[i] = 0x22;
	}

	/* set up our urb */
	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_OUT_EP),
			data->tbuffer, size, memx_txcomplete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit TX URB");
		return;
	}

	if (!wait_for_completion_timeout(&tx_comp, msecs_to_jiffies(30000))) {
		pr_err("wait tx timeout\n");
		return;
	}

	if (emu)
		schedule_work(&data->tx_work);
}

static void rx_thread(struct work_struct *work)
{
	struct memx_data *data =
		container_of(work, struct memx_data, rx_work);
	int i = 0;

	if (emu) {
		for (i = 0; i < MAX_OPS_SIZE; i++)
			data->rbuffer[i] = 0x0;
	}

	/* set up our urb */
	usb_fill_bulk_urb(data->rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_IN_EP),
			data->rbuffer, MAX_OPS_SIZE, memx_rxcomplete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit RX URB");
		return;
	}

	if (!wait_for_completion_timeout(&rx_comp, msecs_to_jiffies(30000))) {
		pr_err("wait rx timeout\n");
		return;
	}

	if (emu)
		schedule_work(&data->rx_work);
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

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}
}

static void reset_mpu(struct memx_data *data, uint32_t id)
{
	uint32_t cfg_header[2] = {0};
	uint32_t chip_id = id;

	cfg_header[0] = FWCFG_ID_DFP_RESETMPU;
	cfg_header[1] = 4;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		&chip_id, 4, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg dfp");
		return;
	}

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg dfp timeout\n");
		return;
	}

	clear_fw_id(data);
}

static void recfg_mpu(struct memx_data *data, uint32_t id)
{
	uint32_t cfg_header[2] = {0};
	uint32_t chip_id = id;

	cfg_header[0] = FWCFG_ID_DFP_RECFGMPU;
	cfg_header[1] = 4;

	memcpy(data->tbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		&chip_id, 4, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg dfp");
		return;
	}

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg dfp timeout\n");
		return;
	}

	clear_fw_id(data);
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

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		&chip_id, 4, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg dfp");
		return;
	}

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
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

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		&wtmem_addr, 4, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit cfg clr");
		return;
	}

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
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

	if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
		pr_err("wait cfg clr timeout\n");
		return;
	}
}

static void fw_thread(struct work_struct *work)
{
	struct memx_data *data =
		container_of(work, struct memx_data, fw_work);
	int i = 0;

	if (dfp_dwn || fw_dwn) {
		const struct firmware *firmware;
		int fw_total_size = 0;
		uint32_t cfg_header[2] = {0};

		if (fw_dwn) {
			if (request_firmware(&firmware, "cascade.bin", &data->udev->dev) < 0) {
				pr_err("cascade bin request failed");
				return;
			}
		} else {
			if (request_firmware(&firmware, "cascade.dfp", &data->udev->dev) < 0) {
				pr_err("cascade dfp request failed");
				return;
			}
		}

		if (fw_dwn)
			cfg_header[0] = FWCFG_ID_FW;
		else
			cfg_header[0] = FWCFG_ID_DFP;
		cfg_header[1] = firmware->size;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			return;
		}

		if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
			pr_err("wait cfg dfp timeout\n");
			return;
		}

		memset(data->tbuffer, 0x0, MAX_OPS_SIZE);

		pr_err("data %p size %zu", firmware->data, firmware->size);

		fw_total_size = firmware->size;

		while (fw_total_size > 0) {
			/* set up our urb */
			if (fw_total_size > MAX_OPS_SIZE) {
				memcpy(data->tbuffer, firmware->data + i*MAX_OPS_SIZE, MAX_OPS_SIZE);
				usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
					data->tbuffer, MAX_OPS_SIZE, memx_complete, data);
			} else {
				memcpy(data->tbuffer, firmware->data + i*MAX_OPS_SIZE, fw_total_size);
				usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
					data->tbuffer, fw_total_size, memx_complete, data);
			}

			/* send the data out the bulk port */
			if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
				pr_err("Can't submit TX URB");
				break;
			}

			if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
				pr_err("wait tx timeout\n");
				break;
			}

			fw_total_size -= MAX_OPS_SIZE;
			i++;
			memset(data->tbuffer, 0x0, MAX_OPS_SIZE);
		}

		clear_fw_id(data);

		release_firmware(firmware);
	} else if (loop) {
		int size = 1;
		int ret = 0;

		for (i = 0; i < MAX_OPS_SIZE; i++)
			data->tbuffer[i] = i & 0xFF;

		while (1) {
			pr_err("test size %d\n", size);
			/* set up our urb */
			usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_OUT_EP),
					data->tbuffer, size, memx_complete, data);

			/* send the data out the bulk port */
			if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
				pr_err("Can't submit TX URB");
				break;
			}

			if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
				pr_err("wait tx timeout\n");
				break;
			}

			for (i = 0; i < MAX_OPS_SIZE; i++)
				data->rbuffer[i] = 0x0;

			/* set up our urb */
			usb_fill_bulk_urb(data->rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_IN_EP),
					data->rbuffer, size, memx_complete, data);

			/* send the data out the bulk port */
			if (usb_submit_urb(data->rxurb, GFP_KERNEL) < 0) {
				pr_err("Can't submit RX URB");
				break;
			}

			if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
				pr_err("wait rx timeout\n");
				break;
			}

			ret = memcmp(data->tbuffer, data->rbuffer, size);
			if (ret) {
				pr_err("test size %d Fail\n", size);
				break;

			} else
				pr_err("test size %d PASS\n", size);

			if (size == 64*1024)
				break;

			else
				size++;
		}
	} else if (dfp_dir) {
		const struct firmware *firmware;
		const uint8_t *buffer;
		uint32_t cfg_header[2] = {0};
		struct DFP_DATA dfp_data[1] = {0};
		uint32_t dfp_count = 0;
		int i = 0, j = 0;

		if (request_firmware(&firmware, "coco.dfp", &data->udev->dev) < 0) {
			pr_err("coco dfp request failed");
			return;
		}

		buffer = firmware->data;
		dfp_count = *(uint32_t *) buffer;

		/*program each dfp data*/
		for (i = 0; i < dfp_count; i++) {
			uint8_t *dfp_base_buf = NULL;
			uint32_t dfp_cfg_addr = 0;
			uint32_t dfp_wtmem_addr = 0;
			uint32_t reg_write_addr = 0;
			uint32_t cfg_size, wtmem_count, wtmem_sze, wtmemflg_sze;

			reset_mpu(data, i + MPU_CHIP_ID_BASE);

			/*read config value*/
			dfp_base_buf = (void *)(buffer + DFP_OFS(i));

			memcpy((void *)dfp_data, (const void *)dfp_base_buf, DFP_CFG_SZ);

			set_dfp_chip_id(data, i + MPU_CHIP_ID_BASE);

			/*program rg_cfg*/
			cfg_size = dfp_data->mpu_rg_cfg_sze;

			dfp_cfg_addr = dfp_data->mpu_rg_cfg_adr - DFP_FLASH_OFFSET;

			if (cfg_size == 0) {
				pr_err("invalid cfg size %d\r\n", cfg_size);
				return;
			}

			cfg_header[0] = FWCFG_ID_DFP_CFGSIZE;
			cfg_header[1] = cfg_size;

			memcpy(data->tbuffer, cfg_header, 8);

			usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
				data->tbuffer, 8, memx_complete, data);

			/* send the data out the bulk port */
			if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
				pr_err("Can't submit cfg size");
				return;
			}

			if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
				pr_err("wait cfg size timeout\n");
				return;
			}

			while (cfg_size > 0) {
				if (cfg_size > MAX_CFG_SZ) {
					memcpy(data->tbuffer, (uint8_t *)(buffer+dfp_cfg_addr), MAX_CFG_SZ);
					usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
						data->tbuffer, MAX_CFG_SZ, memx_complete, data);

					dfp_cfg_addr += MAX_CFG_SZ;
					cfg_size -= MAX_CFG_SZ;
				} else {
					memcpy(data->tbuffer, (uint8_t *)(buffer+dfp_cfg_addr), cfg_size);
					usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
						data->tbuffer, cfg_size, memx_complete, data);

					cfg_size = 0;
				}

				/* send the data out the bulk port */
				if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
					pr_err("Can't submit cfg data");
					return;
				}

				if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
					pr_err("wait cfg data timeout\n");
					return;
				}
			}

			clear_fw_id(data);

			/*program wt_mem*/
			wtmem_count = dfp_data->mpu_cg_wt_mem_count;

			if (wtmem_count == 0) {
				pr_err("invalid wtmem count %d\r\n", wtmem_count);
				return;
			}

			for (j = 0; j < wtmem_count; j++) {
				wtmem_sze = dfp_data->mpu_cg_wt_mem_data[j].mpu_cg_wt_mem_data_sze;
				dfp_wtmem_addr = dfp_data->mpu_cg_wt_mem_data[j].mpu_cg_wt_mem_data_adr - DFP_FLASH_OFFSET;
				reg_write_addr = dfp_data->mpu_cg_wt_mem_data[j].mpu_cg_wt_mem_data_reg;

				if ((wtmem_sze == 0) || (wtmem_sze == 0xFFFFFFFF))
					continue;

				while (wtmem_sze > 0) {
					set_wtmem_addr(data, FWCFG_ID_DFP_WMEMADR, reg_write_addr);

					if (wtmem_sze > MAX_CFG_SZ) {
						set_wtmem_size(data, MAX_CFG_SZ);
						memcpy(data->tbuffer, (uint8_t *)(buffer+dfp_wtmem_addr), MAX_CFG_SZ);
						usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
							data->tbuffer, MAX_CFG_SZ, memx_complete, data);

						dfp_wtmem_addr += MAX_CFG_SZ;

						reg_write_addr += MAX_CFG_SZ;

						wtmem_sze -= MAX_CFG_SZ;
					} else {
						set_wtmem_size(data, wtmem_sze);
						memcpy(data->tbuffer, (uint8_t *)(buffer+dfp_wtmem_addr), wtmem_sze);
						usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
							data->tbuffer, wtmem_sze, memx_complete, data);

						wtmem_sze = 0;
					}

					/* send the data out the bulk port */
					if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
						pr_err("Can't submit wtmem data");
						return;
					}

					if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
						pr_err("wait wtmem data timeout\n");
						return;
					}
					clear_fw_id(data);
				}

				wtmemflg_sze = dfp_data->mpu_cg_wt_mem_data[j].mpu_cg_wt_memflg_data_sze;
				dfp_wtmem_addr = dfp_data->mpu_cg_wt_mem_data[j].mpu_cg_wt_memflg_data_adr - DFP_FLASH_OFFSET;
				reg_write_addr = dfp_data->mpu_cg_wt_mem_data[j].mpu_cg_wt_memflg_data_reg;

				if ((wtmemflg_sze == 0) || (wtmemflg_sze == 0xFFFFFFFF))
					continue;

				while (wtmemflg_sze > 0) {
					set_wtmem_addr(data, FWCFG_ID_DFP_WMEMADR, reg_write_addr);

					if (wtmemflg_sze > MAX_CFG_SZ) {
						set_wtmem_size(data, MAX_CFG_SZ);
						memcpy(data->tbuffer, (uint8_t *)(buffer+dfp_wtmem_addr), MAX_CFG_SZ);
						usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
							data->tbuffer, MAX_CFG_SZ, memx_complete, data);

						dfp_wtmem_addr += MAX_CFG_SZ;

						reg_write_addr += MAX_CFG_SZ;

						wtmemflg_sze -= MAX_CFG_SZ;
					} else {
						set_wtmem_size(data, MAX_CFG_SZ);
						memcpy(data->tbuffer, (uint8_t *)(buffer+dfp_wtmem_addr), wtmemflg_sze);
						usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
							data->tbuffer, wtmemflg_sze, memx_complete, data);

						wtmemflg_sze = 0;
					}
					/* send the data out the bulk port */
					if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
						pr_err("Can't submit wtmemflg data");
						return;
					}

					if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
						pr_err("wait wtmemflg data timeout\n");
						return;
					}
					clear_fw_id(data);
				}
			}
			recfg_mpu(data, i + MPU_CHIP_ID_BASE);
		}
	}
}

static ssize_t memx_read(struct file *file, char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct memx_data *data = file->private_data;
	struct usb_interface *interface;
	size_t xfer_size;

	if (data == NULL)
		return -ENODEV;

	interface = data->interface;

	/* set up our urb */
	usb_fill_bulk_urb(data->rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_IN_EP),
			data->rbuffer, MAX_OPS_SIZE, memx_rxcomplete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit RX URB");
		return -1;
	}

	if (!wait_for_completion_timeout(&rx_comp, msecs_to_jiffies(300000))) {
		pr_err("wait rx timeout\n");
		return -1;
	}

	xfer_size = data->rxurb->actual_length;

	if (copy_to_user(user_buffer, data->rbuffer, xfer_size))
		return -1;

	return xfer_size;
}

static ssize_t memx_write(struct file *file, const char __user *user_buffer, size_t n_bytes, loff_t *pos)
{
	struct memx_data *data = file->private_data;
	struct usb_interface *interface;
	char *buf = data->tbuffer;
	int xfer_total_size = n_bytes;
	size_t xfer_size;
	int i = 0;
	ssize_t ret_size = 0;

	if (data == NULL)
		return -ENODEV;

	interface = data->interface;

	mutex_lock(&data->cfglock);

	while (xfer_total_size > 0) {
		if (xfer_total_size > MAX_MPUOUT_SIZE)
			xfer_size = MAX_MPUOUT_SIZE;
		else
			xfer_size = xfer_total_size;

		if (copy_from_user(buf, user_buffer + i*MAX_MPUOUT_SIZE, xfer_size)) {
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

		if (!wait_for_completion_timeout(&tx_comp, msecs_to_jiffies(300000))) {
			pr_err("wait tx timeout\n");
			ret_size = 0;
			break;
		}

		ret_size += xfer_size;
		xfer_total_size -= xfer_size;
		i++;
	}

	mutex_unlock(&data->cfglock);

	return ret_size;
}

static int memx_release(struct inode *inode, struct file *file)
{
	struct memx_data *dev = file->private_data;
	struct usb_interface *interface;

	if (dev == NULL)
		return -ENODEV;

	interface = dev->interface;

	return 0;
}

static int memx_open(struct inode *inode, struct file *file)
{
	struct memx_data *dev;
	struct usb_interface *interface;

	//printk("%s %d\n", __func__, __LINE__);

	/* get the interface from minor number and driver information */
	interface = usb_find_interface(&memx_ai_driver, iminor(inode));
	if (!interface)
		return -ENODEV;

	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	file->private_data = dev;

	return 0;
}


static long memx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct memx_data *data = file->private_data;
	struct usb_interface *interface = data->interface;
	memx_mpu_size_t mpu_size;
	uint32_t cfg_header[2] = {0};
	long ret = 0;
	int i;

	if (!interface)
		return -ENODEV;

	mutex_lock(&data->cfglock);

	switch (cmd) {
	case MEMX_DRIVER_MPU_IN_SIZE:
		if (copy_from_user(&mpu_size, (memx_mpu_size_t *)arg, sizeof(memx_mpu_size_t))) {
			pr_err("MEMX_DRIVER_MPU_IN_SIZE, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		for (i = 0; i < MEMX_TOTAL_FLOW_COUNT; i++) {
			data->flow_size[i] = mpu_size.flow_size[i];
			data->buffer_size[i] = mpu_size.buffer_size[i];
		}

		MAX_MPUIN_SIZE = data->buffer_size[0];

		cfg_header[0] = FWCFG_ID_MPU_INSIZE;
		cfg_header[1] = 4*MEMX_TOTAL_FLOW_COUNT;

		memcpy(data->tbuffer, cfg_header, 8);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 8, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			break;
		}

		if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		memcpy(data->tbuffer, data->buffer_size, 4*MEMX_TOTAL_FLOW_COUNT);

		usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
			data->tbuffer, 4*MEMX_TOTAL_FLOW_COUNT, memx_complete, data);

		/* send the data out the bulk port */
		if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
			pr_err("Can't submit cfg dfp");
			ret = -ENOMEM;
			break;
		}

		if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		clear_fw_id(data);
	break;
	case MEMX_DRIVER_MPU_OUT_SIZE:
		if (copy_from_user(&mpu_size, (memx_mpu_size_t *)arg, sizeof(memx_mpu_size_t))) {
			pr_err("MEMX_DRIVER_MPU_IN_SIZE, copy_from_user fail\n");
			ret = -ENOMEM;
			break;
		}
		MAX_MPUOUT_SIZE = mpu_size.cfg_size;
	break;
	case MEMX_FW_MPU_OUT_SIZE:
		if (copy_from_user(&mpu_size, (memx_mpu_size_t *)arg, sizeof(memx_mpu_size_t))) {
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

		if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
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

		if (!wait_for_completion_timeout(&fw_comp, msecs_to_jiffies(30000))) {
			pr_err("wait cfg dfp timeout\n");
			ret = -ENOMEM;
			break;
		}

		clear_fw_id(data);
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

static struct usb_class_driver memxchip_class = {
	.name =		 "memxchip",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_classo = {
	.name =		 "memxchipo",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static struct usb_class_driver memxchip_classi = {
	.name =		 "memxchipi",
	.fops =		 &memx_fops,
	.minor_base =   USB_MEMXCHIP3_MINOR_BASE,
};

static int memx_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct memx_data *data;

	data = devm_kzalloc(&intf->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->udev  = udev;

	data->txurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!data->txurb)
		return -ENOMEM;

	data->txurb->transfer_flags = URB_ZERO_PACKET;

	data->rxurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!data->rxurb)
		return -ENOMEM;

	data->tbuffer = kzalloc(MAX_OPS_SIZE, GFP_KERNEL);
	if (!data->tbuffer) {
		//pr_err("Can't allocate memory for tx");
		usb_free_urb(data->txurb);
		return -ENOMEM;
	}

	data->rbuffer = kzalloc(MAX_OPS_SIZE, GFP_KERNEL);
	if (!data->rbuffer) {
		//pr_err("Can't allocate memory for rx");
		usb_free_urb(data->rxurb);
		return -ENOMEM;
	}

	mutex_init(&data->cfglock);

	data->interface = usb_get_intf(intf);

	usb_set_intfdata(intf, data);

	if (id->idProduct == ROLE_MULTI_FIRST)
		usb_register_dev(intf, &memxchip_classo);
	else if (id->idProduct == ROLE_MULTI_LAST)
		usb_register_dev(intf, &memxchip_classi);
	else
		usb_register_dev(intf, &memxchip_class);

	INIT_WORK(&data->fw_work, fw_thread);
	schedule_work(&data->fw_work);

	if (!loop && !fw_dwn && !dfp_dwn) {
		INIT_WORK(&data->tx_work, tx_thread);
		INIT_WORK(&data->rx_work, rx_thread);
		if (emu) {
			schedule_work(&data->tx_work);
			schedule_work(&data->rx_work);
		}
	}

	return 0;
}
static void memx_disconnect(struct usb_interface *intf)
{
	struct memx_data *data = usb_get_intfdata(intf);

	cancel_work_sync(&data->fw_work);
	if (!loop && !fw_dwn && !dfp_dwn) {
		cancel_work_sync(&data->tx_work);
		cancel_work_sync(&data->rx_work);
	}

	usb_kill_urb(data->txurb);
	usb_kill_urb(data->rxurb);

	usb_deregister_dev(intf, &memxchip_class);

	usb_set_intfdata(intf, NULL);

	usb_free_urb(data->txurb);
	usb_free_urb(data->rxurb);
	kfree(data->tbuffer);
	kfree(data->rbuffer);
}

static struct usb_driver memx_ai_driver = {
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
MODULE_DESCRIPTION("A Bulk Test sample");
