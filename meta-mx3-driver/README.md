# meta-mx3-driver

This folder contains a bitbake meta for building the MX3 kernel driver + firmware into a Yocto image.


## Usage

1. Add the path to this folder (`meta-mx3-driver`) to your build's `conf/bblayers.conf`
2. Include the `memx-cascade-plus-pcie` target in your Yocto build process
