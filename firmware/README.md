# MX3 Firmware Options


## Main Options

* `cascade_4chips_flash.bin`: Firmware image for QSPI flash boot boards with `<= 4` chips. Uses 2x 16MB + 1x 1MB BARs. **Default**
* `cascade_mini_bar_flash.bin`: Firmware image for QSPI flash boot boards with `<= 4` chips. Uses 1x 512KB + 1x 256KB + 1x 4KB + 1x 1MB BARs. Helpful for systems with limited BAR space.


## Less Common Options

* `cascade.bin`: Firmware for no-flash boards with `<= 16` chips, that loads from host memory. Uses 1x 256MB + 1x 1MB BARs. "cascade.bin" is also a hardcoded name used by some manufacturing test tools.
* `cascade_16chips_flash.bin`: Firmware image for QSPI flash boot boards with `<= 16` chips. Uses 1x 256MB BAR + 1x 1MB BARs.
