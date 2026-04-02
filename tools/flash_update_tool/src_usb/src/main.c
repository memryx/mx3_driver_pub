/* Copyright (c) 2023-2026 MemryX Inc.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <libusb.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Windows (x86/x64) is little-endian
#define HTOLE32(x) ((uint32_t)(x))
static inline void msleep(unsigned long ms) { Sleep((DWORD)ms); }
#else
#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define HTOLE32(x) ((uint32_t)(x))
#else
#if defined(__has_builtin)
#if __has_builtin(__builtin_bswap32)
#define HTOLE32(x) __builtin_bswap32((uint32_t)(x))
#else
#include <byteswap.h>
#define HTOLE32(x) bswap_32((uint32_t)(x))
#endif
#else
#include <byteswap.h>
#define HTOLE32(x) bswap_32((uint32_t)(x))
#endif
#endif
static inline void msleep(unsigned long ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {}
}
#endif

// ---- Protocol / HW constants -------------------------------------------------
#define VENDOR_ID_LEGACY  0x0559
#define VENDOR_ID         0x38E0
#define MX3PLUS_ZSBL_PID  0x40FF
#define MX3PLUS_FW_PID    0x4006
#define IFACE_INDEX       0

#define MEMX_OUT_EP       0x01
#define MEMX_FW_OUT_EP    0x02
#define MEMX_FW_IN_EP     0x82

#define FWCFG_ID_FW       0x00952701u
#define FWCFG_ID_CLR      0x00952700u

#define MAX_WRITE_SIZE    (64u*1024u)
#define MAX_FW_IMAGE_SIZE (128u*1024u)
#define TRANSFER_TIMEOUT  30000u

// ---- Status / Result ---------------------------------------------------------
typedef enum {
    MX_OK = 0,
    MX_ERR_ARG = -1,
    MX_ERR_IO = -2,
    MX_ERR_MEM = -3,
    MX_ERR_USB = -4,
    MX_ERR_BOUNDS = -5,
} mx_status_t;

#define MX_RETURN_IF(cond, code) do { if (cond) return (code); } while (0)

typedef struct {
    mx_status_t status;
    size_t      size;
    uint8_t* data;
} mx_buf_result_t;

// ---- Device handle -----------------------------------------------------------
typedef struct {
    libusb_context* ctx;
    libusb_device_handle* handle;
    uint8_t* fw_image;   // file buffer
    size_t                fw_size;
    uint8_t* txbuf;      // scratch buffer
} memx_usb_dev_t;

// ---- USB helpers -------------------------------------------------------------
static mx_status_t usb_bulk_send(libusb_device_handle* h, uint8_t ep,
    const void* buf, int len, unsigned timeout_ms, int max_retries) {
    if (!h || !buf || len < 0) return MX_ERR_ARG;
    int transferred = 0, r = 0;
    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        r = libusb_bulk_transfer(h, ep, (unsigned char*)buf, len, &transferred, (int)timeout_ms);
        if (r == 0 && transferred == len) return MX_OK;
        if (r != LIBUSB_ERROR_TIMEOUT && r != LIBUSB_ERROR_PIPE && r != LIBUSB_ERROR_BUSY) break;
    }
    fprintf(stderr, "bulk_send fail ep=0x%02x libusb=%s\n", ep, libusb_error_name(r));
    return MX_ERR_USB;
}

static mx_status_t usb_bulk_recv(libusb_device_handle* h, uint8_t ep,
    void* buf, int len, unsigned timeout_ms, int* out_xfer) {
    if (!h || !buf || len <= 0) return MX_ERR_ARG;
    int transferred = 0;
    int r = libusb_bulk_transfer(h, ep, (unsigned char*)buf, len, &transferred, (int)timeout_ms);
    if (out_xfer) *out_xfer = transferred;
    if (r != 0) {
        fprintf(stderr, "bulk_recv fail ep=0x%02x libusb=%s\n", ep, libusb_error_name(r));
        return MX_ERR_USB;
    }
    return MX_OK;
}

// ---- File loader ----------------------------------------------
static mx_buf_result_t load_file_to_buf(const char* path, size_t max_bytes) {
    mx_buf_result_t out = { .status = MX_ERR_IO, .size = 0, .data = NULL };
    if (!path || !*path) { out.status = MX_ERR_ARG; return out; }

    FILE* f = fopen(path, "rb");
    if (!f) { perror("fopen"); return out; }

    if (fseek(f, 0, SEEK_END) != 0) { perror("fseek"); goto done; }
    long szl = ftell(f);
    if (szl < 0) { perror("ftell"); goto done; }
    if (fseek(f, 0, SEEK_SET) != 0) { perror("fseek"); goto done; }

    size_t sz = (size_t)szl;
    if (sz == 0 || sz > max_bytes) { fprintf(stderr, "file size invalid: %zu\n", sz); out.status = MX_ERR_BOUNDS; goto done; }

    uint8_t* buf = (uint8_t*)malloc(sz);
    if (!buf) { out.status = MX_ERR_MEM; goto done; }

    size_t total = fread(buf, 1, sz, f);
    if (total != sz) { perror("fread"); free(buf); goto done; }

    out.status = MX_OK;
    out.size = sz;
    out.data = buf;

done:
    fclose(f);
    return out;
}

// ---- Device lifecycle --------------------------------------------------------
static mx_status_t memx_dev_init(memx_usb_dev_t* d) {
    if (!d) return MX_ERR_ARG;
    memset(d, 0, sizeof(*d));

    int r = libusb_init(&d->ctx);
    if (r != 0) { fprintf(stderr, "libusb_init: %s\n", libusb_error_name(r)); return MX_ERR_USB; }

    libusb_set_option(d->ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);

    d->txbuf = (uint8_t*)malloc(MAX_FW_IMAGE_SIZE);
    if (!d->txbuf) return MX_ERR_MEM;
    return MX_OK;
}

static void memx_dev_close(memx_usb_dev_t* d) {
    if (!d) return;
    if (d->handle) {
        libusb_release_interface(d->handle, IFACE_INDEX);
        libusb_close(d->handle);
        d->handle = NULL;
    }
}

static void memx_dev_deinit(memx_usb_dev_t* d) {
    if (!d) return;
    memx_dev_close(d);
    if (d->fw_image) { free(d->fw_image); d->fw_image = NULL; d->fw_size = 0; }
    if (d->txbuf) { free(d->txbuf); d->txbuf = NULL; }
    if (d->ctx) { libusb_exit(d->ctx); d->ctx = NULL; }
}

static mx_status_t memx_open_claim(memx_usb_dev_t* d, uint16_t pid) {
    if (!d || !d->ctx) return MX_ERR_ARG;
    memx_dev_close(d);

    uint16_t vendor_id = (pid == MX3PLUS_ZSBL_PID) ? VENDOR_ID_LEGACY : VENDOR_ID;

    d->handle = libusb_open_device_with_vid_pid(d->ctx, vendor_id, pid);

    // Retry another vendor id.
    if (!d->handle) {
        vendor_id = (vendor_id == VENDOR_ID) ? VENDOR_ID_LEGACY : VENDOR_ID;
        d->handle = libusb_open_device_with_vid_pid(d->ctx, vendor_id, pid);
        if (!d->handle) {
            fprintf(stderr, "device not found (PID=0x%04X)\n", pid); return MX_ERR_USB;
        }
    }

#if !defined(_WIN32)
    int kd = libusb_kernel_driver_active(d->handle, IFACE_INDEX);
    if (kd == 1) libusb_detach_kernel_driver(d->handle, IFACE_INDEX);
#endif

    int r = libusb_claim_interface(d->handle, IFACE_INDEX);
    if (r != 0) { fprintf(stderr, "claim iface: %s\n", libusb_error_name(r)); memx_dev_close(d); return MX_ERR_USB; }

    return MX_OK;
}

// ---- Protocol ops ------------------------------------------------------------
static mx_status_t memx_download_firmware(memx_usb_dev_t* d) {
    MX_RETURN_IF(!d || !d->fw_image || d->fw_size < 4, MX_ERR_ARG);

    uint32_t fw_total = (uint32_t)(d->fw_size);
    if (fw_total < 4) return MX_ERR_BOUNDS;

    uint32_t payload_bytes = fw_total - 4;  // exclude leading 4bytes firmware size
    uint8_t  img_fmt = 0;
    if (payload_bytes >= 0x7004 && d->fw_size >= 0x6F08 + 4) {
        img_fmt = *(uint32_t*)(d->fw_image + 0x6F08) & 0xFF;
    }
    if (img_fmt == 1) {
        if (payload_bytes > UINT32_MAX - 8) return MX_ERR_BOUNDS;
        payload_bytes += 8;
    }
    MX_RETURN_IF(payload_bytes > MAX_FW_IMAGE_SIZE, MX_ERR_BOUNDS);

    mx_status_t st = memx_open_claim(d, MX3PLUS_ZSBL_PID);
    if (st) return st;

    uint32_t le_len = HTOLE32(payload_bytes);
    memcpy(d->txbuf, &le_len, 4);
    st = usb_bulk_send(d->handle, MEMX_OUT_EP, d->txbuf, 4, TRANSFER_TIMEOUT, /*retries*/1);
    if (st) return st;

    memcpy(d->txbuf, d->fw_image + 4, payload_bytes);

    if (img_fmt == 1) {
        uint32_t real_size = *(uint32_t*)(d->fw_image);
        memcpy(d->txbuf + payload_bytes - 8, &real_size, 4);
        memcpy(d->txbuf + payload_bytes - 4, &img_fmt, 4);
    }

    st = usb_bulk_send(d->handle, MEMX_OUT_EP, d->txbuf, (int)payload_bytes, TRANSFER_TIMEOUT, 1);
    if (st) return st;

    printf("Firmware download done (size %u)\n", payload_bytes);
    memx_dev_close(d);
    return MX_OK;
}

