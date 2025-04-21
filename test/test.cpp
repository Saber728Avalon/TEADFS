#include "test.h"
#include "lib_tead_fs.h"

#include <stdio.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>


#define ENCRYPT_FILE_FLAG 0x444144554


int open(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	printf("[open] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);

	return 1;
}
int release(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	printf("[release] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);
	char chBuf[1024];
	int nRead = 0;
	char chHeader[256] = { 0 };
	int nFlag = ENCRYPT_FILE_FLAG;
	//create file
	int fdSrc = open(pszFilePath, O_RDONLY);
	if (fdSrc < 0) {
		return TRFR_NORMAL;
	}
	std::string strTmpPath = pszFilePath;
	strTmpPath += ".teadfstmp";
	int fdDst = open(strTmpPath.c_str(), O_RDWR | O_CREAT);
	if (fdDst < 0) {
		return TRFR_NORMAL;
	}
	//write header
	memcpy(chHeader, &nFlag, sizeof(nFlag));
	write(fdDst, chHeader, 256);
	do {
		nRead = read(fdSrc, chBuf, 1024);
		if (nRead <= 0) {
			break;
		}
		for (int i = 0; i < nRead; i++) {
			chBuf[i] = chBuf[i] ^ 0x13;
		}
		write(fdDst, chBuf, nRead);
	} while (1);
	std::string binData;


	return TRFR_NORMAL;
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