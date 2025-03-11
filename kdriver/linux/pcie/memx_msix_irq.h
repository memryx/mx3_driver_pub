/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _MEMX_MSIX_IRQ_H_
#define _MEMX_MSIX_IRQ_H_
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#define MEMRYX_MIN_MSIX_NUMBER (1)
#define MEMRYX_MAX_MSIX_NUMBER (33)

struct memx_irq_entry {
	const char *name;       // descript irq purpose
	irq_handler_t handler;
};

struct memx_msix_entry {
	u32 msg_addr;
	u32 msg_upper_addr;
	u32 msg_data;
	u32 vector_control;
};

struct memx_pending_entry {
	u32 lower_32_bits;
	u32 upper_32_bits;
};

// Note: Our Hw support MSI-X only.
struct memx_interrupt {
	u32 init_done;

	u32 max_hw_support_msix_count;
	u32 curr_used_msix_count;

	s32 irq[MEMRYX_MAX_MSIX_NUMBER];
	u8 enable[MEMRYX_MAX_MSIX_NUMBER];

	struct memx_msix_entry *msix_table;
	u32 msix_table_phy_addr;
	struct memx_pending_entry *pending_table;
	u32 pba_table_phy_addr;
	u32 pba_table_offset;

	spinlock_t lock;
};

struct memx_pcie_dev;
s32 memx_init_msix_irq(struct memx_pcie_dev *memx_dev);
void memx_deinit_msix_irq(struct memx_pcie_dev *memx_dev);

s32 memx_enable_device_msix_capability(struct memx_pcie_dev *memx_dev);
void memx_disable_device_msix_capability(struct memx_pcie_dev *memx_dev);

#endif
