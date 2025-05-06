#include "teadfs_log.h"

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/security.h>
#include <linux/compat.h>
#include <linux/fs_stack.h>
#include <linux/aio.h>


#define LOG_MAX_BUF 4096

void teadfs_write_log(const char* fmt, va_list ap);

void teadfs_log(enum TEADFS_LOG_LEVEL level, const char* format, ...) {
	va_list args;
	va_start(args, format); // 这里是最后一个固定参数
	switch (level)
	{
	case TLL_DBG:
		//vprintk(format, args);
		break;
	case TLL_INF:
		//vprintk(format, args);
		teadfs_write_log(format, args);
		break;
	case TLL_ERR:
		//vprintk(format, args);
		teadfs_write_log(format, args);
		break;
	case TLL_CNT:
		vprintk(format, args);
		teadfs_write_log(format, args);
		break;
	default:
		break;
	}
	return;
}


static struct file* g_file;

int teadfs_log_create(void) {
	int flags_new = O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE;

	g_file = filp_open("/ttt.log", flags_new, 0666);

}
int teadfs_log_release(void) {
	filp_close(g_file, NULL);

}

void teadfs_write_log(const char* fmt, va_list ap) {
	char buf[LOG_MAX_BUF] = { 0 };
	vsnprintf(buf, LOG_MAX_BUF, fmt, ap);
	kernel_write(g_file, buf, strlen(buf), g_file->f_pos);
}
