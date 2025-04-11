#ifndef LOOKUP_H
#define LOOKUP_H

#include <linux/fs.h>

static struct inode* __teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb);

//get inode
struct inode* teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb);

#endif // !LOOKUP_H
