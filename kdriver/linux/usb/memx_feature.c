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
#include "memx_cascade_usb.h"

extern struct usb_driver memx_ai_driver;

#define STATUS_COMPLETE (0)
#define STATUS_WRITE_TIMEOUT (1)
#define STATUS_READ_TIMEOUT (2)
#define STATUS_SUBMIT_ERROR (3)

static int memx_feature_read_chip0(struct memx_data *data, uint32_t *buffer, uint32_t address, uint32_t size);
int memx_admin_trigger(struct transport_cmd *pCmd, struct memx_data *data);

int memx_admin_trigger(struct transport_cmd *pCmd, struct memx_data *data)
{
    uint32_t cfg_header[2] = {0};
    uint32_t status = STATUS_COMPLETE;
    int ret = 0;

    if(pCmd->SQ.opCode == MEMX_ADMIN_CMD_SET_FEATURE){
        cfg_header[0] = FWCFG_ID_SET_FEATURE; //backward
    } else if (pCmd->SQ.opCode == MEMX_ADMIN_CMD_GET_FEATURE){
        cfg_header[0] = FWCFG_ID_GET_FEATURE; //backward
    } else {
        cfg_header[0] = FWCFG_ID_ADM_COMMAND;
    }
	cfg_header[1] = sizeof(struct transport_cmd);

	memcpy(data->fw_wbuffer, cfg_header, 8);

	usb_fill_bulk_urb(data->fw_txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
		data->fw_wbuffer, 8, memx_complete, data);

	/* send the data out the bulk port */
	if (usb_submit_urb(data->fw_txurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit admin header");
		status = STATUS_SUBMIT_ERROR;
	} else {
        if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
            pr_err("Admin header timeout\n");
            status = STATUS_WRITE_TIMEOUT;
        } else {
            memcpy(data->fw_wbuffer, pCmd, sizeof(struct transport_cmd));
            /* set up our urb */
            usb_fill_bulk_urb(data->fw_txurb, data->udev, usb_sndbulkpipe(data->udev, MEMX_FW_OUT_EP),
                    data->fw_wbuffer, sizeof(struct transport_cmd), memx_complete, data);
            /* send the data out the bulk port */
            if (usb_submit_urb(data->fw_txurb, GFP_KERNEL) < 0) {
                pr_err("Can't submit admin TX URB");
                status = STATUS_SUBMIT_ERROR;
            } else {
                if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
                    pr_err("Admin TX URB timeout\n");
                    status = STATUS_WRITE_TIMEOUT;
                } else {
                    usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		                    data->fw_rbuffer, MAX_OPS_SIZE, memx_fwrxcomplete, data);
                    /* get the data in the bulk port */
                    if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
                        pr_err("Can't submit admin RX URB");
                        status = STATUS_SUBMIT_ERROR;
                    } else {
                        if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(MXCNST_TIMEOUT30S))) {
                            pr_err("Admin RX URB timeout\n");
                            status = STATUS_READ_TIMEOUT;
                        } else {
                            memcpy(pCmd, data->fw_rbuffer, sizeof(struct transport_cmd));
                        }
                    }
                }
            }
        }
    }

    if (status != STATUS_COMPLETE) {
        ret = -ENODEV;

        if (status == STATUS_WRITE_TIMEOUT) {
            usb_kill_urb(data->fw_txurb);
            reinit_completion(&data->fw_comp);
        } else if (status == STATUS_READ_TIMEOUT) {
            usb_kill_urb(data->fw_rxurb);
            reinit_completion(&data->fwrx_comp);
        }
    }

	return ret;
}

static long _admin_get_feature(struct memx_data *memx_dev, struct transport_cmd *pCmd){
    long    ret     = 0;
    char version[8] = VERSION;

    switch(pCmd->SQ.subOpCode){
        case FID_DEVICE_THROUGHPUT:
            pCmd->CQ.data[0]  = tx_time_us;
            pCmd->CQ.data[1]  = tx_size / KBYTE;
            pCmd->CQ.data[2] = rx_time_us;
            pCmd->CQ.data[3] = rx_size / KBYTE;
            pCmd->CQ.data[4] = udrv_throughput_info.stream_write_us;
            pCmd->CQ.data[5] = udrv_throughput_info.stream_write_kb;
            pCmd->CQ.data[6] = udrv_throughput_info.stream_read_us;
            pCmd->CQ.data[7] = udrv_throughput_info.stream_read_kb;
            tx_time_us = 0;
            tx_size = 0;
            rx_time_us = 0;
            rx_size = 0;
            udrv_throughput_info.stream_write_us = 0;
            udrv_throughput_info.stream_write_kb = 0;
            udrv_throughput_info.stream_read_us = 0;
            udrv_throughput_info.stream_read_kb = 0;
            break;
        case FID_DEVICE_INTERFACE_INFO:
            //pass through
            break;
        case FID_DEVICE_INFO:
            ret = memx_admin_trigger(pCmd, memx_dev);
            memcpy(&(pCmd->CQ.data[7]), version, sizeof(unsigned int));
            memcpy(&(pCmd->CQ.data[8]), &version[sizeof(unsigned int)], sizeof(unsigned int));
            break;
        case FID_DEVICE_MPU_UTILIZATION:
            ret = memx_feature_read_chip0(memx_dev, &pCmd->CQ.data[0], MXCNST_MPUUTIL_BASE, 64);
            if (ret == 0) {
                pCmd->CQ.status = ERROR_STATUS_NO_ERROR;
            } else {
                pCmd->CQ.status = ERROR_STATUS_UNKNOWN_FAIL;
            }
            break;
        default:
            ret = memx_admin_trigger(pCmd, memx_dev);
            break;
    }

	return ret;
}

