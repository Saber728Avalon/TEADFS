#include "test.h"
#include "lib_tead_fs.h"

#include <stdio.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>


#define ENCRYPT_FILE_FLAG 0x44414554


#define ENCRYPT_FILE_HEADER_SIZE 256

int open(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	printf("[open] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);
	char chHeader[ENCRYPT_FILE_HEADER_SIZE] = { 0 };
	TEADFS_OPEN_RESULT result;
	int fdSrc = -1;
	do {
		fdSrc = open(pszFilePath, O_RDONLY);
		if (fdSrc < 0) {
			result = TOR_INIT;
		}
		int nRead = read(fdSrc, chHeader, ENCRYPT_FILE_HEADER_SIZE);
		
		if (nRead < ENCRYPT_FILE_HEADER_SIZE) {
			result = TOR_INIT;
		}
		if (ENCRYPT_FILE_FLAG == *(uint32_t*)chHeader) {
			result = TOR_DECRYPT;
		}
	} while (0);
	if (fdSrc > 0) {
		close(fdSrc);
	}
	printf("[open] result:%d\n", result);
	return result;
}
int release(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	printf("[release] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);
	char chBuf[1024];
	int nRead = 0;
	char chHeader[ENCRYPT_FILE_HEADER_SIZE] = { 0 };
	int nFlag = ENCRYPT_FILE_FLAG;

	std::string strFilePath = pszFilePath;
	if (std::string::npos == strFilePath.find(".txt")) {
		return TRFR_NORMAL;
	}

	//create file
	int fdSrc = open(pszFilePath, O_RDONLY);
	if (fdSrc < 0) {
		return TRFR_NORMAL;
	}
	nRead = pread(fdSrc, chHeader, ENCRYPT_FILE_HEADER_SIZE, 0);
	if (nRead >= ENCRYPT_FILE_HEADER_SIZE) {
		if (ENCRYPT_FILE_FLAG == *(uint32_t*)chHeader) {
			return TRFR_NORMAL;
		}
	}
	std::string strTmpPath = pszFilePath;
	strTmpPath += ".teadfstmp";
	int fdDst = open(strTmpPath.c_str(), O_RDWR | O_CREAT);
	if (fdDst < 0) {
		return TRFR_NORMAL;
	}
	//write header
	memcpy(chHeader, &nFlag, sizeof(nFlag));
	write(fdDst, chHeader, ENCRYPT_FILE_HEADER_SIZE);
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
	close(fdDst);
	close(fdSrc);

	//unlink(pszFilePath);
	rename(strTmpPath.c_str(), pszFilePath);
	return TRFR_NORMAL;
}
int read(uint64_t offset, uint32_t u32SrcSize, char* pSrcData, uint32_t* u32DstSize, char* pDstData) {
	printf("[read] offset:%lld, size :%d\n", offset, u32SrcSize);
	for (int i = 0; i < u32SrcSize; i++) {
		pDstData[i] = pSrcData[i] ^ 0x13;
	}
	*u32DstSize = u32SrcSize;
	printf("[read]  dest size :%d\n", *u32DstSize);
	return 1;
}

int write(uint64_t offset,  uint32_t u32SrcSize, char* pSrcData, uint32_t* u32DstSize, char* pDstData) {
	printf("[write]  offset:%lld, size :%d\n", offset, u32SrcSize);
	for (int i = 0; i < u32SrcSize; i++) {
		pDstData[i] = pSrcData[i] ^ 0x13;
	}
	*u32DstSize = u32SrcSize;
	printf("[read]  dest size :%d\n", *u32DstSize);
	return 1;
}

int cleanup(uint64_t u64FileId) {

}




void TestFunc() {
	char* pData = "Abc123,./";
	char* pFilePath = "/test/4.txt";

	printf("Start Edif:%s \n");
	int fdDst = open(pFilePath, O_RDWR);
	if (fdDst < 0) {
		printf("open file \n");
		return;
	}
	pwrite(fdDst, pData, strlen(pData), 3);
	close(fdDst);
	printf("Edif Suc \n");
}
int main(int argc, char *argv[]) {
	printf("start test argc: %d\n", argc);
	if (argc > 1) {
		TestFunc();
	}

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