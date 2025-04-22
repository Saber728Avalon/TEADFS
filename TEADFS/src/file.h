#ifndef FILE_H
#define FILE_H


#include <linux/fs.h>

extern const struct file_operations teadfs_main_fops;

extern const struct file_operations teadfs_dir_fops;

struct file* teadfs_get_lower_file(struct dentry* dentry, struct inode* inode, int flags);

void teadfs_put_lower_file(struct inode* inode, struct file* file);

#endif // !FILE_H
