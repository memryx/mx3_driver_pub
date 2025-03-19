CONFIG_MODULE_SIG=n
INCLUDES += -I$(PWD)/../../include

KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build

obj-m := memx_cascade_plus_pcie.o
memx_cascade_plus_pcie-objs := memx_feature.o memx_xflow.o memx_msix_irq.o memx_cascade_pciemain.o memx_fw_cmd.o memx_fw_init.o memx_pcie_dev_list_ctrl.o memx_fs_proc.o memx_fs_sys.o memx_fs.o memx_fw_log.o memx_fs_hwmon.o
all: driver

driver:
	make -C $(KERNEL_SRC) M=$(PWD) EXTRA_CFLAGS="$(INCLUDES)" modules

modules_install:
	make -C $(KERNEL_SRC) M=$(PWD) EXTRA_CFLAGS="$(INCLUDES)" modules_install

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
