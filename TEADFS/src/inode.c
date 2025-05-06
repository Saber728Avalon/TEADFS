#include "teadfs_log.h"
#include "teadfs_header.h"
#include "lookup.h"
#include "config.h"
#include "protocol.h"
#include "file.h"
#include "user_com.h"
#include "mem.h"
#include "mmap.h"

#include <linux/fs.h>
#include <linux/fs_stack.h>
#include <linux/mm.h>
#include <linux/xattr.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/namei.h>

static struct dentry* lock_parent(struct dentry* dentry)
{
	struct dentry* dir;

	dir = dget_parent(dentry);
	mutex_lock_nested(&(dir->d_inode->i_mutex), I_MUTEX_PARENT);
	return dir;
}

static void unlock_dir(struct dentry* dir)
{
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}

static int teadfs_create(struct inode* dir, struct dentry* dentry,
	umode_t mode, bool want_excl)
{
	int rc;
	struct dentry* lower_dentry;
	struct dentry* lower_parent_dentry = NULL;
	struct inode* inode;
	int unlock = 0;
	struct path lower_path;

	LOG_DBG("ENTRY file:%s\n", dentry->d_name.name);
	do {
		//get low dentry
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		//lock directory
		lower_parent_dentry = lock_parent(lower_dentry);
		if (IS_ERR(lower_parent_dentry)) {
			LOG_ERR("Error locking directory of "
				"dentry\n");
			inode = ERR_CAST(lower_parent_dentry);
			break;
		}
		unlock = 1;
		//create file or directory
		rc = vfs_create(d_inode(lower_parent_dentry), lower_dentry, mode,
			want_excl);
		if (rc) {
			LOG_ERR("%s: Failure to create dentry in lower fs; "
				"rc = [%d]\n", __func__, rc);
			inode = ERR_PTR(rc);
			break;
		}
		//get file inode
		inode = __teadfs_get_inode(lower_dentry->d_inode, dir->i_sb);
		if (IS_ERR(inode)) {
#if defined(CONFIG_VFS_UNLINK_3_PARAM)
			vfs_unlink(lower_parent_dentry->d_inode, lower_dentry, &inode);
#else
			vfs_unlink(lower_parent_dentry->d_inode, lower_dentry);
#endif
			LOG_ERR("find indoe fail\n");
			break;
		}
		//copy file attr
		fsstack_copy_attr_times(dir, d_inode(lower_parent_dentry));
		fsstack_copy_inode_size(dir, d_inode(lower_parent_dentry));

		unlock_new_inode(inode);
		d_instantiate(dentry, inode);
	} while (0);

	if (unlock) {
		unlock_dir(lower_parent_dentry);
	}
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}




static int teadfs_getattr(struct vfsmount* mnt, struct dentry* dentry,
	struct kstat* stat)
{
	struct kstat lower_stat;
	int rc;
	int access = OFR_INIT;
	struct path lower_path;
	struct dentry* lower_dentry;
	struct teadfs_inode_info*  inode_info = teadfs_inode_to_private(dentry->d_inode);

	LOG_DBG("ENTRY :%s\n", dentry->d_name.name);
	teadfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	rc = vfs_getattr(&lower_path, &lower_stat);
	if (!rc) {
		fsstack_copy_attr_all(dentry->d_inode,
			teadfs_inode_to_lower(dentry->d_inode));
		generic_fillattr(dentry->d_inode, stat);
		stat->blocks = lower_stat.blocks;

		
		if (S_ISREG(stat->mode) && inode_info->file_decrypt) {
			access = teadfs_request_open_path(&lower_path);
			if (OFR_DECRYPT == access) {
				(*stat).size -= ENCRYPT_FILE_HEADER_SIZE;
			}
		}
	}
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}


