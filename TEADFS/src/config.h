#ifndef CONFIG_H
#define CONFIG_H

#include <linux/version.h>

//
#if KERNEL_VERSION(3, 10 , 0) <= LINUX_VERSION_CODE
	#define CONFIG_BDICONFIG_BDI
#endif

#if KERNEL_VERSION(3, 10 , 0) <= LINUX_VERSION_CODE
	#define CONFIG_VFS_UNLINK_3_PARAM
#endif

#if KERNEL_VERSION(3, 11 , 0) >= LINUX_VERSION_CODE
#define CONFIG_ITERATE_DIR 
#endif


#endif // !CONFIG_H
