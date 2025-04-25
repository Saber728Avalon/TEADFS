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



// /*
//  * These are initializations that only need to be done
//  * once, because the fields are idempotent across use
//  * of the inode, so let the slab aware of that.
//  */
//void teadfs_inode_init_once(struct inode* inode)
//{
//	memset(inode, 0, sizeof(*inode));
//	INIT_HLIST_NODE(&inode->i_hash);
//	INIT_LIST_HEAD(&inode->i_devices);
//	INIT_LIST_HEAD(&inode->i_wb_list);
//	INIT_LIST_HEAD(&inode->i_lru);
//#if defined(CONFIG_INDOE_HAS_IO_LIST)
//	INIT_LIST_HEAD(&inode->i_io_list);
//#endif
//	i_size_ordered_init(inode);
//#ifdef CONFIG_FSNOTIFY
//	INIT_HLIST_HEAD(&inode->i_fsnotify_marks);
//#endif
//}

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
		//init
		inode_init_once(&(inode_info->vfs_inode));
		mutex_init(&inode_info->lower_file_mutex);
		atomic_set(&inode_info->lower_file_count, 0);
		address_space_init_once(&(inode_info->i_decrypt));
		inode = &inode_info->vfs_inode;
	} while (0);

	LOG_DBG("LEVAL\n");
	return inode;
}

static void teadfs_i_callback(struct rcu_head* head) {
	struct teadfs_inode_info* inode_info;
	struct inode* inode = container_of(head, struct inode, i_rcu);

	LOG_DBG("ENTRY\n");
	inode_info = teadfs_inode_to_private(inode);
	teadfs_free(inode_info);
	teadfs_set_inode_lower(inode, NULL);
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
	//BUG_ON(!inode_info->lower_inode);

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
	int rc;
	struct path lower_path;
	struct dentry* lower_dentry;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_sb->s_op->statfs) {
			rc = -ENOSYS;
			break;
		}
		rc = lower_dentry->d_sb->s_op->statfs(lower_dentry, buf);
		if (rc)
			break;

		buf->f_type = TEADFS_SUPER_MAGIC;
	} while (0);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEVAL rc : [%d]\n", rc);
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
