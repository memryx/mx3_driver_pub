#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <limits.h>  // for INT_MAX, INT_MIN
#include <zlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define version "V1.3"

#define FSBL_SIZE_MAX   0x40000

// Make the SDK console work in the debugger
#define printf(...) \
 fprintf(stdout, __VA_ARGS__); \
 fflush(stdout);

int get_pcie_info(long long*, long*);

#define QSPI_BASE_ADDR  0x20010000
#define PERI_QSPI ((uint32_t*)QSPI_BASE_ADDR)
#define PERI_QSPI_TypeDef uint32_t
#define MAX_TRANSFER_LEN 32

#define QSPI_CLK_CFG    (QSPI_BASE_ADDR+0x00)
#define QSPI_TRIG       (QSPI_BASE_ADDR+0x04)
#define QSPI_MODE       (QSPI_BASE_ADDR+0x08)
#define QSPI_OPCFG      (QSPI_BASE_ADDR+0x0C)
#define QSPI_ADDR       (QSPI_BASE_ADDR+0x10)
#define QSPI_DCNT       (QSPI_BASE_ADDR+0x14)
#define QSPI_DATA       (QSPI_BASE_ADDR+0x18)
#define QSPI_BUSY       (QSPI_BASE_ADDR+0x1C)
#define QSPI_BASE       (QSPI_BASE_ADDR+0x20)

#define PERI_QSPI_QSPI_CLK_CFG_hclk_cnt_Pos          (0)
#define PERI_QSPI_QSPI_CLK_CFG_hclk_cnt_Msk          (0xfful << PERI_QSPI_QSPI_CLK_CFG_hclk_cnt_Pos)
#define PERI_QSPI_QSPI_CLK_CFG_ecsn_cnt_Pos          (8)
#define PERI_QSPI_QSPI_CLK_CFG_ecsn_cnt_Msk          (0xfful << PERI_QSPI_QSPI_CLK_CFG_ecsn_cnt_Pos)
#define PERI_QSPI_QSPI_OPCFG_cmd_Pos                 (0)
#define PERI_QSPI_QSPI_OPCFG_cmd_Msk                 (0xfful << PERI_QSPI_QSPI_OPCFG_cmd_Pos)
#define PERI_QSPI_QSPI_OPCFG_r0w1_Pos                (8)
#define PERI_QSPI_QSPI_OPCFG_r0w1_Msk                (0xfful << PERI_QSPI_QSPI_OPCFG_r0w1_Pos)
#define PERI_QSPI_QSPI_OPCFG_noaddr_Pos              (16)
#define PERI_QSPI_QSPI_OPCFG_noaddr_Msk              (0xful << PERI_QSPI_QSPI_OPCFG_noaddr_Pos)
#define PERI_QSPI_QSPI_OPCFG_qio_Pos                 (20)
#define PERI_QSPI_QSPI_OPCFG_qio_Msk                 (0xful << PERI_QSPI_QSPI_OPCFG_qio_Pos)
#define PERI_QSPI_QSPI_OPCFG_dummy_Pos               (24)
#define PERI_QSPI_QSPI_OPCFG_dummy_Msk               (0xfful << PERI_QSPI_QSPI_OPCFG_dummy_Pos)

#define CMD_RDSR       0x05  /*# SPI/QPI           */
#define CMD_WREN       0x06  /*# SPI/QPI           */
#define CMD_BE         0xD8  /*# SPI/QPI - 3/4     */
#define CMD_CE         0x60  /*# SPI/QPI - 0       */
#define CMD_SE         0x20  /*# SPI/QPI - 3/4     */
#define CMD_PP         0x02  /*# SPI/QPI - 3/4     */
#define CMD_4PP        0x32  /*# SPI     - 3/4     */
#define CMD_READ       0x03  /*# SPI     - 3/4     */
#define CMD_FAST_READ  0x0B  /*# SPI     - 3/4     */
#define CMD_2READ      0xBB  /*# SPI     - 3/4     */
#define CMD_DREAD      0x3B  /*# SPI     - 3/4     */
#define CMD_4READ      0xEB  /*# SPI/QPI - 3/4     */
#define CMD_QREAD      0x6B  /*# SPI     - 3/4     */


typedef uint64_t u64;
typedef uint32_t u32;

static u32 *xflow_cnfg_base, *xflow_vbuf_base;

