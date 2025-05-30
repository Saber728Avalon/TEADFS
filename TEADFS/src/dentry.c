/**
 * eCryptfs: Linux filesystem encryption layer
 *
 * Copyright (C) 1997-2003 Erez Zadok
 * Copyright (C) 2001-2003 Stony Brook University
 * Copyright (C) 2004-2006 International Business Machines Corp.
 *   Author(s): Michael A. Halcrow <mahalcro@us.ibm.com>
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


#include "teadfs_header.h"
#include "teadfs_log.h"
#include "teadfs_header.h"
#include "mem.h"

#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/fs_stack.h>
#include <linux/slab.h>


/**
 * teads_d_revalidate - revalidate an ecryptfs dentry
 * @dentry: The ecryptfs dentry
 * @flags: lookup flags
 *
 * Called when the VFS needs to revalidate a dentry. This
 * is called whenever a name lookup finds a dentry in the
 * dcache. Most filesystems leave this as NULL, because all their
 * dentries in the dcache are valid.
 *
 * Returns 1 if valid, 0 otherwise.
 *
 */
static int teadfs_d_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct dentry *lower_dentry = NULL;
	int rc = 1;
	struct path lower_path;

	do {
		if (flags & LOOKUP_RCU) {
			rc = -ECHILD;
			break;
		}
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_op || !lower_dentry->d_op->d_revalidate)
			break;

		LOG_DBG("ENTRY\n");
		rc = lower_dentry->d_op->d_revalidate(lower_dentry, flags);
		if (dentry->d_inode) {
			struct inode* lower_inode =
				teadfs_inode_to_lower(dentry->d_inode);

			fsstack_copy_attr_all(dentry->d_inode, lower_inode);
		}
		if (lower_dentry) {
			teadfs_put_lower_path(dentry, &lower_path);
		}
		LOG_DBG("LEVAL rc : [%d]\n", rc);
	} while (0);
	return rc;
}


/**
 * teadfs_d_release
 * @dentry: The ecryptfs dentry
 *
 * Called when a dentry is really deallocated.
 */
static void teadfs_d_release(struct dentry *dentry)
{
	struct path lower_path;
	struct dentry* lower_dentry;

	LOG_DBG("ENTRY name:%s\n", dentry->d_name.name);
	if (teadfs_dentry_to_private(dentry)) {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		dput(lower_path.dentry);
		mntput(lower_path.mnt);
		teadfs_put_lower_path(dentry, &lower_path);

		teadfs_free(teadfs_dentry_to_private(dentry));
		teadfs_set_dentry_private(dentry, NULL);
	}
	LOG_DBG("LEVAL\n");
	return;
}

const struct dentry_operations teadfs_dops = {
	.d_revalidate = teadfs_d_revalidate,
	.d_release = teadfs_d_release,
};
