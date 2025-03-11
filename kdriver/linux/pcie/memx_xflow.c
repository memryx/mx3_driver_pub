// SPDX-License-Identifier: GPL-2.0+
#include <linux/semaphore.h>
#include "memx_pcie.h"
#include "memx_xflow.h"

static s32 memx_xflow_basic_check(struct memx_pcie_dev *memx_dev, u8 chip_id)
{
	if (!memx_dev || !memx_dev->pDev) {
		pr_err("xflow_basic_check: No Opened Device!\n");
		return -ENODEV;
	}
	if ((memx_dev->xflow_conf_bar_idx == MAX_BAR) || (memx_dev->xflow_vbuf_bar_idx == MAX_BAR)) {
		pr_err("xflow_basic_check: xflow bar idx(%u)(%u) invalid.\n", memx_dev->xflow_conf_bar_idx, memx_dev->xflow_vbuf_bar_idx);
		return -ENODEV;
	}
	if (!memx_dev->bar_info[memx_dev->xflow_conf_bar_idx].iobase ||
		!memx_dev->bar_info[memx_dev->xflow_conf_bar_idx].available) {
		pr_err("xflow_basic_check: bar_idx(%u) invalid.\n", memx_dev->xflow_conf_bar_idx);
		return -ENODEV;
	}
	if (!memx_dev->bar_info[memx_dev->xflow_vbuf_bar_idx].iobase ||
		!memx_dev->bar_info[memx_dev->xflow_vbuf_bar_idx].available) {
		pr_err("xflow_basic_check: bar_idx(%u) invalid.\n", memx_dev->xflow_vbuf_bar_idx);
		return -ENODEV;
	}
	if ((chip_id >= MAX_SUPPORT_CHIP_NUM) ||
		(memx_dev->bar_mode == MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM && chip_id >= 4)) {
		pr_err("xflow_basic_check: chip_id(%u) invalid.\n", chip_id);
		return -ENODEV;
	}
	return 0;
}

static void memx_xflow_set_access_mode(struct memx_pcie_dev *memx_dev, u8 chip_id, bool access_mpu)
{
	_VOLATILE_ u32 *control_register_addr = NULL;
	u32 barmapofs = memx_dev->xflow_conf_bar_offset;
	u8 bar_idx = memx_dev->xflow_conf_bar_idx;

	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_set_base_address: basic check fail\n");
		return;
	}

	control_register_addr = (_VOLATILE_ u32 *)(memx_dev->bar_info[bar_idx].iobase + GET_XFLOW_OFFSET(chip_id, true) + XFLOW_CONTROL_REGISTER_OFFSET - barmapofs);
	*control_register_addr = access_mpu ? 0 : 1;
}

static void memx_xflow_set_base_address(struct memx_pcie_dev *memx_dev, u8 chip_id, u32 base_addr)
{
	_VOLATILE_ u32 *base_addr_reg_addr = NULL;
	u32 barmapofs = memx_dev->xflow_conf_bar_offset;
	u8 bar_idx = memx_dev->xflow_conf_bar_idx;

	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_set_base_address: basic check fail\n");
		return;
	}

	base_addr_reg_addr = (_VOLATILE_ u32 *)(memx_dev->bar_info[bar_idx].iobase + GET_XFLOW_OFFSET(chip_id, true) + XFLOW_BASE_ADDRESS_REGISTER_OFFSET - barmapofs);
	*base_addr_reg_addr = base_addr;
}

static void memx_xflow_write_virtual_buffer_address(struct memx_pcie_dev *memx_dev, u8 chip_id, u32 base_addr_offset, u32 value)
{
	_VOLATILE_ u32 *virtual_buffer_target_address = NULL;
	u32 barmapofs = memx_dev->xflow_vbuf_bar_offset;
	u8 bar_idx = memx_dev->xflow_vbuf_bar_idx;

	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_write_virtual_buffer: basic check fail\n");
		return;
	}

	virtual_buffer_target_address = (_VOLATILE_ u32 *)(memx_dev->bar_info[bar_idx].iobase + GET_XFLOW_OFFSET(chip_id, false) + base_addr_offset - barmapofs);
	*virtual_buffer_target_address = value;
}

