#include <windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <cstdint>
#include <future>
#include "memx/memx.h"

#define VERSION "V1.0"
#define OFFSET_CRC32  0x6F08
#define OFFSET_COMMIT 0x6F0C
#define OFFSET_DATE 0x6F10
#define FW_BUFFER_SIZE (256 * 1024)  // 256KB

bool read_firmware_metadata(const std::string& filepath, uint32_t& out_commit, uint32_t& out_date)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open firmware file: " << filepath << std::endl;
        return false;
    }

    // Read commit ID at OFFSET_COMMIT 0x6F0C
    file.seekg(OFFSET_COMMIT, std::ios::beg);
    file.read(reinterpret_cast<char*>(&out_commit), sizeof(uint32_t));
    if (file.gcount() != sizeof(uint32_t)) {
        std::cerr << "Failed to read commit ID" << std::endl;
        return false;
    }

    // Read date code at OFFSET_DATE 0x6F10
	file.seekg(OFFSET_DATE, std::ios::beg);
    file.read(reinterpret_cast<char*>(&out_date), sizeof(uint32_t));
    if (file.gcount() != sizeof(uint32_t)) {
        std::cerr << "Failed to read date code" << std::endl;
        return false;
    }

    return true;
}

// CRC32 lookup table
static uint32_t crc32_table[256] = {0};

void init_crc32_table()
{
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (polynomial ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
}

uint32_t calc_crc32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF)
{
    for (size_t i = 0; i < length; ++i)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

int calculate_crc32(const std::string& filename)
{
    init_crc32_table();

    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }

    uint8_t buffer[FW_BUFFER_SIZE];
    uint32_t* pBuf32 = reinterpret_cast<uint32_t*>(buffer);
    int* pcrc = nullptr;
    uint32_t crc = 0xFFFFFFFF;

    while (file) {
        file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        std::streamsize bytesRead = file.gcount();

        if (bytesRead <= 32)
            return 0;

        if (pBuf32[OFFSET_CRC32 / 4] == 0) {
            crc = calc_crc32(buffer + 4, bytesRead - 12, 0xFFFFFFFF);
            pcrc = reinterpret_cast<int*>(&buffer[bytesRead - 8]);
        } else if (pBuf32[OFFSET_CRC32 / 4] == 1) {
            crc = calc_crc32(buffer, bytesRead - 4, 0xFFFFFFFF);
            pcrc = reinterpret_cast<int*>(&buffer[bytesRead - 4]);
            if (*pcrc != crc)
                return 0;

            crc = calc_crc32(buffer + 4, pBuf32[0] - 8, 0xFFFFFFFF);
            pcrc = reinterpret_cast<int*>(&buffer[pBuf32[0] - 4]);
        }

        if (*pcrc != crc)
            return 0;
    }

    return crc;
}

memx_status memx_download_with_timeout(uint8_t group_id, const std::string& path, uint8_t type, int timeout_seconds = 10) {
    std::future<memx_status> result = std::async(std::launch::async, [=]() {
        return memx_download_firmware(group_id, path.c_str(), type);
    });

    if (result.wait_for(std::chrono::seconds(timeout_seconds)) == std::future_status::ready) {
        return result.get();
    } else {
        std::cerr << "Firmware download timed out after " << timeout_seconds << " seconds!" << std::endl;
        return static_cast<memx_status>(5);
    }
}

