#ifndef __USER_COM_H___
#define __USER_COM_H___

#include <linux/fs.h>

//open file to user mode
int teadfs_request_open(struct file* file);

//close file to user mode
int teadfs_request_release(struct file* file);

//read file to user mode
int teadfs_request_read(const char* src_data, int src_size, char *dst_data, int dst_size);

//write file to user mode
int teadfs_request_write(const char* src_data, int src_size, char* dst_data, int dst_size);
#endif


