# Use following command to build the application: (inside pcie_boot/)
``` bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

# Use following command to run the application:
`.\Release\pcieupdateflash_win.exe -f <path>/cascade.bin`

# Return Codes

| Code | Description                    |
| ---- | ------------------------------ |
| `0`  | Firmware update successful     |
| `5`  | Timeout during update process  |
| `6`  | No MemryX device found         |
| `14`  | Firmware is already up to date |
| `> 0` | Update failed with error       |