# Device Tree Overlays

This folder contains a collection of dtbo files that a user can use to modify the running device tree without needing to recompile the kernel.

To use these, the Linux distro needs to either:

1. Support loading extra dtbo files in the bootloader (such as in extlinux). [Here](https://wiki.radxa.com/Rock3/extlinux) are [some](https://developer.ridgerun.com/wiki/index.php/NVIDIA_Jetson_-_Device_Tree_Overlay) relevant [pages](https://docs.armbian.com/User-Guide_Allwinner_overlays/)
2. Load U-Boot and manually add the dtbo before loading the Linux kernel, like [here](https://docs.u-boot.org/en/latest/usage/fdt_overlays.html)
3. Have a Linux kernel that supports overlays by just [adding them to configfs](https://github.com/ikwzm/dtbocfg)
