#include "teadfs_log.h"
#include "teadfs_header.h"
#include "lookup.h"

#include <linux/fs.h>
#include <linux/fs_stack.h>
#include <linux/mm.h>
#include <linux/xattr.h>
#include <linux/module.h>

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

static int wrapfs_create(struct inode* dir, struct dentry* dentry,
	umode_t mode, bool want_excl)
{
	int rc;
	struct dentry* lower_dentry;
	struct dentry* lower_parent_dentry = NULL;
	struct inode* inode;

	LOG_DBG("ENTRY\n");
	do {
		//get low dentry
		lower_dentry = teadfs_dentry_to_lower(dentry);

		//lock directory
		lower_parent_dentry = lock_parent(lower_dentry);
		if (IS_ERR(lower_parent_dentry)) {
			LOG_ERR("Error locking directory of "
				"dentry\n");
			inode = ERR_CAST(lower_parent_dentry);
			break;
		}
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
	} while (0);

	unlock_dir(lower_parent_dentry);
	LOG_DBG("rc = [%d]\n", rc);
	return rc;
}




static int teadfs_getattr(struct vfsmount* mnt, struct dentry* dentry,
	struct kstat* stat)
{
	struct kstat lower_stat;
	int rc;

	LOG_DBG("ENTRY\n");
	rc = vfs_getattr(teadfs_dentry_to_lower_path(dentry), &lower_stat);
	if (!rc) {
		fsstack_copy_attr_all(dentry->d_inode,
			teadfs_inode_to_lower(dentry->d_inode));
		generic_fillattr(dentry->d_inode, stat);
		stat->blocks = lower_stat.blocks;
	}
	LOG_DBG("rc = [%d]\n", rc);
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

	LOG_DBG("ENTRY\n");
	do {

		inode = dentry->d_inode;
		lower_inode = teadfs_inode_to_lower(inode);
		lower_dentry = teadfs_dentry_to_lower(dentry);

		rc = inode_change_ok(inode, ia);
		if (rc)
			break;

		memcpy(&lower_ia, ia, sizeof(lower_ia));
		if (ia->ia_valid & ATTR_FILE)
			lower_ia.ia_file = teadfs_file_to_lower(ia->ia_file);
		if (ia->ia_valid & ATTR_SIZE) {
			truncate_setsize(inode, ia->ia_size);
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
	LOG_DBG("rc = [%d]\n", rc);
	return rc;
}



static int
teadfs_setxattr(struct dentry* dentry, const char* name, const void* value,
	size_t size, int flags)
{
	int rc = 0;
	struct dentry* lower_dentry;

	LOG_DBG("ENTRY\n");
	do {

		lower_dentry = teadfs_dentry_to_lower(dentry);
		if (!lower_dentry->d_inode->i_op->setxattr) {
			rc = -EOPNOTSUPP;
			break;
		}
		rc = vfs_setxattr(lower_dentry, name, value, size, flags);
		if (!rc && dentry->d_inode)
			fsstack_copy_attr_all(dentry->d_inode, lower_dentry->d_inode);
	} while (0);
	LOG_DBG("rc = [%d]\n", rc);
	return rc;
}








const struct inode_operations teadfs_symlink_iops = {
	//.readlink = generic_readlink,
	//.follow_link = ecryptfs_follow_link,
	//.put_link = ecryptfs_put_link,
	//.permission = ecryptfs_permission,
	.setattr = teadfs_setattr,
	//.getattr = ecryptfs_getattr_link,
	.setxattr = teadfs_setxattr,
	//.getxattr = ecryptfs_getxattr,
	//.listxattr = ecryptfs_listxattr,
	//.removexattr = ecryptfs_removexattr
};

const struct inode_operations teadfs_dir_iops = {
	//.create = teadfs_create,
	.lookup = teadfs_lookup,
	//.link = ecryptfs_link,
	//.unlink = ecryptfs_unlink,
	//.symlink = ecryptfs_symlink,
	//.mkdir = ecryptfs_mkdir,
	//.rmdir = ecryptfs_rmdir,
	//.mknod = ecryptfs_mknod,
	//.rename = ecryptfs_rename,
	//.permission = ecryptfs_permission,
	.setattr = teadfs_setattr,
	.setxattr = teadfs_setxattr,
	//.getxattr = ecryptfs_getxattr,
	//.listxattr = ecryptfs_listxattr,
	//.removexattr = ecryptfs_removexattr
};

const struct inode_operations teadfs_main_iops = {
	//.permission = ecryptfs_permission,
	.setattr = teadfs_setattr,
	.getattr = teadfs_getattr,
	.setxattr = teadfs_setxattr,
	//.getxattr = ecryptfs_getxattr,
	//.listxattr = ecryptfs_listxattr,
	//.removexattr = ecryptfs_removexattr
};
