#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int test_posix(int block_size, int blocks, int gap, int rdisk, int nocache)
{
	struct timeval start;
	gettimeofday(&start, NULL);

	int f = -1;
	if (rdisk) {
		f = open("/dev/rdisk3", O_RDWR);
     	} else {
		f = open("/dev/disk3", O_RDWR);
	}
        if (f == -1) {
		printf("open failed.\n");
		return -1;
	}

	fcntl(f, F_NOCACHE, nocache);

	void* buffer = malloc(block_size);
	if (!buffer) {
		printf("malloc failed.\n");
		return -1;
	}

	for (int i = 0; i < blocks; ++i) {
		write(f, buffer, block_size);
		if (gap > 0) {
			lseek(f, gap, SEEK_CUR);
		}
	}

	close(f);

	struct timeval end;
	gettimeofday(&end, NULL);
	float duration = ((end.tv_sec + end.tv_usec / 1e6) - (start.tv_sec + start.tv_usec / 1e6));
	printf("POSIX: block_size=%d blocks=%d gap=%d rdisk=%d nocache=%d time=%f\n", block_size, blocks, gap, rdisk, nocache, duration);
	return 0;
}


int test_stdio(int block_size, int blocks, int gap, int rdisk)
{
	struct timeval start;
	gettimeofday(&start, NULL);

	FILE* f = NULL;
	if (rdisk) {
		f = fopen("/dev/rdisk3", "r+b");
     	} else {
		f = fopen("/dev/disk3", "r+b");
	}
        if (!f) {
		printf("fopen failed.\n");
		return -1;
	}

	setbuf(f, 0);

	void* buffer = malloc(block_size);
	if (!buffer) {
		printf("malloc failed.\n");
		return -1;
	}

	for (int i = 0; i < blocks; ++i) {
		fwrite(buffer, block_size, 1, f);
		if (gap > 0) {
			fseek(f, gap, SEEK_CUR);
		}
	}

	fclose(f);

	struct timeval end;
	gettimeofday(&end, NULL);
	float duration = ((end.tv_sec + end.tv_usec / 1e6) - (start.tv_sec + start.tv_usec / 1e6));
	printf("STDIO: block_size=%d blocks=%d gap=%d rdisk=%d time=%f\n", block_size, blocks, gap, rdisk, duration);
	return 0;
}


int main()
{
	for (int i = 0; i < 2; i++) {
/*
		test_posix(4096, 4096, 0, i);
		test_posix(4096, 4096, 0, i);
		test_posix(8192, 2048, 0, i);
		test_posix(4096, 4096, 4096, i);
		test_posix(4096, 4096, 65536, i);
		test_posix(8192, 2048, 65536, i);
		test_posix(16384, 1024, 65536, i);
*/
		for (int j = 0; j < 2; j++) {
			// test_posix(4096, 262144, 0, i, j);
			// test_posix(8192, 131072, 0, i, j);
			// test_posix(16384, 65536, 0, i, j);
			test_posix(32768, 32768, 0, i, j);
			test_posix(65536, 16384, 0, i, j);
			test_posix(131072, 8192, 0, i, j);
			test_posix(262144, 4096, 0, i, j);
			test_posix(524288, 2048, 0, i, j);
		}
	}
}

