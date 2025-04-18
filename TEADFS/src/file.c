/**
 * eCryptfs: Linux filesystem encryption layer
 *
 * Copyright (C) 1997-2004 Erez Zadok
 * Copyright (C) 2001-2004 Stony Brook University
 * Copyright (C) 2004-2007 International Business Machines Corp.
 *   Author(s): Michael A. Halcrow <mhalcrow@us.ibm.com>
 *   		Michael C. Thompson <mcthomps@us.ibm.com>
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

#include "file.h"
#include "teadfs_header.h"
#include "inode.h"
#include "mem.h"
#include "teadfs_log.h"
#include "config.h"

#include <linux/file.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/pagemap.h>
#include <linux/security.h>
#include <linux/compat.h>
#include <linux/fs_stack.h>
#include <linux/aio.h>


/**
 * teadfs_read_update_atime
 * 
 *
 * generic_file_read updates the atime of upper layer inode.  But, it
 * doesn't give us a chance to update the atime of the lower layer
 * inode.  This function is a wrapper to generic_file_read.  It
 * updates the atime of the lower level inode if generic_file_read
 * returns without any errors. This is to be used only for file reads.
 * The function to be used for directory reads is ecryptfs_read.
 */
static ssize_t teadfs_read_update_atime(struct kiocb *iocb,
				const struct iovec *iov,
				unsigned long nr_segs, loff_t pos)
{
	ssize_t rc;
	struct path lower;
	struct file *file = iocb->ki_filp;

	LOG_DBG("ENTRY\n");
	do {
		rc = generic_file_aio_read(iocb, iov, nr_segs, pos);
		/*
		 * Even though this is a async interface, we need to wait
		 * for IO to finish to update atime
		 */
		if (-EIOCBQUEUED == rc)
			rc = wait_on_sync_kiocb(iocb);
		if (rc >= 0) {
			lower.dentry = teadfs_dentry_to_lower(file->f_path.dentry);
			lower.mnt = teadfs_dentry_to_lower_path(file->f_path.dentry)->mnt;
			touch_atime(&lower);
		}
	} while (0);
	
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

static ssize_t teadfs_write(struct kiocb* iocb,
	const struct iovec* iov,
	unsigned long nr_segs, loff_t pos)
{
	ssize_t rc;
	struct path lower;
	struct file* file = iocb->ki_filp;

	LOG_DBG("ENTRY\n");
	do {
		rc = generic_file_aio_write(iocb, iov, nr_segs, pos);
		/*
		 * Even though this is a async interface, we need to wait
		 * for IO to finish to update atime
		 */
		if (-EIOCBQUEUED == rc)
			rc = wait_on_sync_kiocb(iocb);
	} while (0);

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

struct teadfs_getdents_callback {
	void *dirent;
	struct dentry *dentry;
	filldir_t filldir;
	int filldir_called;
	int entries_written;
};

/* Inspired by generic filldir in fs/readdir.c */
static int
teadfs_filldir(void *dirent, const char *lower_name, int lower_namelen,
		 loff_t offset, u64 ino, unsigned int d_type)
{
	struct teadfs_getdents_callback *buf =
	    (struct teadfs_getdents_callback *)dirent;
	size_t name_size;
	char *name;
	int rc;

	LOG_DBG("ENTRY\n");
	buf->filldir_called++;
	rc = buf->filldir(buf->dirent, name, name_size, offset, ino, d_type);
	kfree(name);
	if (rc >= 0)
		buf->entries_written++;

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

/**
 * ecryptfs_readdir
 * @file: The eCryptfs directory file
 * @dirent: Directory entry handle
 * @filldir: The filldir callback function
 */
#if defined(CONFIG_ITERATE_DIR)
	static int teadfs_readdir(struct file* file, struct dir_context* ctx)
#else
	static int teadfs_readdir(struct file* file, void* dirent,
		filldir_t filldir)
#endif
{
	int rc;
	struct file *lower_file;
	struct inode *inode;
	struct teadfs_getdents_callback buf;

	LOG_DBG("ENTRY\n");
	do {
		lower_file = teadfs_file_to_lower(file);
		lower_file->f_pos = file->f_pos;
		inode = file_inode(file);
#if defined(CONFIG_ITERATE_DIR)
		rc = iterate_dir(lower_file, ctx);
#else
		memset(&buf, 0, sizeof(buf));
		buf.dirent = dirent;
		buf.dentry = file->f_path.dentry;
		buf.filldir = filldir;
		buf.filldir_called = 0;
		buf.entries_written = 0;
		rc = vfs_readdir(lower_file, teadfs_filldir, (void*)&buf);
#endif
		file->f_pos = lower_file->f_pos;
		if (rc < 0)
			break;
		if (buf.filldir_called && !buf.entries_written)
			break;
		if (rc >= 0)
			fsstack_copy_attr_atime(inode,
				file_inode(lower_file));
	} while (0);
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}


struct teadfs_open_req {
	struct file** lower_file;
	struct path path;
	struct completion done;
	struct list_head kthread_ctl_list;
};
static struct teadfs_kthread_ctl {
#define ECRYPTFS_KTHREAD_ZOMBIE 0x00000001
	u32 flags;
	struct mutex mux;
	struct list_head req_list;
	wait_queue_head_t wait;
} teadfs_kthread_ctl;
/**
 * teadfs_privileged_open
 * @lower_file: Result of dentry_open by root on lower dentry
 * @lower_dentry: Lower dentry for file to open
 * @lower_mnt: Lower vfsmount for file to open
 *
 * This function gets a r/w file opened againt the lower dentry.
 *
 * Returns zero on success; non-zero otherwise
 */
int teadfs_privileged_open(struct file** lower_file,
	struct path *lower_path,
	const struct cred* cred)
{
	struct teadfs_open_req req;
	int flags = O_LARGEFILE;
	int rc = 0;
	struct dentry* lower_dentry = lower_path->dentry;

	LOG_DBG("ENTRY\n");
	do {
		init_completion(&req.done);
		req.lower_file = lower_file;
		req.path.dentry = lower_path->dentry;
		req.path.mnt = lower_path->mnt;
		/* Corresponding dput() and mntput() are done when the
		 * lower file is fput() when all eCryptfs files for the inode are
		 * released. */
		flags |= IS_RDONLY(lower_dentry->d_inode) ? O_RDONLY : O_RDWR;
		(*lower_file) = dentry_open(&req.path, flags, cred);
		if (!IS_ERR(*lower_file))
			break;
		if ((flags & O_ACCMODE) == O_RDONLY) {
			rc = PTR_ERR((*lower_file));
			break;
		}
		mutex_lock(&teadfs_kthread_ctl.mux);
		list_add_tail(&req.kthread_ctl_list, &teadfs_kthread_ctl.req_list);
		mutex_unlock(&teadfs_kthread_ctl.mux);
		wake_up(&teadfs_kthread_ctl.wait);
		wait_for_completion(&req.done);
		if (IS_ERR(*lower_file))
			rc = PTR_ERR(*lower_file);
	} while (0);
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}


/**
 * teadfs_open
 * @inode: inode speciying file to open
 * @file: Structure to return filled in
 *
 * Opens the file specified by inode.
 *
 * Returns zero on success; non-zero otherwise
 */
static int teadfs_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct dentry *teadfs_dentry = file->f_path.dentry;
	/* Private value of ecryptfs_dentry allocated in
	 * ecryptfs_lookup() */
	struct teadfs_file_info *file_info;
	struct path *lower_path = teadfs_dentry_to_lower_path(teadfs_dentry);
	int flags = O_LARGEFILE;

	LOG_DBG("ENTRY file:%px name:%s\n", file, teadfs_dentry->d_name.name);
	do {
		/* Released in ecryptfs_release or end of function if failure */
		file_info = teadfs_zalloc(sizeof(struct teadfs_file_info), GFP_KERNEL);
		teadfs_set_file_private(file, file_info);
		if (!file_info) {
			LOG_ERR("Error attempting to allocate memory\n");
			rc = -ENOMEM;
			break;
		}
		/* open lower object and link wrapfs's file struct to lower's */
		LOG_ERR("dentry:%px mnt:%px   lower_path:%px\n", lower_path->dentry, lower_path->mnt, lower_path);
		//check file flag
		flags |= file->f_flags;
		//only write will not support in mmap to read file. so add read
		LOG_ERR("flags:%x\n", flags);
		LOG_ERR("flags1:%x O_WRONLY:%x\n", flags & O_ACCMODE, O_WRONLY);
		if ((flags & O_ACCMODE) == O_WRONLY) {
			flags ^= O_WRONLY;
			LOG_ERR("flags:%x\n", flags);
			flags |= O_RDWR;
		}
		LOG_ERR("flags:%x\n", flags);
		file_info->lower_file = dentry_open(lower_path, flags, current_cred());
		if (IS_ERR(file_info->lower_file)) {
			rc = PTR_ERR(file_info->lower_file);
			file_info->lower_file = teadfs_file_to_lower(file);
			if (file_info->lower_file) {
				fput(file_info->lower_file); /* fput calls dput for lower_dentry */
			}
		}
		LOG_ERR("lower_file:%px\n", file_info->lower_file);
		rc = 0;
	} while (0);
	//release memory
	if (rc) {
		LOG_ERR("Open File Error, Code:%d\n", rc);
		teadfs_free(teadfs_file_to_private(file));
		teadfs_set_file_private(file, NULL);
	}
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

static int teadfs_flush(struct file *file, fl_owner_t td)
{
	struct file *lower_file = teadfs_file_to_lower(file);
	int rc = 0;
	LOG_DBG("ENTRY\n");

	if (lower_file->f_op && lower_file->f_op->flush) {
		filemap_write_and_wait(file->f_mapping);
		rc = lower_file->f_op->flush(lower_file, td);
	}
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

static int teadfs_release(struct inode *inode, struct file *file)
{
	struct teadfs_file_info* file_info = teadfs_file_to_private(file);
	LOG_DBG("ENTRY file:%px\n", file);
	if (file_info->lower_file) {
		fput(file_info->lower_file);

	}
	teadfs_free(file_info);
	LOG_DBG("LEVAL\n");
	return 0;
}

static int
teadfs_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	int rc;

	rc = filemap_write_and_wait(file->f_mapping);
	if (rc)
		return rc;

	return vfs_fsync(teadfs_file_to_lower(file), datasync);
}

static int teadfs_fasync(int fd, struct file *file, int flag)
{
	int rc = 0;
	struct file *lower_file = NULL;

	LOG_DBG("ENTRY\n");
	lower_file = teadfs_file_to_lower(file);
	if (lower_file->f_op && lower_file->f_op->fasync)
		rc = lower_file->f_op->fasync(fd, lower_file, flag);

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

static long
teadfs_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file *lower_file = NULL;
	long rc = -ENOTTY;

	LOG_DBG("ENTRY\n");

	if (teadfs_file_to_private(file))
		lower_file = teadfs_file_to_lower(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->unlocked_ioctl)
		rc = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

#ifdef CONFIG_COMPAT
static long
teadfs_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file *lower_file = NULL;
	long rc = -ENOIOCTLCMD;

	LOG_DBG("ENTRY\n");
	if (teadfs_file_to_private(file))
		lower_file = teadfs_file_to_lower(file);
	if (lower_file && lower_file->f_op && lower_file->f_op->compat_ioctl)
		rc = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}
#endif

const struct file_operations teadfs_dir_fops = {
#if defined(CONFIG_ITERATE_DIR)
	.iterate = teadfs_readdir,
#else
	.readdir = teadfs_readdir,
#endif
	.read = generic_read_dir,
	.unlocked_ioctl = teadfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = teadfs_compat_ioctl,
#endif
	.open = teadfs_open,
	.flush = teadfs_flush,
	.release = teadfs_release,
	.fsync = teadfs_fsync,
	.fasync = teadfs_fasync,
	.splice_read = generic_file_splice_read,
	.llseek = default_llseek,
};

const struct file_operations teadfs_main_fops = {
	.llseek = generic_file_llseek,
	.read = do_sync_read,
	.aio_read = teadfs_read_update_atime,
	.write = do_sync_write,
	.aio_write = teadfs_write,
	.readdir = teadfs_readdir,
	.unlocked_ioctl = teadfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = teadfs_compat_ioctl,
#endif
	.mmap = generic_file_mmap,
	.open = teadfs_open,
	.flush = teadfs_flush,
	.release = teadfs_release,
	.fsync = teadfs_fsync,
	.fasync = teadfs_fasync,
	.splice_read = generic_file_splice_read,
};