int calculate_crc32(const char* filename) {
    int crc = 0;
    FILE *file=NULL;
    file=fopen(filename,"rb");
    if(file != NULL) {
        unsigned char buffer[FSBL_SIZE_MAX];
        int *pcrc;
        crc = crc32(0L, Z_NULL, 0);
        size_t bytesRead;
        uint32_t *pBuf32 = (uint32_t *)buffer;

        while ((bytesRead = fread(buffer, 1, FSBL_SIZE_MAX, file)) > 0) {
            if(bytesRead <= 32) {
                fclose(file);
                return 0;
            }

            if (pBuf32[0x6F08/sizeof(uint32_t)] == 0) {
                  crc = crc32(crc, buffer+4, bytesRead-12);
                  pcrc = (int *)&buffer[bytesRead-8];
            } else if (pBuf32[0x6F08/sizeof(uint32_t)] == 1) {
              crc = crc32(crc, buffer, bytesRead-4);
                  pcrc = (int *)&buffer[bytesRead-4];
                  if(*pcrc != crc) {
                      fclose(file);
                      return 0;
                  }
              crc = crc32(0L, Z_NULL, 0);
              crc = crc32(crc, buffer+4, pBuf32[0]-8);
                  pcrc = (int *)&buffer[pBuf32[0]-4];
            }

            //printf("SS=0x%lX CRC=0x%X\r\n",bytesRead,*pcrc);
            if(*pcrc != crc) {
                fclose(file);
                return 0;
            }
        }
    }
    fclose(file);
    return crc;
}

void memx_write32(u32 address, u32 value)
{
    xflow_cnfg_base[1] = 0x1;
    xflow_cnfg_base[0] = address;
    xflow_vbuf_base[0] = value;
}

u32 memx_read32(u32 address)
{
    xflow_cnfg_base[1] = 0x1;
    xflow_cnfg_base[0] = address;
    return xflow_vbuf_base[0];
}


