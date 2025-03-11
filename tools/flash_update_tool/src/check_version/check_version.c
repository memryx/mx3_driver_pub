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

#define version "V1.0"

#define FSBL_SIZE_MAX 	0x40000

// Make the SDK console work in the debugger
#define printf(...) \
 fprintf(stdout, __VA_ARGS__); \
 fflush(stdout);

int get_pcie_info(long long*, long*, int);


typedef uint64_t u64;
typedef uint32_t u32;

static u32 *xflow_cnfg_base, *xflow_vbuf_base;

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

int main(int argc, char **argv)
{
	char *p;
	FILE *fsbl_bin=NULL;
	u32 *vptr, *vptr_bkup;
	int err=0, imgSize=0, crc32=0, fd=0;
	clock_t start_t, end_t;
	u32 waittime;
	u32 buffer[FSBL_SIZE_MAX/sizeof(u32)];
	size_t bytesRead;
	int c,i,j;
	int retry = 2;
	int devcnt, ret = 0;
	char *filename=0;
	int isFlashBoot=0;
	u32 imageVersion=0,imageDate=0,imageMODEL=0;
	int verbose = 0;
	int devid = 1;
	int retmod = 0;

	long long pbase[16][6] = {0};
	long psize[16][6] = {0};



	while ((c = getopt (argc, argv, "i:htbfvd:")) != -1) {
		switch (c) {
		case 'i':
			filename = optarg;
			//printf("input image file: %s\n", filename);
			break;
		case 'h':
			printf("$sudo %s -t -b -f -v -d 1\n", argv[0]);
			printf("(-t): return code 0:A0_chip   1:A1_chip\n");//retmod 0
			printf("(-b): return code 0:pcie_boot 1:flash_boot\n");//retmod 1
			printf("(-f): return code 32bits firmware version code\n");//retmod 2
			printf("t/b/f can only choose one\n");
			printf("(-v): verbose the parsing details\n");
			printf("(-d): select return target if multiple devices existed. default is 1 for first device\n");
			abort ();
		case 'd': {
			char *p;
			devid = strtol(optarg, &p, 16);
		} break;
		case 't': {
			retmod = 0;//return A0 A1
		} break;
		case 'b': {
			retmod = 1;//return 1: flash boot  0: pcie boot
		} break;
		case 'f': {
			retmod = 2;//return commit id
		} break;
		case 'v':
			verbose = 1;
			break;
		default:
			fprintf (stderr, "Unknown option `-%c'.\n", (char)c);
			abort ();
		}
	}

	if(verbose) {
		printf("\n########## Memryx Version Check Tool (%s)(dev%d)(retmod%d) ###############\n", version, devid, retmod);
	}


err_retry:

	/* Check if Cadence PCIe RP is up or not */
	devcnt = get_pcie_info(&pbase[0][0], &psize[0][0], verbose);
	if (devcnt == 0) {
		printf("Memryx PCIe EP not found!\n");
		return -1;
	}

	for(j=0; j< devcnt; j++) {

		if(verbose) {
		printf("(DEICE%d): Start Version Check:***********************************\n", j+1);
		printf("(DEICE%d): BAR0: Base Address: 0x%016llX  Size=0x%08lX\n", j+1, pbase[j][0], psize[j][0]);
		printf("(DEICE%d): BAR1: Base Address: 0x%016llX  Size=0x%08lX\n", j+1, pbase[j][1], psize[j][1]);
		printf("(DEICE%d): BAR2: Base Address: 0x%016llX  Size=0x%08lX\n", j+1, pbase[j][2], psize[j][2]);
		printf("(DEICE%d): BAR3: Base Address: 0x%016llX  Size=0x%08lX\n", j+1, pbase[j][3], psize[j][3]);
		printf("(DEICE%d): BAR4: Base Address: 0x%016llX  Size=0x%08lX\n", j+1, pbase[j][4], psize[j][4]);
		printf("(DEICE%d): BAR5: Base Address: 0x%016llX  Size=0x%08lX\n", j+1, pbase[j][5], psize[j][5]);
		}

		if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
			printf("Access /dev/mem failed\n");
			return -1;
		}

		isFlashBoot = 0;
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
			//isFlashBoot = 1;
		} else if(((psize[j][0] == 0x1000000) && (psize[j][2] == 0x1000000)&& (psize[j][4] == 0x100000)) ||
				  ((psize[j][0] == 0x4000000) && (psize[j][2] == 0x4000000)&& (psize[j][4] == 0x100000)) ) {
			vptr = (u32 *)mmap(NULL, psize[j][0], PROT_READ|PROT_WRITE, MAP_SHARED, fd, pbase[j][0]);
			xflow_vbuf_base = (u32 *) &(vptr[0]);
			vptr = (u32 *)mmap(NULL, psize[j][2], PROT_READ|PROT_WRITE, MAP_SHARED, fd, pbase[j][2]);
			xflow_cnfg_base = (u32 *) &(vptr[0]);
			//isFlashBoot = 1;
		} else {
			printf("None Supported BAR Mapping\n");
			close(fd);
			return -1;
		}

		if (filename)
			fsbl_bin=fopen(filename,"rb");

		if(fsbl_bin != NULL) {
			fseek(fsbl_bin,0L,SEEK_END);
			imgSize = ftell(fsbl_bin);
			fseek(fsbl_bin,0L,SEEK_SET);
			//printf("Image size is: 0x%x\n",imgSize);
			bytesRead = fread(buffer, 1, FSBL_SIZE_MAX, fsbl_bin);
			//printf("Image Ver=0x%08X Date=0x%08X MODEL=0x%08X\n\n", buffer[0x6F0C>>2], buffer[0x6F10>>2], buffer[0x6FF8>>2]);
			imageVersion = buffer[0x6F0C>>2];
			imageDate    = buffer[0x6F10>>2];
			imageMODEL   = buffer[0x6FF8>>2];
		}

		//printf("0x%08X\n", memx_read32(0x20000100));
		if(((memx_read32(0x20000100) >> 7) & 0x3) == 0x0) {
			isFlashBoot = 1;
		} else {
			isFlashBoot = 0;
		}


		memx_write32(0x20000500, 0x0);
		if(verbose)
			printf("(DEICE%d): CHIP-A%d / %s boot\n", j+1, (memx_read32(0x20000500)==0x5)?1:0, isFlashBoot ? "FLASH" : "PCIE");
		if (isFlashBoot) {
			if(verbose)
				printf("(DEICE%d): Version(0x%08X) DateCode(0x%08X) Model(0x%08X)\n", j+1, memx_read32(0x40046f08), memx_read32(0x40046f0C), memx_read32(0x40046ff4));
			if(imageVersion || imageDate || imageMODEL) {
				if(verbose)
					printf("ImageVer: Version(0x%08X) DateCode(0x%08X) Model(0x%08X)\n", buffer[0x6F0C>>2], buffer[0x6F10>>2], buffer[0x6FF8>>2]);
				if ((buffer[0x6F0C>>2] == memx_read32(0x40046f08)) && (buffer[0x6F10>>2] == memx_read32(0x40046f0C)) && (buffer[0x6FF8>>2] == memx_read32(0x40046ff4))) {
					if(verbose)
						printf("(DEICE%d): MACTH with the image firmware version\n", j+1);
				} else {
					if(verbose)
						printf("(DEICE%d): DIFFERENT with the image firmware version\n", j+1);
				}
			}
		}

		if(devid == (j+1)) {
			if (retmod == 0) {
				ret = (memx_read32(0x20000500)==0x5)?1:0;
			} else if (retmod == 1) {
				ret = (isFlashBoot) ? 1:0;
			} else if (retmod == 2) {
				ret = memx_read32(0x40046f08);
			}
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
		if (fsbl_bin)
			fclose(fsbl_bin);

	}

	if(verbose) {
		if (retmod == 0) {
			printf("ReturnCode DEVICE%d CHIP-A%d\n", devid, ret);
		} else if (retmod == 1) {
			printf("ReturnCode DEVICE%d %s boot\n", devid, ret? "FLASH":"PCIE");
		} else if (retmod == 2) {
			printf("ReturnCode DEVICE%d VERSION 0x%08X\n", devid, ret);
		}
	}

	printf("0x%X\n", ret);
	return ret;
}

