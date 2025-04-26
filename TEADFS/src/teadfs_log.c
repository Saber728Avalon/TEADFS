#include "teadfs_log.h"

#include <linux/kernel.h>
#include <linux/printk.h>

#define LOG_MAX_BUF 4096


void teadfs_log(enum TEADFS_LOG_LEVEL level, const char* format, ...) {
	va_list args;
	va_start(args, format); // 这里是最后一个固定参数
	switch (level)
	{
	case TLL_DBG:
		//vprintk(format, args);
		break;
	case TLL_INF:
		vprintk(format, args);
		break;
	case TLL_ERR:
		vprintk(format, args);
		break;
	case TLL_CNT:
		vprintk(format, args);
		break;
	default:
		break;
	}
	return;
}
