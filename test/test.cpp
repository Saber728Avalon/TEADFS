#include "test.h"
#include "lib_tead_fs.h"

#include <stdio.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include <utime.h>


#define ENCRYPT_FILE_FLAG 0x44414554


#define ENCRYPT_FILE_HEADER_SIZE 256







std::string GetProcPath(uint32_t u32PID) {
	char link_buf[64];
	char path[PATH_MAX];
	sprintf(link_buf, "/proc/%d/exe", u32PID);
	ssize_t len = readlink(link_buf, path, sizeof(path) - 1);
	if (len != -1) {
		path[len] = '\0';
	}
	std::string strPath = path;
	return strPath;
}

std::string GetFileName(std::string strPath) {
	std::string strFileName = strPath;
	size_t nPos = strPath.rfind("/");
	if (std::string::npos != nPos) {
		strFileName = strPath.substr(nPos);
	}
	return strFileName;
}



int open(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath) {
	
	printf("[open] file id:%0xllx, pid:%d path:%s\n", u64FileId, u32PID, pszFilePath);
	std::string strProcPath = GetProcPath(u32PID);



	char chHeader[ENCRYPT_FILE_HEADER_SIZE] = { 0 };
	TEADFS_OPEN_RESULT result = TOR_INIT;
	int fdSrc = -1;
	do {
		fdSrc = open(pszFilePath, O_RDONLY);
		if (fdSrc < 0) {
			break;
		}
		int nRead = read(fdSrc, chHeader, ENCRYPT_FILE_HEADER_SIZE);
		
		if (nRead < ENCRYPT_FILE_HEADER_SIZE) {
			break;
		}
		if (ENCRYPT_FILE_FLAG == *(uint32_t*)chHeader) {
			if (std::string::npos != strProcPath.find("cat")) {
				result = TOR_ENCRYPT;
			} else {
				result = TOR_DECRYPT;
			}
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
	std::string strFileName = GetFileName(strFilePath);
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


	struct stat stat = { 0 };
	fstat(fdSrc, &stat);
	fchmod(fdDst, stat.st_mode);
	
	fchown(fdDst, stat.st_uid, stat.st_gid);
	struct timespec times[2] = { 0 };

	times[0] = stat.st_atim;
	times[1] = stat.st_mtim;
	futimens(fdDst, times);


	close(fdDst);
	close(fdSrc);
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
	printf("[write]  dest size :%d\n", *u32DstSize);
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