/**
 * teadfs_setattr
 * @dentry: dentry handle to the inode to modify
 * @ia: Structure with flags of what to change and values
 *
 * Updates the metadata of an inode. If the update is to the size
 * i.e. truncation, then ecryptfs_truncate will handle the size modification
 * of both the ecryptfs inode and the lower inode.
 *
 * All other metadata changes will be passed right to the lower filesystem,
 * and we will just update our inode to look like the lower.
 */
static int teadfs_setattr(struct dentry* dentry, struct iattr* ia)
{
	int rc = 0;
	struct dentry* lower_dentry;
	struct iattr lower_ia;
	struct inode* inode;
	struct inode* lower_inode;
	struct path lower_path;

	LOG_INF("ENTRY :%s\n", dentry->d_name.name);
	do {
		inode = dentry->d_inode;
		lower_inode = teadfs_inode_to_lower(inode);
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;

		rc = inode_change_ok(inode, ia);
		if (rc)
			break;

		memcpy(&lower_ia, ia, sizeof(lower_ia));
		if (ia->ia_valid & ATTR_FILE)
			lower_ia.ia_file = teadfs_file_to_lower(ia->ia_file);
		if (ia->ia_valid & ATTR_SIZE) {
			rc = truncate_upper(dentry, ia, &lower_ia);
			if (rc < 0)
				break;
		}

		/*
		 * mode change is for clearing setuid/setgid bits. Allow lower fs
		 * to interpret this in its own way.
		 */
		if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
			lower_ia.ia_valid &= ~ATTR_MODE;

		mutex_lock(&lower_dentry->d_inode->i_mutex);
		rc = notify_change(lower_dentry, &lower_ia, NULL);
		mutex_unlock(&lower_dentry->d_inode->i_mutex);
	} while (0);
	fsstack_copy_attr_all(inode, lower_inode);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_INF("LEAVE rc = [%d]\n", rc);
	return rc;
}



static int
teadfs_setxattr(struct dentry* dentry, const char* name, const void* value,
	size_t size, int flags)
{
	int rc = 0;
	struct dentry* lower_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_inode->i_op->setxattr) {
			rc = -EOPNOTSUPP;
			break;
		}
		rc = vfs_setxattr(lower_dentry, name, value, size, flags);
		if (!rc && dentry->d_inode)
			fsstack_copy_attr_all(dentry->d_inode, lower_dentry->d_inode);
	} while (0);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}


static int teadfs_readlink_lower(struct dentry* dentry, char** buf,
	size_t* bufsiz)
{
	struct dentry* lower_dentry;
	mm_segment_t old_fs;
	int rc;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		*buf = teadfs_zalloc(PATH_MAX, GFP_KERNEL);
		if (!(*buf)) {
			LOG_ERR("Mem alloc Fail\n");
			break;
		}
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		old_fs = get_fs();
		set_fs(get_ds());
		rc = lower_dentry->d_inode->i_op->readlink(lower_dentry,
			(char __user*)(*buf),
			PATH_MAX);
		set_fs(old_fs);
		if (rc < 0)
			break;
	} while (0);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

int teadfs_readlink(struct dentry* dentry, char __user* buffer, int buflen) {
	struct dentry* lower_dentry;
	struct path lower_path;
	int rc;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		rc = generic_readlink(dentry, buffer, buflen);
	} while (0);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
}


static void* teadfs_follow_link(struct dentry* dentry, struct nameidata* nd)
{
	char* buf;
	size_t len = PATH_MAX;
	int rc;
	struct path lower_path;
	struct dentry* lower_dentry;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		rc = teadfs_readlink_lower(dentry, &buf, &len);
		if (rc)
			break;
		fsstack_copy_attr_atime(dentry->d_inode,
			lower_dentry->d_inode);
		buf[len] = '\0';
	} while (0);
	nd_set_link(nd, buf);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return NULL;
}

static int teadfs_link(struct dentry* old_dentry, struct inode* dir,
	struct dentry* new_dentry)
{
	struct dentry* lower_old_dentry;
	struct dentry* lower_new_dentry;
	struct dentry* lower_dir_dentry;
	u64 file_size_save;
	int rc;
	struct path lower_old_path, lower_new_path;