static mx_status_t memx_download_flash(memx_usb_dev_t* d) {
    MX_RETURN_IF(!d || !d->fw_image || d->fw_size == 0, MX_ERR_ARG);

    mx_status_t st = memx_open_claim(d, MX3PLUS_FW_PID);
    if (st) return st;

    uint32_t cfg[2];
    cfg[0] = HTOLE32(FWCFG_ID_FW);
    cfg[1] = HTOLE32((uint32_t)d->fw_size);
    memcpy(d->txbuf, cfg, sizeof(cfg));

    st = usb_bulk_send(d->handle, MEMX_FW_OUT_EP, d->txbuf, (int)sizeof(cfg), TRANSFER_TIMEOUT, 1);
    if (st) return st;

    size_t remaining = d->fw_size;
    const uint8_t* p = d->fw_image;
    while (remaining) {
        int chunk = (int)((remaining > MAX_WRITE_SIZE) ? MAX_WRITE_SIZE : remaining);
        memcpy(d->txbuf, p, (size_t)chunk);
        st = usb_bulk_send(d->handle, MEMX_FW_OUT_EP, d->txbuf, chunk, TRANSFER_TIMEOUT, 1);
        if (st) return st;
        p += chunk; remaining -= (size_t)chunk;
    }

    int rx = 0;
    st = usb_bulk_recv(d->handle, MEMX_FW_IN_EP, d->txbuf, 4, TRANSFER_TIMEOUT, &rx);
    if (st) return st;
    if (rx != 4 || *(uint32_t*)d->txbuf != 0) {
        fprintf(stderr, "Flash download error (rx=%d, code=0x%08x)\n", rx, *(uint32_t*)d->txbuf);
        return MX_ERR_IO;
    }
    printf("Flash download done (size %u)\n", (unsigned)(d->fw_size + 4));

    cfg[0] = HTOLE32(FWCFG_ID_CLR);
    cfg[1] = HTOLE32(0);
    memcpy(d->txbuf, cfg, sizeof(cfg));
    st = usb_bulk_send(d->handle, MEMX_FW_OUT_EP, d->txbuf, (int)sizeof(cfg), TRANSFER_TIMEOUT, 1);
    if (st) return st;

    // ZLP
    st = usb_bulk_send(d->handle, MEMX_FW_OUT_EP, d->txbuf, 0, TRANSFER_TIMEOUT, 0);
    if (st) return st;

    memx_dev_close(d);
    return MX_OK;
}

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s [-all | -flash] <firmware.bin>\n"
        "\n"
        "Modes:\n"
