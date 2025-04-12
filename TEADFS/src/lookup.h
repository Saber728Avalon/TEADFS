#ifndef LOOKUP_H
#define LOOKUP_H

#include <linux/fs.h>

struct inode* __teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb);

//get inode
struct inode* teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb);

struct dentry* teadfs_lookup(struct inode* ecryptfs_dir_inode,
	struct dentry* ecryptfs_dentry,
	unsigned int flags);
#endif // !LOOKUP_H