	LOG_DBG("ENTRY\n");
	do {
		file_size_save = i_size_read(old_dentry->d_inode);

		teadfs_get_lower_path(old_dentry, &lower_old_path);
		teadfs_get_lower_path(new_dentry, &lower_new_path);
		lower_old_dentry = lower_old_path.dentry;
		lower_new_dentry = lower_new_path.dentry;
		dget(lower_old_dentry);
		dget(lower_new_dentry);
		lower_dir_dentry = lock_parent(lower_new_dentry);
		rc = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode,
			lower_new_dentry
#if defined(CONFIG_VFS_LINK_4_PARAM)
			, NULL
#endif	
		);
		if (rc || !lower_new_dentry->d_inode)
			break;
		rc = teadfs_interpose(lower_new_dentry, new_dentry, dir->i_sb);
		if (rc)
			break;
		fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
		fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
		set_nlink(old_dentry->d_inode,
			teadfs_inode_to_lower(old_dentry->d_inode)->i_nlink);
		i_size_write(new_dentry->d_inode, file_size_save);
	} while (0);

	unlock_dir(lower_dir_dentry);
	dput(lower_new_dentry);
	dput(lower_old_dentry);
	teadfs_put_lower_path(old_dentry, &lower_old_path);
	teadfs_put_lower_path(new_dentry, &lower_new_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}




static int teadfs_unlink(struct inode* dir, struct dentry* dentry)
{
	struct dentry* lower_dentry;
	struct inode* lower_dir_inode = teadfs_inode_to_lower(dir);
	struct dentry* lower_dir_dentry;
	int rc;
	struct inode* inode = dentry->d_inode;
	struct path lower_path;

	LOG_INF("ENTRY :%s\n", dentry->d_name.name);
	do {

		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		dget(lower_dentry);
		lower_dir_dentry = lock_parent(lower_dentry);
		rc = vfs_unlink(lower_dir_inode, lower_dentry
#if defined(CONFIG_VFS_UNLINK_4_PARAM)
			,NULL
#endif
			);
		if (rc) {
			printk(KERN_ERR "Error in vfs_unlink; rc = [%d]\n", rc);
			break;
		}
		fsstack_copy_attr_times(dir, lower_dir_inode);
		set_nlink(inode, teadfs_inode_to_lower(inode)->i_nlink);
		inode->i_ctime = dir->i_ctime;
		d_drop(dentry);

	} while (0);

	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

static void
teadfs_put_link(struct dentry* dentry, struct nameidata* nd, void* ptr)
{
	char* buf = nd_get_link(nd);
	LOG_DBG("ENTRY\n");
	if (!IS_ERR(buf)) {
		/* Free the char* */
		teadfs_free(buf);
	}
	LOG_DBG("LEAVE \n");
}

static int
teadfs_permission(struct inode* inode, int mask)
{
	return inode_permission(teadfs_inode_to_lower(inode), mask);
}



static int teadfs_getattr_link(struct vfsmount* mnt, struct dentry* dentry,
	struct kstat* stat)
{
	int rc = 0;

	LOG_DBG("ENTRY\n");
	generic_fillattr(dentry->d_inode, stat);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}



static ssize_t
teadfs_getxattr(struct dentry* dentry, const char* name, void* value,
	size_t size)
{
	int rc = 0;
	struct dentry* lower_dentry;
	struct path lower_path;
	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_inode->i_op->getxattr) {
			rc = -EOPNOTSUPP;
			break;
		}
		mutex_lock(&lower_dentry->d_inode->i_mutex);
		rc = lower_dentry->d_inode->i_op->getxattr(lower_dentry, name, value,
			size);
		mutex_unlock(&lower_dentry->d_inode->i_mutex);
	} while (0);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}



