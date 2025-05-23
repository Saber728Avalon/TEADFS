#ifndef TEADFS_LOG_H
#define TEADFS_LOG_H

#include <linux/kernel.h>

enum TEADFS_LOG_LEVEL {
	TLL_DBG,
	TLL_INF,
	TLL_ERR,

	TLL_CNT,
};

void teadfs_log(enum TEADFS_LOG_LEVEL level, const char* format, ...);

int teadfs_log_create(void);
int teadfs_log_release(void);

#define LOG_DBG(FMT, ...) teadfs_log(TLL_DBG, "%s(%d)[debug] " FMT, __FUNCTION__, __LINE__, ##__VA_ARGS__); 
#define LOG_INF(FMT, ...) teadfs_log(TLL_INF, "%s(%d)[info ] " FMT, __FUNCTION__, __LINE__, ##__VA_ARGS__); 
#define LOG_ERR(FMT, ...) teadfs_log(TLL_ERR, "%s(%d)[error] " FMT, __FUNCTION__, __LINE__, ##__VA_ARGS__); 

#endif // !TEADFS_LOG_H
