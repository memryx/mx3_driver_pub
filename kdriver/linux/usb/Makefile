CONFIG_MODULE_SIG=n
obj-m = memx_cascade_usb.o
KVERSION = $(shell uname -r)
EXTRA_CFLAGS += -I$(PWD)/../../include

memx_cascade_usb-objs := memx_feature.o memx_cascade_usbmain.o memx_cascade_debugfs.o memx_fs.o memx_fs_proc.o memx_fw_log.o memx_fs_sys.o memx_fs_hwmon.o

all:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

debug:
	make -C /lib/modules/$(KVERSION)/build EXTRA_CFLAGS="$(INCLUDES) -DDEBUG" M=$(PWD) modules

android:
	make -C /lib/modules/$(KVERSION)/build EXTRA_CFLAGS="$(INCLUDES) -DANDROID" M=$(PWD) modules

clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
	sudo rmmod memx_cascade_usb

install:
	sudo insmod memx_cascade_usb.ko
	#sudo insmod memx_cascade_usb.ko g_drv_fs_type=1 fs_debug_en=1 pcie_aspm=1 pcie_lane_no=1 pcie_lane_speed=2