static ssize_t
teadfs_listxattr(struct dentry* dentry, char* list, size_t size)
{
	int rc = 0;
	struct dentry* lower_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_inode->i_op->listxattr) {
			rc = -EOPNOTSUPP;
			break;
		}
		mutex_lock(&lower_dentry->d_inode->i_mutex);
		rc = lower_dentry->d_inode->i_op->listxattr(lower_dentry, list, size);
		mutex_unlock(&lower_dentry->d_inode->i_mutex);
	} while (0);

	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

static int teadfs_removexattr(struct dentry* dentry, const char* name)
{
	int rc = 0;
	struct dentry* lower_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		if (!lower_dentry->d_inode->i_op->removexattr) {
			rc = -EOPNOTSUPP;
			break;
		}
		mutex_lock(&lower_dentry->d_inode->i_mutex);
		rc = lower_dentry->d_inode->i_op->removexattr(lower_dentry, name);
		mutex_unlock(&lower_dentry->d_inode->i_mutex);
	} while (0);

	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

// linux file link
static int teadfs_symlink(struct inode* dir, struct dentry* dentry,
	const char* symname)
{
	int rc;
	struct dentry* lower_dentry;
	struct dentry* lower_dir_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		dget(lower_dentry);
		lower_dir_dentry = lock_parent(lower_dentry);
		rc = vfs_symlink(lower_dir_dentry->d_inode, lower_dentry,
			symname);
		if (rc || !lower_dentry->d_inode)
			break;
		rc = teadfs_interpose(lower_dentry, dentry, dir->i_sb);
		if (rc)
			break;
		fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
		fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	} while (0);

	unlock_dir(lower_dir_dentry);
	dput(lower_dentry);
	if (!dentry->d_inode)
		d_drop(dentry);

	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

static int teadfs_mkdir(struct inode* dir, struct dentry* dentry, umode_t mode)
{
	int rc;
	struct dentry* lower_dentry;
	struct dentry* lower_dir_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		lower_dir_dentry = lock_parent(lower_dentry);
		rc = vfs_mkdir(lower_dir_dentry->d_inode, lower_dentry, mode);
		if (rc || !lower_dentry->d_inode)
			break;
		rc =teadfs_interpose(lower_dentry, dentry, dir->i_sb);
		if (rc)
			break;
		fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
		fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
		set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);
	} while (0);

	unlock_dir(lower_dir_dentry);
	if (!dentry->d_inode)
		d_drop(dentry);

	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

static int teadfs_rmdir(struct inode* dir, struct dentry* dentry)
{
	struct dentry* lower_dentry;
	struct dentry* lower_dir_dentry;
	int rc;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	teadfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;

	dget(dentry);
	lower_dir_dentry = lock_parent(lower_dentry);
	dget(lower_dentry);
	rc = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
	dput(lower_dentry);
	if (!rc && dentry->d_inode)
		clear_nlink(dentry->d_inode);
	fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
	set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);
	unlock_dir(lower_dir_dentry);
	if (!rc)
		d_drop(dentry);
	dput(dentry);
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