static long _admin_set_feature(struct memx_data *memx_dev, struct transport_cmd *pCmd){
    long    ret     = 0;

    switch(pCmd->SQ.subOpCode){
        default:
            ret = memx_admin_trigger(pCmd, memx_dev);
            break;
    }

	return ret;
}

static long _admin_command(struct memx_data *memx_dev, struct transport_cmd *pCmd){
    long ret = 0;

    mutex_lock(&memx_dev->cfglock);

    switch (pCmd->SQ.opCode) {
        case MEMX_ADMIN_CMD_SET_FEATURE:
            ret = _admin_set_feature(memx_dev, pCmd);
            break;
        case MEMX_ADMIN_CMD_GET_FEATURE:
            ret = _admin_get_feature(memx_dev, pCmd);
            break;
        case MEMX_ADMIN_CMD_DEVIOCTRL:
            if (pCmd->SQ.subOpCode == FID_DEVICE_I2C_TRANSCEIVE) {
                pCmd->SQ.opCode = MEMX_ADMIN_CMD_SET_FEATURE;
                ret = _admin_set_feature(memx_dev, pCmd);
            } else {
                ret = -EFAULT;
                pr_err(" _admin_command: non-support admin cmd(%u) sub(%u)\n", pCmd->SQ.opCode, pCmd->SQ.subOpCode);
            }
            break;
        case MEMX_ADMIN_CMD_DOWNLOAD_DFP: //usb interface not support
        case MEMX_ADMIN_CMD_SELFTEST:     //usb interface not support
        default:
            ret = -EFAULT;
            pr_err(" _admin_command: non-support admin cmd(%u)\n", pCmd->SQ.opCode);
            break;
    }

    mutex_unlock(&memx_dev->cfglock);

    if ((!ret) && (pCmd->SQ.subOpCode == FID_DEVICE_I2C_TRANSCEIVE)) {
        ret = memx_read_chip0(memx_dev, (uint32_t *)pCmd, 0x400FD200, sizeof(struct transport_cmd));
        if (!ret)
            memcpy(&pCmd->CQ.data[3],&pCmd->SQ.cdw3,16);
    }

	return ret;
}

static long memx_admin_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct memx_data        *data       = NULL;
    struct usb_interface    *interface  = NULL;
    struct transport_cmd    admin_cmd   = {0};
    long                    ret         = 0;

    if (!file || !file->private_data){
        ret = -ENODEV;
    } else {
        data        = file->private_data;
        interface   = data->interface;
    }

    if(!interface){
        ret = -ENODEV;
    } else {
        if (copy_from_user(&admin_cmd, (struct transport_cmd *)arg, sizeof(struct transport_cmd))) {
            pr_err("memx_admin_ioctl, copy_from_user fail\n");
            ret = -ENOMEM;
        } else {
            switch (cmd) {
                case MEMX_GET_DEVICE_FEATURE: //backward
                case MEMX_SET_DEVICE_FEATURE: //backward
                case MEMX_ADMIN_COMMAND:
                    ret = _admin_command(data, &admin_cmd);
                    break;

                default:
                    ret = -EFAULT;
                    pr_err(" feature_ioctl: non-support ioctl cmd(0x%x)\n", cmd);
            }

            if (copy_to_user((void __user*)arg, &admin_cmd, sizeof(struct transport_cmd))) {
                pr_err("memx_admin_ioctl, copy_to_user fail\n");
                ret = -ENOMEM;
            }
        }
    }
    return ret;
}

static int memx_admin_open(struct inode *inode, struct file *file)
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


	file->private_data = dev;

	return 0;
}

static s32 memx_admin_release(struct inode *inode, struct file *file)
{
#ifdef DEBUG
	u32 major = imajor(inode);
	u32 minor = iminor(inode);
#endif

#ifdef DEBUG
	pr_info(" feature_close: (%d-%d) success.\n", major, minor);
#endif
	return 0;
}

static int memx_feature_read_chip0(struct memx_data *data, uint32_t *buffer, uint32_t address, uint32_t size)
{
	uint32_t cfg_header[4] = {0};
	int ret = 0;

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
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fw_comp, msecs_to_jiffies(1000))) {
		pr_err("wait memxcmd timeout\n");
		ret = -ENOMEM;
		return ret;
	}

	usb_fill_bulk_urb(data->fw_rxurb, data->udev, usb_rcvbulkpipe(data->udev, MEMX_FW_IN_EP),
		data->fw_rbuffer, size, memx_fwrxcomplete, data);

	/* get the data in the bulk port */
	if (usb_submit_urb(data->fw_rxurb, GFP_KERNEL) < 0) {
		pr_err("Can't submit memxcmd read");
		ret = -ENOMEM;
		return ret;
	}

	if (!wait_for_completion_timeout(&data->fwrx_comp, msecs_to_jiffies(500))) {
		usb_kill_urb(data->fw_rxurb);
		pr_info("Can't wait memx_get_logbuffer response\n");
		ret = -ENOMEM;
		return ret;
	}

	if (data->fw_rxurb->actual_length == size) {
		memcpy(buffer, (uint32_t *)data->fw_rbuffer, size);
		return 0;

	} else {
		pr_err("memx_get_logbuffer response fail\r\n");
		return -ENOMEM;
	}
}

struct file_operations memx_feature_fops = {
owner: THIS_MODULE,
unlocked_ioctl : memx_admin_ioctl,
open : memx_admin_open,
release : memx_admin_release,
};