#if defined(_WIN32)
        "  -flash    : flash only (default on Windows)\n"
#else
        "  -all      : download firmware, wait, then flash (default on Linux)\n"
        "  -flash    : flash only\n"
#endif
        , prog);
}

int main(int argc, char** argv) {
    if (argc < 2) { print_usage(argv[0]); return EXIT_FAILURE; }

    int argi = 1;
#if defined(_WIN32)
    int do_all = 0; // default on Windows: flash only
#else
    int do_all = 1; // default on Linux: -all
#endif
    if (argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-all") == 0) {
            do_all = 1;
            argi++;
        }
        else if (strcmp(argv[argi], "-flash") == 0) {
            do_all = 0;
            argi++;
        }
        else if (!strcmp(argv[argi], "-h") || !strcmp(argv[argi], "--help")) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else {
            fprintf(stderr, "Unknown option: %s\n\n", argv[argi]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

#if defined(_WIN32)
    if (do_all == 1) {
        fprintf(stderr, "-all option not support for windows \n\n");
        return EXIT_FAILURE;
    }
#endif

    if (argc <= argi) {
        fprintf(stderr, "Missing firmware file path.\n\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char* path = argv[argi];

    memx_usb_dev_t dev;
    mx_status_t st = memx_dev_init(&dev);
    if (st) {
        fprintf(stderr, "init fail (%d)\n", st);
        return EXIT_FAILURE;
    }

    mx_buf_result_t f = load_file_to_buf(path, MAX_FW_IMAGE_SIZE);
    if (f.status) {
        fprintf(stderr, "load file fail (%d)\n", f.status);
        memx_dev_deinit(&dev);
        return EXIT_FAILURE;
    }

    dev.fw_image = f.data;
    dev.fw_size = f.size;

    if (do_all) {
        st = memx_download_firmware(&dev);
        if (st) {
            fprintf(stderr, "download firmware fail (%d)\n", st);
            memx_dev_deinit(&dev);
            return EXIT_FAILURE;
        }
        msleep(3000);
    }

    // Flash step (always runs; uses the loaded file)
    st = memx_download_flash(&dev);
    if (st) {
        fprintf(stderr, "download flash fail (%d)\n", st);
        memx_dev_deinit(&dev);
        return EXIT_FAILURE;
    }

    memx_dev_deinit(&dev);
    return EXIT_SUCCESS;
}
