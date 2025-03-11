<picture>
  <source srcset="figures/mx3_driver.png" media="(prefers-color-scheme: dark)">
  <source srcset="figures/mx3_driver_light.png" media="(prefers-color-scheme: light)">
  <img src="figures/mx3_driver_light.png" alt="MemryX MX3 Linux Driver">
</picture>


<!-- Badges for quick project insights -->
[![MemryX SDK](https://img.shields.io/badge/MemryX%20SDK-1.2-brightgreen)](https://developer.memryx.com)

# MemryX MX3 Linux Driver

This repo serves as a mirror of the Linux kernel driver for the MemryX MX3, along with the Python wrapper around the driver's [C API (libmemx.so)](https://developer.memryx.com/api/driver/driver.html), and binary copies of chip firmware and related tools.

> **IMPORTANT**: This repo is for open-source reference and advanced users **only**. If you are looking to simply use MX3 hardware, **please** install the [MemryX SDK](https://developer.memryx.com) instead!


### Repository Overview

The repository is structured as follows.

| **Folder**         | **Description**                                                                                          |
|--------------------|----------------------------------------------------------------------------------------------------------|
| `kdriver`          | Source for the Linux kernel driver for MX3. [GPL](/kdriver/LICENSE.md)                                 |
| `pymodule`         | Source for the Python wrapper around the [driver API](https://developer.memryx.com/api/driver/driver.html). [MIT](/pymodule/LICENSE.md)   |
| `tools`            | Source for MX3 firmware checker and updater tools. [GPL](/tools/flash_update_tool/LICENSE.md) |
| `firmware`         | MX3 firmware binary blobs. [See license here](https://developer.memryx.com/license.html#mx3-firmware-and-windows-driver). |


## Building

Each component has similar build instructions using `make`.

### Linux Kernel Driver

First, make sure you have the necessary Linux kernel headers and tools to [build external modules](https://docs.kernel.org/kbuild/modules.html).

Then just:

```bash
cd kdriver/linux/pcie/
make
```

### Firmware Tools

These tools shouldn't require external dependencies other than a typical gcc toolchain. For each, simply `cd` to their folder and run:

```bash
make
```

### Python Module

This module is normally bundled with the MemryX SDK [pip package](https://developer.memryx.com/get_started/install_tools.html), but can also be built from source here. You have to first install the `libmemx.so` library, which can be manually extracted from the [pre-built driver](https://developer.memryx.com/get_started/install_driver.html), either from the `.deb` or the `install.sh`.

Once `libmemx.so` and its header file, `memx.h` are available on your path, run:

```bash
cd pymodule
make all
```


## Licenses


* `kdriver` and `flash update tool`: [GPLv2](/kdriver/LICENSE.md)
* `pymodule`: [MIT](/pymodule/LICENSE.md)
* `firmware`: Proprietary. See details [here](https://developer.memryx.com/license.html#mx3-firmware-and-windows-driver). *TL;DR*: you're free to use and redistribute exact copies how ever you want, but do not modify, decompile, nor create derivatives.


## See Also
Enhance your experience with MemryX solutions by exploring the following resources:

- **[Developer Hub](https://developer.memryx.com/index.html):** Access comprehensive documentation for MemryX hardware and software.
- **[MemryX SDK Installation Guide](https://developer.memryx.com/get_started/install.html):** Learn how to set up essential tools and drivers to start using MemryX accelerators.
- **[Tutorials](https://developer.memryx.com/tutorials/tutorials.html):** Follow detailed, step-by-step instructions for various use cases and applications.
- **[Model Explorer](https://developer.memryx.com/model_explorer/models.html):** Discover and explore models that have been compiled and optimized for MemryX accelerators.
- **[Examples](https://github.com/memryx/MemryX_eXamples):** Explore a collection of end-to-end AI applications powered by MemryX hardware and software.
