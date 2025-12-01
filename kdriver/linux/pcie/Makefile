CONFIG_MODULE_SIG=n
INCLUDES += -I$(PWD)/../../include
obj-m := memx_cascade_plus_pcie.o
memx_cascade_plus_pcie-objs := memx_feature.o memx_xflow.o memx_msix_irq.o memx_cascade_pciemain.o memx_fw_cmd.o memx_fw_init.o memx_pcie_dev_list_ctrl.o memx_fs_proc.o memx_fs_sys.o memx_fs.o memx_fw_log.o memx_fs_hwmon.o
all: driver app

driver:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) EXTRA_CFLAGS="$(INCLUDES)" modules

app:
	gcc memx_pcie_test.c -o app -I$(PWD)/../../include

debug:  
	make -C /lib/modules/$(shell uname -r)/build EXTRA_CFLAGS="$(INCLUDES) -DDEBUG" M=$(PWD) modules

android:
	make -C /lib/modules/$(shell uname -r)/build EXTRA_CFLAGS="$(INCLUDES) -DANDROID" M=$(PWD) modules

install:
	sudo insmod memx_cascade_plus_pcie.ko
#	sudo insmod memx_cascade_plus_pcie.ko  g_drv_fs_type=0/1/2 fs_debug_en=1

uninstall:
	sudo rmmod memx_cascade_plus_pcie

test:
	./app

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sudo rm -rf *.out app