static void qspi_burst_read(PERI_QSPI_TypeDef *oPERI_QSPI, uint8_t cmd, uint32_t mode, uint32_t addr, uint8_t *data, uint32_t dcnt, uint32_t noaddr, uint32_t qio, uint32_t dummy)
{
    // always wait until previous transmission is finished
    // since we have no software timer within module can use, we just block here
    // until hardware itself triggers timeout and breaks out hopefully
    while(memx_read32(QSPI_BUSY)) ;

    memx_write32(QSPI_MODE , mode);
    memx_write32(QSPI_OPCFG , ((cmd << PERI_QSPI_QSPI_OPCFG_cmd_Pos) & PERI_QSPI_QSPI_OPCFG_cmd_Msk) \
                            | ((0x0 << PERI_QSPI_QSPI_OPCFG_r0w1_Pos) & PERI_QSPI_QSPI_OPCFG_r0w1_Msk) \
                            | ((noaddr << PERI_QSPI_QSPI_OPCFG_noaddr_Pos) & PERI_QSPI_QSPI_OPCFG_noaddr_Msk) \
                            | ((qio << PERI_QSPI_QSPI_OPCFG_qio_Pos) & PERI_QSPI_QSPI_OPCFG_qio_Msk) \
                            | ((dummy << PERI_QSPI_QSPI_OPCFG_dummy_Pos) & PERI_QSPI_QSPI_OPCFG_dummy_Msk));

    memx_write32(QSPI_ADDR , addr);
    memx_write32(QSPI_DCNT , dcnt);
    memx_write32(QSPI_BASE , (u32)((u64)data)&0xFFFFFFFF);

    // trigger transmission start
    memx_write32(QSPI_TRIG , 1);

    // wait until this transmission is ready
    while(memx_read32(QSPI_BUSY)) ;
}
void qspi_cmd(PERI_QSPI_TypeDef *oPERI_QSPI, uint8_t cmd, uint32_t r0w1, uint32_t addr, uint8_t *data, uint32_t dcnt, uint32_t noaddr, uint32_t qio, uint32_t dummy)
{
    u32 wlen = (dcnt + 3) >> 2; // ceiling to 1 word = 4 bytes

    // always wait until previous transmission is finished
    // since we have no software timer within module can use, we just block here
    // until hardware itself triggers timeout and breaks out hopefully
    while(memx_read32(QSPI_BUSY)) ;

    memx_write32(QSPI_MODE , 3);

    memx_write32(QSPI_OPCFG , ((cmd << PERI_QSPI_QSPI_OPCFG_cmd_Pos) & PERI_QSPI_QSPI_OPCFG_cmd_Msk) \
                            | ((r0w1 << PERI_QSPI_QSPI_OPCFG_r0w1_Pos) & PERI_QSPI_QSPI_OPCFG_r0w1_Msk) \
                            | ((noaddr << PERI_QSPI_QSPI_OPCFG_noaddr_Pos) & PERI_QSPI_QSPI_OPCFG_noaddr_Msk) \
                            | ((qio << PERI_QSPI_QSPI_OPCFG_qio_Pos) & PERI_QSPI_QSPI_OPCFG_qio_Msk) \
                            | ((dummy << PERI_QSPI_QSPI_OPCFG_dummy_Pos) & PERI_QSPI_QSPI_OPCFG_dummy_Msk));

    memx_write32(QSPI_ADDR , addr);
    memx_write32(QSPI_DCNT , dcnt);


    // write data to fifo before transmission
    if((r0w1 & 0x1) == 1) {
        for(uint32_t i=0; i<wlen; i++)
            memx_write32(QSPI_DATA , ((uint32_t *)data)[i]);
    }

    // trigger transmission start
    memx_write32(QSPI_TRIG , 1);

    // read data from fifo after transmission
    if((r0w1 & 0x1) == 0) {
        // wait until this transmission is ready
        while(memx_read32(QSPI_BUSY)) ;
        for(uint32_t i=0; i<wlen; i++)
            ((uint32_t *)data)[i] = memx_read32(QSPI_DATA);
    }
}
static int qspi_wren(PERI_QSPI_TypeDef *oPERI_QSPI)
{
    uint32_t status = 0;
    clock_t start_t, end_t;
    u32 waittime;

    qspi_cmd(oPERI_QSPI, CMD_WREN, 0, 0, 0, 0, 1, 0, 0);

    start_t = clock();
    do {
        qspi_cmd(oPERI_QSPI, CMD_RDSR, 0, 0, (void*)&status, 2, 1, 0, 0);
        end_t = clock();
        waittime = (u32)((end_t - start_t) / CLOCKS_PER_SEC);
        if (waittime >= 2) {
            return -1;
        }
    } while ((status & (0x2)) == 0x0);

    return 0;
}
static void qspi_waitrdy(PERI_QSPI_TypeDef *oPERI_QSPI)
{
    uint32_t status = 0;

    do {
        qspi_cmd(oPERI_QSPI, CMD_RDSR, 0, 0, (void*)&status, 2, 1, 0, 0);
    } while (status & 0x1);
}
int qspi_erase(PERI_QSPI_TypeDef *oPERI_QSPI, uint32_t addr, uint32_t ecnt)
{
    uint8_t erase_cmd = 0;
    int sector_count, i, add_incr;

    /*if (ecnt == store_flash->flash_size) {
        erase_cmd = CMD_CE;
        qspi_wren(oPERI_QSPI);
        qspi_cmd(oPERI_QSPI, erase_cmd, 0, 0, 0, 0, 1, 0, 0);
        qspi_waitrdy(oPERI_QSPI);
        return;
    } else*/ if (ecnt % (64*1024)) {
        erase_cmd = CMD_SE;
        add_incr = 4*1024;
    } else {
        erase_cmd = CMD_BE;
        add_incr = 64*1024;
    }

    sector_count = ecnt / add_incr;
    if (sector_count == 0)
        sector_count = 1;

    for (i = 0; i < sector_count; i++) {
        if(qspi_wren(oPERI_QSPI)) {
            return -1;
        }
        qspi_cmd(oPERI_QSPI, erase_cmd, 0, addr, 0, 0, 0, 0, 0);
        qspi_waitrdy(oPERI_QSPI);
        addr += add_incr;
    }
    return 0;
}

