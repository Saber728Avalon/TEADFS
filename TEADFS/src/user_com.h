#ifndef __USER_COM_H___
#define __USER_COM_H___

#include <linux/fs.h>

//open file to user mode
int teadfs_request_open_file(struct file* file);
int teadfs_request_open_path(struct path* path);

//close file to user mode
int teadfs_request_release(struct file* file);

//read file to user mode
int teadfs_request_read(loff_t offset, const char* src_data, int src_size, char *dst_data, int dst_size);

//write file to user mode
int teadfs_request_write(loff_t offset, const char* src_data, int src_size, char* dst_data, int dst_size);
#endif


