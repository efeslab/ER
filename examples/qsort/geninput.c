#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SIZE 1024
int a[SIZE];
int main(int argc, char **argv) {
	if (argc < 2) {
		printf("Usage: %s out_file\n", argv[0]);
	}
	FILE *f = fopen(argv[1], "wb");
	if (!f) {
		perror("Open file failed");
		exit(-1);
	}
	srand(time(NULL));
	for (int i = 0; i < SIZE; ++i) {
		a[i] = rand();
	}
	fwrite(a, sizeof(a[0]), SIZE, f);
	fclose(f);
}
