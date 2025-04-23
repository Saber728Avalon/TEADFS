#ifndef __MISCDEV_H___
#define __MISCDEV_H___

#include <linux/fs.h>

int teadfs_init_miscdev(void);


void teadfs_destroy_miscdev(void);
#endif


