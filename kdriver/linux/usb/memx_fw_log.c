// SPDX-License-Identifier: GPL-2.0+
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include "../include/memx_ioctl.h"
#include "memx_cascade_usb.h"
#include "memx_fs.h"
#include "memx_fw_log.h"
#include "memx_cascade_debugfs.h"

#define MEMX_DBGLOG_CHIP_BUFFER_SIZE(chipid) (0x1000+(chipid*0))

s32 memx_fw_log_dump(struct memx_data *memx_dev, u8 chip_id)
{
	u8 *log_buf = NULL;	/* this is circular buffer */
	u8 *log_buf_seq = NULL;/* linear arranged */
	u32 *wp_addr = NULL, *rp_addr = NULL;
	u32 wp_value = 0, rp_value = 0;
	s32 size = 0;
	s32 total = 0, total_tmp = 0;

	if (!memx_dev) {
		pr_err("fw_log_dump: memx_dev is NULL\n");
		return -EINVAL;
	}

	if (chip_id >= memx_dev->chipcnt) {
		pr_err("%s: invalid mpu chip id(%u), should be 0 to (%u - 1)\n", __func__, chip_id, memx_dev->chipcnt);
		return -EINVAL;
	}

	log_buf = kzalloc(8192, GFP_KERNEL);
	if ((log_buf != NULL) && (memx_get_logbuffer(memx_dev, chip_id, log_buf, 8192) == 0x1008)) {

		wp_addr = (u32 *)(log_buf+0x1000);
		rp_addr = (u32 *)(log_buf+0x1004);

		wp_value = *wp_addr;
		rp_value = *rp_addr;
		total = (s32)((wp_value - rp_value) & (MEMX_DBGLOG_CHIP_BUFFER_SIZE(chip_id) - 1));
		pr_err("=============CHIP(%u) WP(%d) RP(%d) Total(%d)=============\n", chip_id, wp_value, rp_value, total);

		log_buf_seq = kmalloc(total+1, GFP_KERNEL);
		if (!log_buf_seq) {
			//pr_err("kmalloc for log_buf_seq failed\n");
			kfree(log_buf);
			return -ENOMEM;
		}

		if (rp_value+total < MEMX_DBGLOG_CHIP_BUFFER_SIZE(chip_id)) {
			memcpy(log_buf_seq, log_buf + rp_value, total);
		} else {
			u32 slice1 = MEMX_DBGLOG_CHIP_BUFFER_SIZE(chip_id) - rp_value;
			u32 slice2 = total - slice1;

			memcpy(log_buf_seq, log_buf + rp_value, slice1);
			memcpy(log_buf_seq+slice1, log_buf, slice2);
		}
		log_buf_seq[total] = 0;

		rp_value = 0;
		total_tmp = total;
		while (total_tmp) {
			size = pr_err("%s", log_buf_seq + rp_value);
			total_tmp -= (size + 2);
			rp_value = ((rp_value + (size + 2)));
			if (rp_value >= total)
				break;
		}
		*rp_addr = wp_value;
		pr_err("============================================================\n");
		kfree(log_buf_seq);
		kfree(log_buf);
	}

	return 0;
}
