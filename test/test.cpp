#include "test.h"
#include <stdio.h>
#include "lib_tead_fs.h"
int open(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	printf("[open] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);

	return 1;
}
int release(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	printf("[release] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);

	return 1;
}
int read(uint32_t u32SrcSize, char* pSrcData, uint32_t* u32DstSize, char* pDstData) {
	printf("[read] size :%d\n", u32SrcSize);
	for (int i = 0; i < u32SrcSize; i++) {
		pDstData[i] = pSrcData[i] ^ 0x13;
	}
	*u32DstSize = u32SrcSize;
	return 1;
}

int write(uint32_t u32SrcSize, char* pSrcData, uint32_t* u32DstSize, char* pDstData) {
	printf("[write] size :%d\n", u32SrcSize);
	for (int i = 0; i < u32SrcSize; i++) {
		pDstData[i] = pSrcData[i] ^ 0x13;
	}
	*u32DstSize = u32SrcSize;
	return 1;
}

int cleanup(uint64_t u64FileId) {

}
int main(int argc, char *argv[]) {
	printf("start test\n");

	TEAFS_DEAL_CB cb = {
		.open = open
		, .release = release
		, .read = read
		, .write = write
		, .cleanup = cleanup
	};
	StartTEADFS(cb);

	while (1){
		getchar();
	}

	return 0;
}