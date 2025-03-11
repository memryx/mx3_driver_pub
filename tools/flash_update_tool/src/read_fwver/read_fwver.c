#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[]) {
	FILE *fp;
	char buffer[8];
	int value, date = 0x0;
	int chipnum = 0x0;
	int param_location = 0x0;

	if (argc < 3 || strcmp(argv[1], "-f") != 0) {
		printf("Usage: ./read_ver -f filepath. Use local path, default filename\n\n");
		fp = fopen("cascade.bin", "rb");
		param_location = 1;
	} else {
		const char* filename = argv[2];
		fp = fopen(filename, "rb");
		param_location = 3;
	}


	if (fp == NULL) {
		perror("Error opening file");
		return 1;
	}

	fseek(fp, 28428, SEEK_SET);

	if (fread(buffer, 1, 8, fp) != 8) {
		perror("Error reading file");
		fclose(fp);
		return 1;
	}

	value = *(int*)buffer;
	date = *(int*)(buffer + 4);

	fseek(fp, 28664, SEEK_SET);
        if (fread(buffer, 1, 4, fp) != 4) {
                perror("Error reading file");
                fclose(fp);
                return 1;
        }
	chipnum = *(int*)buffer;
	chipnum = (chipnum >> 16);

	if ((argc > param_location) && (strcmp(argv[param_location], "-v") == 0)) {
		printf("0x%x", value);
		fclose(fp);
		return value;
	} else if ((argc > param_location) && (strcmp(argv[param_location], "-d") == 0)) {
		printf("0x%x", date);
		fclose(fp);
		return date;
	} else if ((argc > param_location) && (strcmp(argv[param_location], "-c") == 0)) {
		printf("%d", chipnum);
		fclose(fp);
		return chipnum;
	} else {
		printf("firmware version: 0x%x, date: 0x%x, chipnum: %d\n", value, date, chipnum);
		fclose(fp);
		return 0;
	}
}
