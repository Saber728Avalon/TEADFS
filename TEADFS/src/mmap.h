#ifndef MMAP_H
#define MMAP_H

#include <linux/fs.h>

extern const struct address_space_operations teadfs_aops;

int truncate_upper(struct dentry* dentry, struct iattr* ia,
	struct iattr* lower_ia);
#endif // !MMAP_H
