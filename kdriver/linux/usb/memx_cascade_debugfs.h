/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _MEMX_CASCADE_DEBUGFS_H_
#define _MEMX_CASCADE_DEBUGFS_H_

struct memx_data;
int memx_dbgfs_enable(struct memx_data *data);
int memx_send_memxcmd(struct memx_data *data, uint32_t chipid, uint32_t command, uint32_t parameter, uint32_t parameter2);
int memx_read_chip0(struct memx_data *data, uint32_t *buffer, uint32_t address, uint32_t size);
int memx_get_logbuffer(struct memx_data *data, uint32_t chipid, u8 *buffer, uint32_t maxsize);

#endif
