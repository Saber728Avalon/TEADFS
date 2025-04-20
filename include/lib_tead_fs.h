#ifndef LIB_TEAD_FS_H
#define LIB_TEAD_FS_H

#include <iostream>


	enum TEADFS_OPEN_RESULT {
		TOR_INIT,
		TOR_ENCRYPT,// file be encrypt, and access by decrypt
		TOR_DECRYPT,// file be encrypt, and access by decrypt

		TOR_COUNT
		};

	enum TEADFS_RELEASE_FILE_RESULT {
		TRFR_NORMAL, // Do not take any action
		TRFR_ENCRYPT, // encrypt file.

		TRFR_COUNT
	};

	struct TEAFS_DEAL_CB {
		int (*open)(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath);
		int (*release)(uint64_t u64FileId, uint32_t u32PID, char* pszFilePath);
		int (*read)(uint32_t u32SrcSize, char *pSrcData, uint32_t *u32DstSize, char* pDstData);
		int (*write)(uint32_t u32SrcSize, char* pSrcData, uint32_t* u32DstSize, char* pDstData);
		int (*cleanup)(uint64_t u64FileId);
	};
	//start and connect fs
	int StartTEADFS(struct TEAFS_DEAL_CB cb);
	
#endif //LIB_TEAD_FS_H