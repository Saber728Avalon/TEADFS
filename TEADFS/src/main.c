#include "teadfs_log.h"
#include "mem.h"
#include "teadfs_header.h"
#include "lookup.h"
#include "dentry.h"
#include "super.h"
#include "netlink.h"
#include "global_param.h"
#include "miscdev.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/string.h>



/**
 * teadfs_mount
 * @fs_type
 * @flags
 * @dev_name: The path to mount over
 * @raw_data: The options passed into the kernel
 */
static struct dentry* teadfs_mount(struct file_system_type* fs_type, int flags,
	const char* dev_name, void* raw_data)
{
	struct super_block* s = NULL;
	struct teadfs_sb_info* sbi = NULL;
	struct teadfs_dentry_info* root_info;
	const char* err = "Getting sb failed";
	struct inode* inode;
	struct path lower_path;
	int rc;
	int release_path = 0;

	LOG_DBG("ENTRY\n");
	do {
		sbi = teadfs_zalloc(sizeof(struct teadfs_sb_info), GFP_KERNEL);
		if (!sbi) {
			rc = -ENOMEM;
			break;
		}
		s = sget(fs_type, NULL, set_anon_super, flags, NULL);
		if (IS_ERR(s)) {
			rc = PTR_ERR(s);
			LOG_ERR("sget() failed\n");
			break;
		}
#if defined(CONFIG_BDICONFIG_BDI)
		LOG_DBG("ENTRY\n");
		rc = bdi_setup_and_register(&sbi->bdi, "teadfs", BDI_CAP_MAP_COPY);
		if (rc) {
			LOG_ERR("bdi_setup_and_register() failed\n");
			break;
		}
		s->s_bdi = &sbi->bdi;
#endif
		teadfs_set_lower_super(s, sbi);
		/* ->kill_sb() will take care of sbi after that point */
		sbi = NULL;
		s->s_op = &teadfs_sops;
		s->s_d_op = &teadfs_dops;

		err = "Reading sb failed";
		rc = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &lower_path);
		if (rc) {
			LOG_ERR("kern_path() failed\n");
			break;
		}
		release_path = 1;
		//double mount is error
		if (0 == strcmp(lower_path.dentry->d_sb->s_type->name, "teadfs")) {
			rc = -EINVAL;
			LOG_ERR("Mount on filesystem of type "
				"eCryptfs explicitly disallowed due to "
				"known incompatibilities\n");
			break;
		}

		teadfs_set_superblock_lower(s, lower_path.dentry->d_sb);
		/**
		 * Set the POSIX ACL flag based on whether they're enabled in the lower
		 * mount.
		 */
		s->s_flags = flags & ~MS_POSIXACL;
		s->s_flags |= lower_path.dentry->d_sb->s_flags & MS_POSIXACL;

		/**
		 * Force a read-only eCryptfs mount when:
		 *   1) The lower mount is ro
		 *   2) The ecryptfs_encrypted_view mount option is specified
		 */
		if (lower_path.dentry->d_sb->s_flags & MS_RDONLY)
			s->s_flags |= MS_RDONLY;

		s->s_maxbytes = lower_path.dentry->d_sb->s_maxbytes;
		s->s_blocksize = lower_path.dentry->d_sb->s_blocksize;
		s->s_magic = TEADFS_SUPER_MAGIC;

		inode = teadfs_get_inode(lower_path.dentry->d_inode, s);
		rc = PTR_ERR(inode);
		if (IS_ERR(inode))
			break;

		s->s_root = d_make_root(inode);
		if (!s->s_root) {
			rc = -ENOMEM;
			LOG_ERR("d_make_root() failed\n");
			break;
		}

		rc = -ENOMEM;
		root_info = teadfs_zalloc(sizeof(struct teadfs_dentry_info), GFP_KERNEL);
		if (!root_info)
			break;
		spin_lock_init(&(root_info->lock));
		/* ->kill_sb() will take care of root_info */
		teadfs_set_dentry_private(s->s_root, root_info);
		teadfs_set_lower_path(s->s_root, &lower_path);
		LOG_ERR("dentry:%px mnt:%px\n", lower_path.dentry, lower_path.mnt);
		s->s_flags |= MS_ACTIVE;
		LOG_INF("Mount success\n");
		return dget(s->s_root);
	} while (0);

	if (release_path) {
		path_put(&lower_path);
	}
	if (s) {
		deactivate_locked_super(s);
	}
	if (sbi) {
		teadfs_free(sbi);
	}
	LOG_ERR("%s; rc = [%d]\n", err, rc);
	return ERR_PTR(rc);
}


/**
 * teadfs_kill_block_super
 * @sb: The ecryptfs super block
 *
 * Used to bring the superblock down and free the private data.
 */
static void teadfs_kill_block_super(struct super_block* sb)
{
	struct teadfs_sb_info* sb_info = teadfs_get_super_block(sb);
	
	LOG_DBG("ENTRY\n");
	do {
		kill_anon_super(sb);
		if (!sb_info)
			break;
#if defined(CONFIG_BDICONFIG_BDI)
		bdi_destroy(&sb_info->bdi);
#endif
		teadfs_set_superblock_lower(sb, NULL);
		teadfs_free(sb_info);
	} while (0);
	LOG_DBG("LEVAL\n");
	return;
}

static struct file_system_type teadfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "teadfs",
    .mount = teadfs_mount,
    .kill_sb = teadfs_kill_block_super,
    .fs_flags = 0
};
MODULE_ALIAS_FS("teadfs");

static int __init teadfs_module_init(void) {
    int rc;

	teadfs_log_create();

    LOG_DBG("ENTRY\n");
    do {
        rc = register_filesystem(&teadfs_fs_type);
        if (rc) {
            LOG_ERR("Failed to register filesystem\n");
            break;
        }
		//init param
		teadfs_init_global_param();

		//create netlink
		teadfs_start_netlink();

		// check client is connect ?
		teadfs_init_miscdev();

		
    } while (0);
    LOG_DBG("LEVAL\n");
    return 0;
}
 
static void __exit teadfs_module_exit(void) {
    LOG_DBG("ENTRY\n");
    unregister_filesystem(&teadfs_fs_type);

	teadfs_release_netlink();

	teadfs_destroy_miscdev();
    LOG_DBG("LEVAL\n");

	teadfs_log_release();
}


 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TEADFS");
MODULE_DESCRIPTION("Transport Encrypt And Decrypt File");
MODULE_VERSION("0.1");
MODULE_ALIAS("teadfs");

module_init(teadfs_module_init);
module_exit(teadfs_module_exit);