void qspi_writeflash(PERI_QSPI_TypeDef *oPERI_QSPI, uint32_t addr, uint32_t* buf, uint32_t cnt, int op_bits)
{
    uint8_t write_cmd = 0, qio = 0, dummy = 0;
    int last_length = cnt;
    uint8_t *transfer_buf = (uint8_t *)buf;
    uint32_t transfer_addr = addr;

    if (op_bits == 1) {
        write_cmd = CMD_PP;
        qio = 0;
        dummy = 0;
    } else {
        write_cmd = CMD_4PP;
        qio = 4;
        dummy = 0;
    }

    while (last_length > 0) {
        qspi_wren(oPERI_QSPI);
        qspi_cmd(oPERI_QSPI, write_cmd, 1, transfer_addr, transfer_buf, MAX_TRANSFER_LEN, 0, qio, dummy);
        qspi_waitrdy(oPERI_QSPI);

        if (last_length > MAX_TRANSFER_LEN) {
            last_length -= MAX_TRANSFER_LEN;
            transfer_buf += MAX_TRANSFER_LEN;
            transfer_addr += MAX_TRANSFER_LEN;
        } else
            last_length = 0;
    }
}
void qspi_readflash(PERI_QSPI_TypeDef *oPERI_QSPI, uint32_t addr, uint8_t* buf, uint32_t cnt, int op_bits, int dma)
{
    uint8_t read_cmd = 0, qio = 0, dummy = 0;
    int last_length = cnt;
    uint8_t *transfer_buf = buf;
    uint32_t transfer_addr = addr;

    if (op_bits == 1) {
        read_cmd = CMD_READ;
        qio = 0;
        dummy = 0;
    } else {
        read_cmd = CMD_QREAD;
        qio = 4;
        dummy = 8;
    }

    if (dma)
        qspi_burst_read(oPERI_QSPI, read_cmd, 0x13, addr, buf, cnt, 0, qio, dummy);
    else {
        while (last_length > 0) {
            qspi_cmd(oPERI_QSPI, read_cmd, 0, transfer_addr, transfer_buf, MAX_TRANSFER_LEN, 0, qio, dummy);

            if (last_length > MAX_TRANSFER_LEN) {
                last_length -= MAX_TRANSFER_LEN;
                transfer_buf += MAX_TRANSFER_LEN;
                transfer_addr += MAX_TRANSFER_LEN;
            } else
                last_length = 0;
        }
    }
}

int update_qspi_firmware(uint32_t *buffer, uint32_t size)
{
    if(qspi_erase(PERI_QSPI, 0, (size+16383)& ~(16383))) {
        return -1;
    }
    qspi_writeflash(PERI_QSPI, 0, (uint32_t*) (buffer), size+4, 1);
}

