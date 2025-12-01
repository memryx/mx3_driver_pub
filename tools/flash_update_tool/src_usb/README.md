# memx_usb_update_flash_tool

USB update flash tool for MX3 USB module.

---

## Modes

- **`-all`**   : (USB boot) Download firmware, wait, then flash. *(default on Linux)*
- **`-flash`** : (QSPI boot) Flash only. *(default on Windows)*

---

## Build & Install

### Linux (Debian/Ubuntu)

#### Install dependencies
```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config libusb-1.0-0-dev
```

#### Build
```bash
# from the project root
cmake -S . -B build
cmake --build build -j
```

The binary will be in `build/` (e.g. `build/memx_usb_update_flash_tool`).

#### udev rules (permissions)

Without udev permissions, non-root users often get `LIBUSB_ERROR_ACCESS`.

1. Create a **udev rule** for MX3 Plus:
   ```bash
   sudo tee /etc/udev/rules.d/50-memx_usb_update_flash_tool.rules >/dev/null <<'RULE'
   SUBSYSTEM=="usb", ATTR{idVendor}=="0559", MODE="0666", TAG+="uaccess"
   SUBSYSTEM=="usb", ATTR{idVendor}=="38e0", MODE="0666", TAG+="uaccess"
   RULE
   ```

   > Notes:
   > - `TAG+="uaccess"` (systemd) grants access to the active desktop user.
   > - On older distros, use the fallback line and ensure your user is in the `plugdev` group:
   >   ```bash
   >   sudo groupadd -f plugdev
   >   sudo usermod -aG plugdev "$USER"
   >   ```

2. Reload rules and replug the device:
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   # then unplug/replug the USB device
   ```

#### Run
From the build output directory:
```bash
./memx_usb_update_flash_tool [-flash | -all] cascade.bin
```
---

### Windows

#### Install dependencies
1. Download **libusb v1.0.29 (Windows release)** from:
   <https://github.com/libusb/libusb/releases/tag/v1.0.29>
   Unzip it to a directory, e.g. `C:\libusb-1.0.29`.

2. Install **WinUSB driver** for your MX3 device:
   - Download [Zadig](https://zadig.akeo.ie/).
   - Select your MX3 USB device (VID `0559` / PID `4006`) or (VID `38E0` / PID `4006`).
   - Install the **WinUSB** driver.

#### Build
```powershell
# from the project root
cmake -S . -B build -DLIBUSB_ROOT_DIR=C:/libusb-1.0.29
cmake --build build --config Release -j
```

- The `.exe` will be in `build/Release/` (MSVC multi-config).
- CMake is set up to automatically copy `libusb-1.0.dll` next to the executable after build.

#### Run
From the build output directory:
```powershell
memx_usb_update_flash_tool.exe -flash cascade.bin
```

> **Note:** On Windows the default mode is `-flash`.
> The `-all` (firmware stage) mode is only supported on Linux.
