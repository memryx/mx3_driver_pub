/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _MEMX_CASCADE_USB_H_
#define _MEMX_CASCADE_USB_H_

#include <linux/cdev.h>
#include "memx_cascade_debugfs.h"
#include "memx_fs.h"
#include "memx_fs_proc.h"

#define VERSION "2.34"

#define DEVICE_NODE_NAME "memx%d"
#define DEVICE_NODE_NAME_USB "memxchip%d"

#ifdef CONFIG_USB_DYNAMIC_MINORS
#define USB_MEMXCHIP3_MINOR_BASE 0
#else
#define USB_MEMXCHIP3_MINOR_BASE 192
#endif

#define FIRMWARE_BIN_NAME "cascade.bin"
#define USB_FEATURE_NAME "memx_usb_ai_chip_feature"

#define DEVICE_NODE_DEFAULT_ACCESS_RIGHT (0666)

#define DEVICE_VENDOR_ID	0x0559

#define ROLE_G0_SINGLE_DEV  0x4006
#define ROLE_G0_MULTI_FIRST 0x4007
#define ROLE_G0_MULTI_LAST  0x4008
#define ROLE_G1_SINGLE_DEV  0x4016
#define ROLE_G1_MULTI_FIRST 0x4017
#define ROLE_G1_MULTI_LAST  0x4018
#define ROLE_G2_SINGLE_DEV  0x4026
#define ROLE_G2_MULTI_FIRST 0x4027
#define ROLE_G2_MULTI_LAST  0x4028
#define ROLE_G3_SINGLE_DEV  0x4036
#define ROLE_G3_MULTI_FIRST 0x4037
#define ROLE_G3_MULTI_LAST  0x4038
#define ROLE_MX3PLUS_ZSBL_DEV  0x40FF

#define MEMX_IN_EP       0x81
#define MEMX_OUT_EP      0x01
#define MEMX_FW_IN_EP    0x82
#define MEMX_FW_OUT_EP   0x02
#define MAX_OPS_SIZE     (64*1024)
#define MAX_READ_SIZE    (128*1024)
#define DFP_FLASH_OFFSET 0x20000
#define MPU_CHIP_ID_BASE 1
#define MEMX_HEADER_SIZE 64
#define MAX_MPUIN_SIZE   18000
#define MAX_MPUOUT_SIZE  54000

#define FWCFG_ID_CLR            0x952700
#define FWCFG_ID_FW             0x952701
#define FWCFG_ID_DFP            0x952702
#define FWCFG_ID_MPU_INSIZE     0x952703
#define FWCFG_ID_MPU_OUTSIZE    0x952704
#define FWCFG_ID_DFP_CHIPID     0x952705
#define FWCFG_ID_DFP_CFGSIZE    0x952706
#define FWCFG_ID_DFP_WMEMADR    0x952707
#define FWCFG_ID_DFP_WTMEMSZ    0x952708
#define FWCFG_ID_DFP_RESETMPU   0x952709
#define FWCFG_ID_DFP_RECFGMPU   0x95270A
#define FWCFG_ID_WREG           0x95270B
#define FWCFG_ID_RREG_ADR       0x95270C
#define FWCFG_ID_RREG           0x95270D
#define FWCFG_ID_MPU_GROUP      0x95270E
#define FWCFG_ID_RESET_DEVICE   0x95270F
#define FWCFG_ID_GET_FEATURE    0x952710
#define FWCFG_ID_SET_FEATURE    0x952711
#define FWCFG_ID_ADM_COMMAND    0x952712

#define DBGFS_ID_ENABLE         0x6d6580
#define DBGFS_ID_RDADDR         0x6d6581
#define DBGFS_ID_WRADDR         0x6d6582
#define DBGFS_ID_MEMXCMD        0x6d6583
#define DBGFS_ID_GETLOG         0x6d6584