int main(int argc, char** argv)
{
    memx_status status = MEMX_STATUS_OK;
    std::string fw_path;

    std::cout << std::endl << "########## Memryx Flash UpdateTool - Windows (" << VERSION << ") ###############" << std::endl;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-f" && i + 1 < argc) {
            fw_path = argv[++i];
        }
    }

    if (fw_path.empty()) {
        std::cerr << "Please provide the -f flag followed by a filename." << std::endl;
        return 1;
    }

    // Read firmware metadata
    uint32_t fw_commit = 0, fw_datecode = 0;
    if (!read_firmware_metadata(fw_path, fw_commit, fw_datecode)) {
        return 1;
    }

    std::cout << "Reading firmware metadata..." << std::endl;
    std::cout << "  Firmware Commit: 0x" << std::setw(8) << std::setfill('0') << std::hex << fw_commit
              << ", Date Code: 0x" << fw_datecode << std::dec << std::endl;

    if (!calculate_crc32(fw_path)) {
        std::cerr << "  ==> CRC32 validation failed!" << std::endl;
        return 1;
    }

    // Read device info
    uint32_t device_count = 0;
    uint8_t group_id = 0;
    uint8_t chip_id = 0;
    uint64_t current_value = 0;
    status = memx_operation_get_device_count(&device_count);
    if (memx_status_error(status)) { std::cerr << "Failed to get device count, status = " << status << std::endl; return status; }

    if (device_count == 0) {
        std::cout << std::endl << "No MemryX device found, exiting..." << std::endl;
        return 6;
    }

    for (group_id = 0; group_id < device_count; ++group_id) {
        std::cout << std::endl << "Reading device " << static_cast<int>(group_id) << " info..." << std::endl;

        status = memx_get_feature(group_id, chip_id, OPCODE_GET_FW_COMMIT, &current_value);
        if (memx_status_error(status)) { std::cerr << "Failed to get firmware commit, status = " << status << std::endl; return status; }
        uint32_t dev_commit = static_cast<uint32_t>(current_value);

        status = memx_get_feature(group_id, chip_id, OPCODE_GET_DATE_CODE, &current_value);
        if (memx_status_error(status)) { std::cerr << "Failed to get firmware date code, status = " << status << std::endl; return status; }
        uint32_t dev_datecode = static_cast<uint32_t>(current_value);

        std::cout << "  Firmware Commit: 0x" << std::setw(8) << std::setfill('0') << std::hex << dev_commit
                << ", Date Code: 0x" << dev_datecode << std::dec << std::endl;

        char kdriver_version[8] = {0};
        status = memx_get_feature(group_id, chip_id, OPCODE_GET_KDRIVER_VERSION, (uint64_t*)&kdriver_version);
        if (memx_status_error(status)) { std::cerr << "Failed to get KDriver version, status = " << status << std::endl; return status; }
        std::cout << "  KDriver Version: " << kdriver_version << std::endl;

        if (dev_commit == fw_commit) {
            std::cout << std::endl << "Firmware already up to date, exiting..." << std::endl;
            status = static_cast<memx_status>(14);
            continue;
        }

        status = memx_get_feature(group_id, chip_id, OPCODE_GET_QSPI_RESET_RELEASE, &current_value);
        if (memx_status_error(status)) { std::cerr << "Failed to get QSPI reset release, status = " << status << std::endl; return status; }
        uint32_t qspi_reset = static_cast<uint32_t>((current_value >> 28) & 0x7);

        if (qspi_reset == 4) {
            uint32_t set_value = 1;
            status = memx_set_feature(group_id, chip_id, OPCODE_SET_QSPI_RESET_RELEASE, set_value);
            if (memx_status_error(status)) { std::cerr << "Failed to set QSPI reset release, status = " << status << std::endl; return status; }

            status = memx_get_feature(group_id, chip_id, OPCODE_GET_QSPI_RESET_RELEASE, &current_value);
            if (memx_status_error(status)) { std::cerr << "Failed to get QSPI reset release, status = " << status << std::endl; return status; }
            uint32_t qspi_reset_after = static_cast<uint32_t>((current_value >> 28) & 0x7);
            if (qspi_reset_after != 7) {
                std::cerr << "QSPI reset state unexpected after set! Expected 7, got " << qspi_reset_after << std::endl;
                return 4;
            }
            Sleep(100);
        }

        std::cout << std::endl << "Start download firmware..." << std::endl;
        status = memx_download_with_timeout(group_id, fw_path.c_str(), 0, 10);
        if (status == 0) {
            std::cout << "  Firmware download successful!" << std::endl;
        } else if (status == 5) {
            std::cerr << "  Firmware download timed out!" << std::endl;
            return status;
        } else {
            std::cerr << "  Firmware download failed, status = " << status << std::endl;
            return status;
        }

    }

    return status;
}
