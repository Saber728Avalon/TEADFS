#include "teadfs_log.h"
#include "mem.h"
#include "teadfs_header.h"
#include "lookup.h"
#include "dentry.h"
#include "super.h"

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
	struct path path;
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
			break;
		}
#if defined(CONFIG_BDICONFIG_BDI)
		rc = bdi_setup_and_register(&sbi->bdi, "teadfs", BDI_CAP_MAP_COPY);
#endif
		if (rc)
			break;

		teadfs_set_lower_super(sbi, s);
		s->s_bdi = &sbi->bdi;

		/* ->kill_sb() will take care of sbi after that point */
		sbi = NULL;
		s->s_op = &teadfs_sops;
		s->s_d_op = &teadfs_dops;

		err = "Reading sb failed";
		rc = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY, &path);
		if (rc) {
			LOG_ERR("kern_path() failed\n");
			break;
		}
		release_path = 1;
		//double mount is error
		if (0 == strcmp(path.dentry->d_sb->s_type->name, "teadfs")) {
			rc = -EINVAL;
			LOG_ERR("Mount on filesystem of type "
				"eCryptfs explicitly disallowed due to "
				"known incompatibilities\n");
			break;
		}

		teadfs_set_superblock_lower(s, path.dentry->d_sb);

		/**
		 * Set the POSIX ACL flag based on whether they're enabled in the lower
		 * mount.
		 */
		s->s_flags = flags & ~MS_POSIXACL;
		s->s_flags |= path.dentry->d_sb->s_flags & MS_POSIXACL;

		/**
		 * Force a read-only eCryptfs mount when:
		 *   1) The lower mount is ro
		 *   2) The ecryptfs_encrypted_view mount option is specified
		 */
		if (path.dentry->d_sb->s_flags & MS_RDONLY)
			s->s_flags |= MS_RDONLY;

		s->s_maxbytes = path.dentry->d_sb->s_maxbytes;
		s->s_blocksize = path.dentry->d_sb->s_blocksize;
		s->s_magic = ECRYPTFS_SUPER_MAGIC;

		inode = teadfs_get_inode(path.dentry->d_inode, s);
		rc = PTR_ERR(inode);
		if (IS_ERR(inode))
			break;

		s->s_root = d_make_root(inode);
		if (!s->s_root) {
			rc = -ENOMEM;
			break;
		}

		rc = -ENOMEM;
		root_info = teadfs_zalloc(sizeof(struct teadfs_dentry_info), GFP_KERNEL);
		if (!root_info)
			break;

		/* ->kill_sb() will take care of root_info */
		teadfs_set_dentry_private(s->s_root, root_info);
		teadfs_get_lower_path(s->s_root, &path);

		s->s_flags |= MS_ACTIVE;
		LOG_DBG("Mount success\n");
		return dget(s->s_root);
	} while (0);

	if (release_path) {
		path_put(&path);
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


static struct file_system_type teadfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "teadfs",
    .mount = teadfs_mount,
    .kill_sb = generic_shutdown_super,
    .fs_flags = 0
};
MODULE_ALIAS_FS("teadfs");

static int __init teadfs_module_init(void) {
    int rc;
	printk(KERN_INFO "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    LOG_DBG("ENTRY\n");
	printk(KERN_INFO "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    do {
        rc = register_filesystem(&teadfs_fs_type);
        if (rc) {
            LOG_ERR("Failed to register filesystem\n");
            break;
        }
    } while (0);
    LOG_DBG("LEVAL\n");
	printk(KERN_INFO "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    return 0;
}
 
static void __exit teadfs_module_exit(void) {
	printk(KERN_INFO "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
    LOG_DBG("ENTRY\n");
    unregister_filesystem(&teadfs_fs_type);
    LOG_DBG("LEVAL\n");
	printk(KERN_INFO "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
}


 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TEADFS");
MODULE_DESCRIPTION("Transport Encrypt And Decrypt File");
MODULE_VERSION("0.1");
MODULE_ALIAS("teadfs");

module_init(teadfs_module_init);
module_exit(teadfs_module_exit);