static u32 memx_xflow_read_virtual_buffer_address(struct memx_pcie_dev *memx_dev, u8 chip_id, u32 base_addr_offset)
{
	u32 result = 0;
	_VOLATILE_ u32 *virtual_buffer_target_address = NULL;
	u32 barmapofs = memx_dev->xflow_vbuf_bar_offset;
	u8 bar_idx = memx_dev->xflow_vbuf_bar_idx;

	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_read_virtual_buffer: basic check fail\n");
		return 0;
	}

	virtual_buffer_target_address = (_VOLATILE_ u32 *)(memx_dev->bar_info[bar_idx].iobase + GET_XFLOW_OFFSET(chip_id, false) + base_addr_offset - barmapofs);
	result = *virtual_buffer_target_address;
	return result;
}

void memx_xflow_write(struct memx_pcie_dev *memx_dev, u8 chip_id, u32 base_addr, u32 base_addr_offset, u32 value, bool access_mpu)
{
	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_write: basic check fail\n");
		return;
	}

	if (memx_dev->bar_mode == MEMXBAR_SRAM1MB) {
		unsigned long timeout = jiffies + (HZ);

		while (memx_sram_read(memx_dev, MEMX_EXTINFO_CMD_BASE) != MEMX_EXTCMD_COMPLETE) {
			if (time_after(jiffies, timeout)) {
				pr_err("ERROR: %s cmd timeout\n", __func__);
				return;
			}
			// speed up the hotplug
			if (memx_sram_read(memx_dev, MEMX_EXTINFO_CMD_BASE) == 0xFFFFFFFF)
				return;
		}

		memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE, (base_addr + base_addr_offset));
		memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE + 4, value);
		memx_sram_write(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD(chip_id, MEMX_EXTCMD_XFLOW_WRITE_REG, (access_mpu ? 0 : 1)));

		while (memx_sram_read(memx_dev, MEMX_EXTINFO_CMD_BASE) != MEMX_EXTCMD_COMPLETE) {
			if (time_after(jiffies, timeout)) {
				pr_err("ERROR: %s wait cmd complete timeout\n", __func__);
				return;
			}
			// speed up the hotplug
			if (memx_sram_read(memx_dev, MEMX_EXTINFO_CMD_BASE) == 0xFFFFFFFF)
				return;
		}
	} else {
		memx_xflow_set_access_mode(memx_dev, chip_id, access_mpu);
		memx_xflow_set_base_address(memx_dev, chip_id, base_addr);
		memx_xflow_write_virtual_buffer_address(memx_dev, chip_id, base_addr_offset, value);
		if (!access_mpu)
			memx_xflow_set_access_mode(memx_dev, chip_id, true);
	}
}

