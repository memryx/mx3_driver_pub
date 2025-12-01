// SPDX-License-Identifier: GPL-2.0+
#include <linux/module.h>    // included for all kernel modules
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include "../include/memx_ioctl.h"
#include "memx_cascade_usb.h"


int memx_dbgfs_enable(struct memx_data *data)
{
	uint32_t cfg_header[4] = {0};
	int ret = 0;
	uint32_t i, *buf32;

	mutex_lock(&data->cfglock);

	cfg_header[0] = DBGFS_ID_ENABLE;
	cfg_header[1] = 0;
	cfg_header[2] = MXCNST_MAGIC1;
	cfg_header[3] = MXCNST_MAGIC2;

	memcpy(data->tbuffer, cfg_header, 16);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 16, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit txurb\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(1000))) {
		pr_err("wait completion timeout\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		data->fw_rbuffer, MAX_OPS_SIZE, memx_fwrxcomplete, data);

	/* get the data in the bulk port */
	if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit data read");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(500))) {
		usb_kill_urb(data->fw_rxurb);
		pr_info("Can't wait debugfs response\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	buf32 = (uint32_t *)data->fw_rbuffer;
	if (data->fw_rxurb->actual_length == 16) {
		for (i = 1; i < 4; i++) {
			if (buf32[i] != cfg_header[i]) {
				pr_err("check_dbgfs_support failed\n");
				ret = -ENOMEM;
				mutex_unlock(&data->cfglock);
				return ret;
			}
		}
		data->chipcnt = buf32[0];
		pr_info("usb debugfs found %d chips\n", data->chipcnt);
		mutex_unlock(&data->cfglock);
		return 0;
	}

	mutex_unlock(&data->cfglock);
	return -ENOMEM;
}

int memx_send_memxcmd(struct memx_data *data, uint32_t chipid, uint32_t command, uint32_t parameter, uint32_t parameter2)
{
	uint32_t cfg_header[5] = {0};
	int ret = 0;
	uint32_t *buf32;

	mutex_lock(&data->cfglock);

	cfg_header[0] = DBGFS_ID_MEMXCMD;
	cfg_header[1] = chipid;/* chip id */
	cfg_header[2] = command;
	cfg_header[3] = parameter;
	cfg_header[4] = parameter2;

	memcpy(data->tbuffer, cfg_header, 20);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 20, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(1000))) {
		pr_err("wait memxcmd timeout\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		data->fw_rbuffer, MAX_OPS_SIZE, memx_fwrxcomplete, data);

	/* get the data in the bulk port */
	if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd read");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(500))) {
		usb_kill_urb(data->fw_rxurb);
		pr_info("Can't wait memxcmd 0x%X response\n", command);
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	buf32 = (uint32_t *)data->fw_rbuffer;
	if ((data->fw_rxurb->actual_length == 4) && (buf32[0] == 1)) {
		mutex_unlock(&data->cfglock);
		return 0;

	} else {
		pr_err("MEMXCMD 0x%X response fail\r\n", command);
		mutex_unlock(&data->cfglock);
		return -ENOMEM;
	}
}

int memx_read_chip0(struct memx_data *data, uint32_t *buffer, uint32_t address, uint32_t size)
{
	uint32_t cfg_header[4] = {0};
	int ret = 0;

	mutex_lock(&data->cfglock);

	cfg_header[0] = DBGFS_ID_RDADDR;
	cfg_header[1] = 0;/* chip id */
	cfg_header[2] = address;
	cfg_header[3] = size;

	memcpy(data->tbuffer, cfg_header, 16);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 16, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(1000))) {
		pr_err("wait memxcmd timeout\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		data->fw_rbuffer, size, memx_fwrxcomplete, data);

	/* get the data in the bulk port */
	if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd read");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(500))) {
		usb_kill_urb(data->fw_rxurb);
		pr_info("Can't wait memx_get_logbuffer response\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (data->fw_rxurb->actual_length == size) {
		memcpy(buffer, (uint32_t *)data->fw_rbuffer, size);
		mutex_unlock(&data->cfglock);
		return 0;

	} else {
		pr_err("memx_get_logbuffer response fail\r\n");
		mutex_unlock(&data->cfglock);
		return -ENOMEM;
	}
}

int memx_get_logbuffer(struct memx_data *data, uint32_t chipid, u8 *buffer, uint32_t maxsize)
{
	uint32_t cfg_header[5] = {0};
	int ret = 0;

	mutex_lock(&data->cfglock);

	cfg_header[0] = DBGFS_ID_GETLOG;
	cfg_header[1] = chipid;/* chip id */
	cfg_header[2] = 0;
	cfg_header[3] = 0;
	cfg_header[4] = 0;

	memcpy(data->tbuffer, cfg_header, 20);

	usb_fill_bulk_urb(data->txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->tbuffer, 20, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(1000))) {
		pr_err("wait memxcmd timeout\n");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		data->fw_rbuffer, maxsize, memx_fwrxcomplete, data);

	/* get the data in the bulk port */
	if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd read");
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(500))) {
		usb_kill_urb(data->fw_rxurb);
		pr_info("Can't wait %s response\n", __func__);
		ret = -ENOMEM;
		mutex_unlock(&data->cfglock);
		return ret;
	}

	if (data->fw_rxurb->actual_length > 0) {
		memcpy(buffer, (uint32_t *)data->fw_rbuffer, data->fw_rxurb->actual_length);
		//pr_err("GETLOG recv size %d\r\n", data->fw_rxurb->actual_length);
		mutex_unlock(&data->cfglock);
		return data->fw_rxurb->actual_length;

	} else {
		pr_err("%s response fail\r\n", __func__);
		mutex_unlock(&data->cfglock);
		return -ENOMEM;
	}
}