int main(int argc, char *argv[])
{
    printf("\n########## Memryx Flash UpdateTool - Directed (%s) ###############\n", version);

    char *p;
    FILE *fsbl_bin=NULL;
    u32 *vptr, *vptr_bkup;
    int err=0, imgSize=0, crc32=0, fd=0;
    clock_t start_t, end_t;
    u32 waittime;
    u32 buffer[FSBL_SIZE_MAX/sizeof(u32)];
    size_t bytesRead;
    int i,j;
    int retry = 2;
    int devcnt, ret = 0;

    long long pbase[16][6] = {0};
    long psize[16][6] = {0};

    if (argc < 3 || strcmp(argv[1], "-f") != 0) {
        printf("Please provide the -f flag followed by a filename.\n");
        return -1;
    }

    /* Retrieve the filename from command-line argument */
    const char* filename = argv[2];

err_retry:

    /* Calculate the CRC32 on the FSBL image */
    crc32 = calculate_crc32(filename);
    if(!crc32) {
        printf("CRC checked FAILED\n");
        return -1;
    }

    /* Check if Cadence PCIe RP is up or not */
    devcnt = get_pcie_info(&pbase[0][0], &psize[0][0]);
    if (devcnt == 0) {
        printf("Memryx PCIe EP not found!\n");
        return -1;
    }

    ret = 0;
    for(j=0; j< devcnt; j++) {

        printf("(DEICE%d): Start Update Flash----\n",j+1);
        printf("BAR0: Base Address: 0x%016llX  Size=0x%08lX\n", pbase[j][0], psize[j][0]);
        printf("BAR1: Base Address: 0x%016llX  Size=0x%08lX\n", pbase[j][1], psize[j][1]);
        printf("BAR2: Base Address: 0x%016llX  Size=0x%08lX\n", pbase[j][2], psize[j][2]);
        printf("BAR3: Base Address: 0x%016llX  Size=0x%08lX\n", pbase[j][3], psize[j][3]);
        printf("BAR4: Base Address: 0x%016llX  Size=0x%08lX\n", pbase[j][4], psize[j][4]);
        printf("BAR5: Base Address: 0x%016llX  Size=0x%08lX\n", pbase[j][5], psize[j][5]);

        if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
            printf("Access /dev/mem failed\n");
            return -1;
        }

        if((psize[j][0] == 0x10000000) && (psize[j][1] == 0x100000)) {
            vptr = (u32 *)mmap(NULL, psize[j][0], PROT_READ|PROT_WRITE, MAP_SHARED, fd, pbase[j][0]);
            vptr_bkup = vptr;
            xflow_cnfg_base = (u32 *) &(vptr[0x0C000000/sizeof(u32)]);
            xflow_vbuf_base = (u32 *) &(vptr[0x08000000/sizeof(u32)]);
        } else if((psize[j][0] == 0x8000000) && (psize[j][2] == 0x100000)) {
            vptr = (u32 *)mmap(NULL, psize[j][0], PROT_READ|PROT_WRITE, MAP_SHARED, fd, pbase[j][0]);
            vptr_bkup = vptr;
            xflow_cnfg_base = (u32 *) &(vptr[0x04000000/sizeof(u32)]);
            xflow_vbuf_base = (u32 *) &(vptr[0x00000000/sizeof(u32)]);
        } else if(((psize[j][0] == 0x1000000) && (psize[j][2] == 0x1000000)&& (psize[j][4] == 0x100000)) ||
                  ((psize[j][0] == 0x4000000) && (psize[j][2] == 0x4000000)&& (psize[j][4] == 0x100000)) ) {
            vptr = (u32 *)mmap(NULL, psize[j][0], PROT_READ|PROT_WRITE, MAP_SHARED, fd, pbase[j][0]);
            xflow_vbuf_base = (u32 *) &(vptr[0]);
            vptr = (u32 *)mmap(NULL, psize[j][2], PROT_READ|PROT_WRITE, MAP_SHARED, fd, pbase[j][2]);
            xflow_cnfg_base = (u32 *) &(vptr[0]);
        } else {
            printf("None Supported BAR Mapping\n");
            close(fd);
            return -1;
        }

        fsbl_bin=fopen(filename,"rb");
        if(fsbl_bin != NULL) {
            fseek(fsbl_bin,0L,SEEK_END);
            imgSize = ftell(fsbl_bin);
            fseek(fsbl_bin,0L,SEEK_SET);
            printf("Image size is: 0x%x\n",imgSize);
        } else {
            printf("FSBL image file is not present or is empty!\n");
            return -1;
        }


        bytesRead = fread(buffer, 1, FSBL_SIZE_MAX, fsbl_bin);

        xflow_cnfg_base[0]=0x20000208;
        *xflow_vbuf_base &= ~0x30000000;
        sleep(0.01);
        *xflow_vbuf_base |= 0x70000000;
        sleep(0.01);

        if (update_qspi_firmware(buffer, imgSize)) {
            ret = -1;
        }

        /* Read Back and then compare */
        qspi_readflash(PERI_QSPI, 0, (uint8_t*)(0x400A0000), imgSize, 1, 1);
        xflow_cnfg_base[1]=0x1;
        xflow_cnfg_base[0]=0x400A0000;
        for(i=0; i<bytesRead/sizeof(u32); i++){
            if(xflow_vbuf_base[i] != buffer[i]) {
                ret = -1;
                break;
            }
        }

        if(ret == 0) {
            printf("Update Flash OK\n");
            printf("NewVer=0x%08X Date=0x%08X MODEL=0x%08X\n\n", buffer[0x6F0C>>2], buffer[0x6F10>>2], buffer[0x6FF8>>2]);
        }

        /* Close the memory */
        close(fd);

        if(((psize[j][0] == 0x1000000) && (psize[j][2] == 0x1000000)&& (psize[j][4] == 0x100000)) ||
           ((psize[j][0] == 0x4000000) && (psize[j][2] == 0x4000000)&& (psize[j][4] == 0x100000)) ) {
            err = munmap(xflow_cnfg_base, psize[j][0]);
            err |= munmap(xflow_vbuf_base, psize[j][0]);
        } else {
            err = munmap(vptr_bkup, psize[j][0]);
        }
        if(err != 0){
            printf("UnMapping Failed\n");
            return -1;
        }

        /* Close the FSBL image file */
        fclose(fsbl_bin);

    }

errr:

    if ((retry)&&(ret != 0)) {
        retry--;
        goto err_retry;
    }

    printf("##########################################################\n");
    if(ret == 0) {
        printf("*****************ALL %d Devices FLASH IMAGE upgrade OK\r\n", devcnt);
    } else {
        printf("*****************FLASH IMAGE upgrade FAILED\r\n");
    }

    printf("##########################################################\n\n");
    return ret;
}

