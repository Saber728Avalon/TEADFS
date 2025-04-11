#ifndef TEADFS_LOG_H
#define TEADFS_LOG_H

#include <stdarg.h>

enum TEADFS_LOG_LEVEL {
	TLL_DBG,
	TLL_INF,
	TLL_ERR,

	TLL_CNT,
};

void teadfs_log(enum TEADFS_LOG_LEVEL level, const char* func_name, int line, const char* format, ...);

#define LOG_DBG(...) teadfs_log(TLL_DBG, __FUNCTION__, __LINE__, __VA_ARGS__); 
#define LOG_INF(...) teadfs_log(TLL_INF, __FUNCTION__, __LINE__, __VA_ARGS__); 
#define LOG_ERR(...) teadfs_log(TLL_ERR, __FUNCTION__, __LINE__, __VA_ARGS__); 

#endif // !TEADFS_LOG_H