static int
teadfs_mknod(struct inode* dir, struct dentry* dentry, umode_t mode, dev_t dev)
{
	int rc;
	struct dentry* lower_dentry;
	struct dentry* lower_dir_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	do {
		teadfs_get_lower_path(dentry, &lower_path);
		lower_dentry = lower_path.dentry;
		lower_dir_dentry = lock_parent(lower_dentry);
		rc = vfs_mknod(lower_dir_dentry->d_inode, lower_dentry, mode, dev);
		if (rc || !lower_dentry->d_inode)
			break;
		rc = teadfs_interpose(lower_dentry, dentry, dir->i_sb);
		if (rc)
			break;
		fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
		fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
	} while (0);

	unlock_dir(lower_dir_dentry);
	if (!dentry->d_inode)
		d_drop(dentry);

	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

static int
teadfs_rename(struct inode* old_dir, struct dentry* old_dentry,
	struct inode* new_dir, struct dentry* new_dentry)
{
	int rc;
	struct dentry* lower_old_dentry;
	struct dentry* lower_new_dentry;
	struct dentry* lower_old_dir_dentry;
	struct dentry* lower_new_dir_dentry;
	struct dentry* trap = NULL;
	struct inode* target_inode;
	int is_rename_lock = 0;
	struct path lower_old_path, lower_new_path;

	LOG_DBG("ENTRY\n");
	do {
		LOG_INF("rename: %s --> %s\n", old_dentry->d_name.name, new_dentry->d_name.name)
		teadfs_get_lower_path(old_dentry, &lower_old_path);
		lower_old_dentry = lower_old_path.dentry;
		teadfs_get_lower_path(new_dentry, &lower_new_path);
		lower_new_dentry = lower_new_path.dentry;
		dget(lower_old_dentry);
		dget(lower_new_dentry);
		lower_old_dir_dentry = dget_parent(lower_old_dentry);
		lower_new_dir_dentry = dget_parent(lower_new_dentry);
		target_inode = d_inode(new_dentry);
		trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
		/* source should not be ancestor of target */
		if (trap == lower_old_dentry) {
			rc = -EINVAL;
			break;
		}
		/* target should not be ancestor of source */
		if (trap == lower_new_dentry) {
			rc = -ENOTEMPTY;
			break;
		}
		is_rename_lock = 1;
		rc = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
			lower_new_dir_dentry->d_inode, lower_new_dentry
#if defined(CONFIG_VFS_RENAME_6_PARAM)
			, NULL
			, 0
#endif
		);
		if (rc)
			break;
		if (target_inode)
			fsstack_copy_attr_all(target_inode,
				teadfs_inode_to_lower(target_inode));
		fsstack_copy_attr_all(new_dir, lower_new_dir_dentry->d_inode);
		if (new_dir != old_dir)
			fsstack_copy_attr_all(old_dir, lower_old_dir_dentry->d_inode);

	} while (0);

	if (is_rename_lock) {
		unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
	}
	dput(lower_new_dir_dentry);
	dput(lower_old_dir_dentry);
	dput(lower_new_dentry);
	dput(lower_old_dentry);

	teadfs_put_lower_path(old_dentry, &lower_old_path);
	teadfs_put_lower_path(new_dentry, &lower_new_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}



const struct inode_operations teadfs_symlink_iops = {
	.readlink		= teadfs_readlink,
	.follow_link	= teadfs_follow_link,
	.put_link		= teadfs_put_link,
	.permission		= teadfs_permission,
	.setattr		= teadfs_setattr,
	.getattr		= teadfs_getattr_link,
	.setxattr		= teadfs_setxattr,
	.getxattr		= teadfs_getxattr,
	.listxattr		= teadfs_listxattr,
	.removexattr	= teadfs_removexattr
};

const struct inode_operations teadfs_dir_iops = {
	.create			= teadfs_create,
	.lookup			= teadfs_lookup,
	.link			= teadfs_link,
	.unlink			= teadfs_unlink,
	.symlink		= teadfs_symlink,
	.mkdir			= teadfs_mkdir,
	.rmdir			= teadfs_rmdir,
	.mknod			= teadfs_mknod,
	.rename			= teadfs_rename,
	.permission		= teadfs_permission,
	.setattr		= teadfs_setattr,
	.setxattr		= teadfs_setxattr,
	.getxattr		= teadfs_getxattr,
	.listxattr		= teadfs_listxattr,
	.removexattr	= teadfs_removexattr
};

const struct inode_operations teadfs_main_iops = {
	.permission		= teadfs_permission,
	.setattr		= teadfs_setattr,
	.getattr		= teadfs_getattr,
	.setxattr		= teadfs_setxattr,
	.getxattr		= teadfs_getxattr,
	.listxattr		= teadfs_listxattr,
	.removexattr	= teadfs_removexattr
};