u32 memx_xflow_read(struct memx_pcie_dev *memx_dev, u8 chip_id, u32 base_addr, u32 base_addr_offset, bool access_mpu)
{
	u32 result = 0;

	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_read: get memx_dev->mutex fail\n");
		return 0;
	}

	if (memx_dev->bar_mode == MEMXBAR_SRAM1MB) {
		unsigned long timeout = jiffies + (HZ);

		while (memx_sram_read(memx_dev, MEMX_EXTINFO_CMD_BASE) != MEMX_EXTCMD_COMPLETE) {
			if (time_after(jiffies, timeout)) {
				pr_err("ERROR: %s cmd timeout 1\n", __func__);
				return 0;
			}
		}

		memx_sram_write(memx_dev, MEMX_EXTINFO_DATA_BASE, (base_addr+base_addr_offset));
		memx_sram_write(memx_dev, MEMX_EXTINFO_CMD_BASE, MEMX_EXTINFO_CMD(chip_id, MEMX_EXTCMD_XFLOW_READ_REG, (access_mpu ? 0 : 1)));

		timeout = jiffies + (HZ);
		while (memx_sram_read(memx_dev, MEMX_EXTINFO_CMD_BASE) != MEMX_EXTCMD_COMPLETE) {
			if (time_after(jiffies, timeout)) {
				pr_err("ERROR: %s cmd timeout 2\n", __func__);
				return 0;
			}
		}
		result = memx_sram_read(memx_dev, MEMX_EXTINFO_DATA_BASE + 4);
	} else {
		memx_xflow_set_access_mode(memx_dev, chip_id, access_mpu);
		memx_xflow_set_base_address(memx_dev, chip_id, base_addr);
		result = memx_xflow_read_virtual_buffer_address(memx_dev, chip_id, base_addr_offset);
		if (!access_mpu)
			memx_xflow_set_access_mode(memx_dev, chip_id, true);
	}
	return result;
}

void memx_xflow_trigger_mpu_sw_irq(struct memx_pcie_dev *memx_dev, u8 chip_id, enum xflow_mpu_sw_irq_idx sw_irq_idx)
{
	u32 write_value = 0;

	if (memx_xflow_basic_check(memx_dev, chip_id)) {
		pr_err("xflow_trigger_mpu_sw_irq: basic check fail.\n");
		return;
	}

	switch (sw_irq_idx) {
	case reset_device_idx_3:
		write_value = (0x1 << reset_device_idx_3);
	break;
	case fw_cmd_idx_4:
		write_value = (0x1 << fw_cmd_idx_4);
	break;
	case move_sram_data_to_di_port_idx_5:
		write_value = (0x1 << move_sram_data_to_di_port_idx_5);
	break;
	case init_wtmem_and_fmem_idx_6:
		write_value = (0x1 << init_wtmem_and_fmem_idx_6);
	break;
	case reset_mpu_idx_7:
		write_value = (0x1 << reset_mpu_idx_7);
	break;
	default:
		pr_err("Invalid sw_irq_idx(%u), it should not be used.\n", sw_irq_idx);
		return;
	}
	memx_xflow_write(memx_dev, chip_id, AHB_HUB_IRQ_EN_BASE, 0x0, write_value, true);
}

void memx_sram_write(struct memx_pcie_dev *memx_dev, u32 base_addr, u32 value)
{
	u8 bar_idx = memx_dev->sram_bar_idx;

	if (bar_idx == MAX_BAR) {
		pr_err("%s: Invalid bar_idx!\n", __func__);
		return;
	}
	if ((base_addr < (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_DATA_SRAM_OFFS)) || (base_addr >= (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_MAX_SIZE))) {
		pr_err("%s: Invalid base_addr!\n", __func__);
		return;
	}
	base_addr = base_addr - MEMX_CHIP_SRAM_BASE;

	*((_VOLATILE_ u32 *)(memx_dev->bar_info[bar_idx].iobase+base_addr)) = value;
}

u32 memx_sram_read(struct memx_pcie_dev *memx_dev, u32 base_addr)
{
	u8 bar_idx = memx_dev->sram_bar_idx;

	if (bar_idx == MAX_BAR) {
		pr_err("%s: Invalid bar_idx!\n", __func__);
		return 0;
	}
	if ((base_addr < (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_DATA_SRAM_OFFS)) || (base_addr >= (MEMX_CHIP_SRAM_BASE + MEMX_CHIP_SRAM_MAX_SIZE))) {
		pr_err("%s: Invalid base_addr!\n", __func__);
		return 0;
	}
	base_addr = base_addr - MEMX_CHIP_SRAM_BASE;

	return *((_VOLATILE_ u32 *)(memx_dev->bar_info[bar_idx].iobase+base_addr));
}
