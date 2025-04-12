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

#define LOG_DBG(fmt, args...) teadfs_log(TLL_DBG, "%s %d [dbg]" fmt, __FUNCTION__, __LINE__, ##args); 
#define LOG_INF(fmt, args...) teadfs_log(TLL_INF, "%s %d [dbg]" fmt, __FUNCTION__, __LINE__, ##args); 
#define LOG_ERR(fmt, args...) teadfs_log(TLL_ERR, "%s %d [dbg]" fmt, __FUNCTION__, __LINE__, ##args); 

#endif // !TEADFS_LOG_H
