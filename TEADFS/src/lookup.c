#include "lookup.h"
#include "teadfs_log.h"
#include "teadfs_header.h"
#include "mem.h"
#include "inode.h"
#include "mmap.h"
#include "inode.h"
#include "file.h"

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/fs_stack.h>
#include <linux/mount.h>
#include <linux/dcache.h>
#include <linux/namei.h> 

static int teadfs_inode_test(struct inode* inode, void* lower_inode)
{
	if (teadfs_inode_to_lower(inode) == (struct inode*)lower_inode)
		return 1;
	return 0;
}

//set inode operator file callback function
static int teadfs_inode_set(struct inode* inode, void* opaque)
{
	struct inode* lower_inode = opaque;

	teadfs_set_inode_lower(inode, lower_inode);
	fsstack_copy_attr_all(inode, lower_inode);
	/* i_size will be overwritten for encrypted regular files */
	fsstack_copy_inode_size(inode, lower_inode);
	inode->i_ino = lower_inode->i_ino;
	inode->i_version++;
	inode->i_mapping->a_ops = &teadfs_aops;
	inode->i_mapping->backing_dev_info = inode->i_sb->s_bdi;

	if (S_ISLNK(inode->i_mode))
		inode->i_op = &teadfs_symlink_iops;
	else if (S_ISDIR(inode->i_mode))
		inode->i_op = &teadfs_dir_iops;
	else
		inode->i_op = &teadfs_main_iops;

	if (S_ISDIR(inode->i_mode))
		inode->i_fop = &teadfs_dir_fops;
	else if (special_file(inode->i_mode))
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
	else
		inode->i_fop = &teadfs_main_fops;

	return 0;
}


struct inode* __teadfs_get_inode(struct inode* lower_inode,
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


/**
 * teadfs_lookup_interpose - Dentry interposition for a lookup
 */
static int teadfs_lookup_interpose(struct dentry* dentry,
	struct dentry* lower_dentry,
	struct inode* dir_inode)
{
	struct inode* inode, * lower_inode = lower_dentry->d_inode;
	struct teadfs_dentry_info* dentry_info;
	struct vfsmount* lower_mnt;
	int rc = 0;

	dentry_info = teadfs_zalloc(sizeof(struct teadfs_dentry_info), GFP_KERNEL);
	if (!dentry_info) {
		LOG_ERR("%s: Out of memory whilst attempting "
			"to allocate ecryptfs_dentry_info struct\n",
			__func__);
		dput(lower_dentry);
		return -ENOMEM;
	}

	lower_mnt = mntget(teadfs_dentry_to_lower_path(dentry->d_parent)->mnt);
	fsstack_copy_attr_atime(dir_inode, lower_dentry->d_parent->d_inode);

	teadfs_set_dentry_private(dentry, dentry_info);
	teadfs_set_dentry_lower(dentry, lower_dentry);
	teadfs_dentry_to_lower_path(dentry)->mnt = lower_mnt;

	if (!lower_dentry->d_inode) {
		/* We want to add because we couldn't find in lower */
		d_add(dentry, NULL);
		return 0;
	}
	inode = __teadfs_get_inode(lower_inode, dir_inode->i_sb);
	if (IS_ERR(inode)) {
		LOG_ERR("%s: Error interposing; rc = [%ld]\n",
			__func__, PTR_ERR(inode));
		return PTR_ERR(inode);
	}

	if (inode->i_state & I_NEW)
		unlock_new_inode(inode);
	d_add(dentry, inode);

	return rc;
}



/**
 * teadfs_lookup
 * @ecryptfs_dir_inode: The eCryptfs directory inode
 * @ecryptfs_dentry: The eCryptfs dentry that we are looking up
 * @ecryptfs_nd: nameidata; may be NULL
 *
 * Find a file on disk. If the file does not exist, then we'll add it to the
 * dentry cache and continue on to read it from the disk.
 */
struct dentry* teadfs_lookup(struct inode* ecryptfs_dir_inode,
	struct dentry* ecryptfs_dentry,
	unsigned int flags)
{
	struct dentry* lower_dir_dentry, * lower_dentry;
	int rc = 0;

	LOG_DBG("ENTRY\n");
	do {
		lower_dir_dentry = teadfs_dentry_to_lower(ecryptfs_dentry->d_parent);
		mutex_lock(&lower_dir_dentry->d_inode->i_mutex);
		lower_dentry = lookup_one_len(ecryptfs_dentry->d_name.name,
			lower_dir_dentry,
			ecryptfs_dentry->d_name.len);
		mutex_unlock(&lower_dir_dentry->d_inode->i_mutex);
		if (IS_ERR(lower_dentry)) {
			rc = PTR_ERR(lower_dentry);
			LOG_ERR("%s: lookup_one_len() returned "
				"[%d] on lower_dentry = [%s]\n", __func__, rc,
				ecryptfs_dentry->d_name.name);
			break;
		}
		if (lower_dentry->d_inode)
			break;
		dput(lower_dentry);
	} while (0);
	rc = teadfs_lookup_interpose(ecryptfs_dentry, lower_dentry,
		ecryptfs_dir_inode);
	LOG_DBG("rc = [%d]\n", rc);
	return ERR_PTR(rc);
}


struct inode* teadfs_get_inode(struct inode* lower_inode,
	struct super_block* sb) {
	struct inode* inode = __teadfs_get_inode(lower_inode, sb);

	if (!IS_ERR(inode) && (inode->i_state & I_NEW))
		unlock_new_inode(inode);

	return inode;
}