/**
 * eCryptfs: Linux filesystem encryption layer
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2006 International Business Machines Corp.
 *   Author(s): Michael A. Halcrow <mahalcro@us.ibm.com>
 *              Michael C. Thompson <mcthomps@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "super.h"
#include "teadfs_log.h"
#include "teadfs_header.h"
#include "mem.h"

#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/key.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/file.h>
#include <linux/crypto.h>
#include <linux/statfs.h>
#include <linux/magic.h>
#include <linux/mm.h>


/**
 * teadfs_alloc_inode - allocate an ecryptfs inode
 * @sb: Pointer to the ecryptfs super block
 *
 * Called to bring an inode into existence.
 *
 * Only handle allocation, setting up structures should be done in
 * ecryptfs_read_inode. This is because the kernel, between now and
 * then, will 0 out the private data pointer.
 *
 * Returns a pointer to a newly allocated inode, NULL otherwise
 */
static struct inode *teadfs_alloc_inode(struct super_block *sb)
{
	struct teadfs_inode_info*inode_info;
	struct inode *inode = NULL;

	LOG_DBG("ENTRY\n");
	do {
		inode_info = teadfs_zalloc(sizeof(struct teadfs_inode_info), GFP_KERNEL);
		if (unlikely(!inode_info))
			break;
		mutex_init(&inode_info->lower_file_mutex);
		atomic_set(&inode_info->lower_file_count, 0);
		inode = &inode_info->vfs_inode;
	} while (0);

	LOG_DBG("LEVAL\n");
	return inode;
}

static void teadfs_i_callback(struct rcu_head *head)
{
	struct inode *inode = container_of(head, struct inode, i_rcu);
	struct teadfs_inode_info *inode_info;
	inode_info = teadfs_inode_to_private(inode);

	LOG_DBG("ENTRY\n");
	teadfs_free(inode_info);
	LOG_DBG("LEVAL\n");
}

/**
 * teadfs_destroy_inode
 * @inode: The ecryptfs inode
 *
 * This is used during the final destruction of the inode.  All
 * allocation of memory related to the inode, including allocated
 * memory in the crypt_stat struct, will be released here.
 * There should be no chance that this deallocation will be missed.
 */
static void teadfs_destroy_inode(struct inode *inode)
{
	struct teadfs_inode_info *inode_info;
	LOG_DBG("ENTRY\n");
	inode_info = teadfs_inode_to_private(inode);
	BUG_ON(inode_info->lower_inode);
	call_rcu(&inode->i_rcu, teadfs_i_callback);
	LOG_DBG("LEVAL\n");
}

/**
 * teadfs_statfs
 * @sb: The ecryptfs super block
 * @buf: The struct kstatfs to fill in with stats
 *
 * Get the filesystem statistics. Currently, we let this pass right through
 * to the lower filesystem and take no action ourselves.
 */
static int teadfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
	struct dentry *lower_dentry = teadfs_dentry_to_lower(dentry);
	int rc;

	LOG_DBG("ENTRY\n");
	do {
		if (!lower_dentry->d_sb->s_op->statfs) {
			rc = -ENOSYS;
			break;
		}
		rc = lower_dentry->d_sb->s_op->statfs(lower_dentry, buf);
		if (rc)
			break;

		buf->f_type = TEADFS_SUPER_MAGIC;
	} while (0);
	LOG_DBG("LEVAL %d\n", rc);
	return rc;
}

/**
 * teadfs_evict_inode
 * @inode - The ecryptfs inode
 *
 * Called by iput() when the inode reference count reached zero
 * and the inode is not hashed anywhere.  Used to clear anything
 * that needs to be, before the inode is completely destroyed and put
 * on the inode free list. We use this to drop out reference to the
 * lower inode.
 */
static void teadfs_evict_inode(struct inode *inode)
{
	LOG_DBG("ENTRY\n");
	truncate_inode_pages(&inode->i_data, 0);
	clear_inode(inode);
	iput(teadfs_inode_to_lower(inode));
	LOG_DBG("LEVAL\n");
}

/**
 * teadfs_show_options
 *
 * Prints the mount options for a given superblock.
 * Returns zero; does not fail.
 */
static int teadfs_show_options(struct seq_file *m, struct dentry *root)
{
	seq_printf(m, "teadfs");

	return 0;
}

const struct super_operations teadfs_sops = {
	.alloc_inode = teadfs_alloc_inode,
	.destroy_inode = teadfs_destroy_inode,
	.statfs = teadfs_statfs,
	.remount_fs = NULL,
	.evict_inode = teadfs_evict_inode,
	.show_options = teadfs_show_options
};
