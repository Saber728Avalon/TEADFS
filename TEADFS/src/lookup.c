#include "teadfs_log.h"

#include <linux/stdlib.h>
#include <linux/fs.h>


static int teadfs_inode_test(struct inode* inode, void* lower_inode)
{
	if (ecryptfs_inode_to_lower(inode) == (struct inode*)lower_inode)
		return 1;
	return 0;
}

//set inode operator file callback function
static int teadfs_inode_set(struct inode* inode, void* opaque)
{
	struct inode* lower_inode = opaque;

	ecryptfs_set_inode_lower(inode, lower_inode);
	fsstack_copy_attr_all(inode, lower_inode);
	/* i_size will be overwritten for encrypted regular files */
	fsstack_copy_inode_size(inode, lower_inode);
	inode->i_ino = lower_inode->i_ino;
	inode->i_version++;
	inode->i_mapping->a_ops = &ecryptfs_aops;
	inode->i_mapping->backing_dev_info = inode->i_sb->s_bdi;

	if (S_ISLNK(inode->i_mode))
		inode->i_op = &ecryptfs_symlink_iops;
	else if (S_ISDIR(inode->i_mode))
		inode->i_op = &ecryptfs_dir_iops;
	else
		inode->i_op = &ecryptfs_main_iops;

	if (S_ISDIR(inode->i_mode))
		inode->i_fop = &ecryptfs_dir_fops;
	else if (special_file(inode->i_mode))
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
	else
		inode->i_fop = &ecryptfs_main_fops;

	return 0;
}


static struct inode* __teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb)
{
	struct inode* inode;

	// check inode to super block, if not equal. have error. 
	if (lower_inode->i_sb != teadfs_superblock_to_lower(sb))
		return ERR_PTR(-EXDEV);
	if (!igrab(lower_inode))
		return ERR_PTR(-ESTALE);
	inode = iget5_locked(sb, (unsigned long)lower_inode,
		teadfs_inode_test, teadfs_inode_set,
		lower_inode);
	if (!inode) {
		iput(lower_inode);
		return ERR_PTR(-EACCES);
	}
	if (!(inode->i_state & I_NEW))
		iput(lower_inode);

	return inode;
}


struct inode* teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb) {
	struct inode* inode = __teadfs_get_inode(lower_inode, sb);

	if (!IS_ERR(inode) && (inode->i_state & I_NEW))
		unlock_new_inode(inode);

	return inode;
}