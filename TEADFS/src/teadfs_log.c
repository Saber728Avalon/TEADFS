#include "teadfs_log.h"

#include <linux/printk.h>

void teadfs_log(enum TEADFS_LOG_LEVEL level, const char* func_name, int line, const char* format, ...) {
	va_list args;
	va_start(args, format);
	switch (level)
	{
	case TLL_DBG:
		printk(format, func_name, line,  args);
		break;
	case TLL_INF:
		printk(format, func_name, line, args);
		break;
	case TLL_ERR:
		printk(format, func_name, line, args);
		break;
	case TLL_CNT:
		printk(format, func_name, line, args);
		break;
	default:
		break;
	}
	va_end(args);
	return;
}