#define _LINUX_VERSION_CODE_		LINUX_VERSION_CODE
#define MXCNST_RWACCESS				(0666)
#define MXCNST_DATASRAM_BASE		(0x40080000)
#define MXCNST_COLDRSTCNT_ADDR		(0x400fdf40)
#define MXCNST_MANUFACTID			(0x20000404)
#define MXCNST_COMMITID				(0x40046f08)
#define MXCNST_BOOT_MODE			(0x20000100)
#define MXCNST_CHIP_VERSION			(0x20000500)
#define MXCNST_MEMX0_CMD			(0x6d656d30)
#define MXCNST_MEMXt_CMD			(0x6d656d74)
#define MXCNST_MEMXR_CMD			(0x6d656d72)
#define MXCNST_MEMXW_CMD			(0x6d656d77)
#define MXCNST_MEMXQ_CMD			(0x6D656D51)
#define MXCNST_MAGIC1				(0xABCDEF01)
#define MXCNST_MAGIC2				(0x23456789)
#define MXCNST_ASPMCTRL				(0x40046F6C)
#define MXCNST_PCIEDYNMCTRL			(0x40046F68)
#define MXCNST_TIMEOUT30S			(30000)
#define MXCNST_TIMEOUT3S			(3000)
#define MXCNST_RMTCMD_PARAM			(0x40046F48)
#define MXCNST_RMTCMD_COMMD			(0x40046F44)
#define MXCNST_MPUUTIL_BASE			(0x40046d00)
#define MXCNST_TEMP_BASE			(0x40046d40)

#define IS_WARNING_SKIP(STATUS) ((STATUS) == (-EIDRM))

enum memx_fs_hif_type {
	MEMX_FS_HIF_NONE,
	MEMX_FS_HIF_PROC,
	MEMX_FS_HIF_SYS,
};

enum memx_chip_ids {
	CHIP_ID0 = 0,
	CHIP_ID1,
	CHIP_ID2,
	CHIP_ID3,
	CHIP_ID4,
	CHIP_ID5,
	CHIP_ID6,
	CHIP_ID7,
	CHIP_ID8,
	CHIP_ID9,
	CHIP_ID10,
	CHIP_ID11,
	CHIP_ID12,
	CHIP_ID13,
	CHIP_ID14,
	CHIP_ID15,
	MAX_CHIP_NUM
};

union memx_fs_hif {
	struct {
		struct proc_dir_entry *root_dir;
		struct proc_dir_entry *cmd_entry;
		struct proc_dir_entry *verinfo_entry;
		struct proc_dir_entry *mpu_uti_entry;
		struct proc_dir_entry *temperature_entry;
		struct proc_dir_entry *debug_entry;
		struct proc_dir_entry *thermal_entry;
		struct proc_dir_entry *qspi_entry;
		struct proc_dir_entry *i2ctrl_entry;
		struct proc_dir_entry *gpio_entry;
		struct proc_dir_entry *throughput_entry;
	} proc;
	struct {
		struct kobject *root_dir;
	} sys;
};

struct memx_file_sys {
	u32						dbgfs_en;
	u32						debug_en;
	enum memx_fs_hif_type	type;
	union memx_fs_hif		hif;
};

struct memx_data {
	struct usb_interface *interface;
	struct usb_device    *udev;
	unsigned long         state;
	unsigned long         tx_size;
	unsigned long         rx_size;
	struct urb           *txurb;
	struct urb           *rxurb;
	struct urb           *fw_txurb;
	struct urb           *fw_rxurb;
	unsigned char        *tbuffer;
	unsigned char        *rbuffer;
	unsigned char        *fw_wbuffer;
	unsigned char        *fw_rbuffer;
	wait_queue_head_t     read_wq;
	uint32_t              flow_size[MEMX_TOTAL_FLOW_COUNT];
	uint32_t              buffer_size[MEMX_TOTAL_FLOW_COUNT];
	uint32_t              usb_first_chip_pipeline_flag;
	uint32_t              usb_last_chip_pingpong_flag;
	struct mutex          cfglock;
	struct mutex          readlock;
	uint8_t               flow_id;
	struct completion     fw_comp;
	struct completion     tx_comp;
	struct completion     rx_comp;
	struct completion     fwrx_comp;
	unsigned long         max_mpuin_size;
	unsigned long         max_mpuout_size;
	unsigned long         product_id;

	char devname[16];
	struct memx_file_sys  fs;
	uint32_t              chipcnt;
	u32                   minor_index;
	u32                   ThermalThrottlingDisable;
	u32                   gpio_r;

	u32                   reference_count;

	struct cdev           feature_cdev;
};

extern struct file_operations memx_feature_fops;
extern u32 tx_time_us;
extern u32 rx_time_us;
extern u32 tx_size;
extern u32 rx_size;
extern struct memx_throughput_info udrv_throughput_info;

struct urb;
void memx_complete(struct urb *urb);
void memx_fwrxcomplete(struct urb *urb);

#endif
