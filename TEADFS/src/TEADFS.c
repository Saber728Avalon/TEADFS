#include "teadfs_log.h"
#include "mem.h"
#include "teadfs_header.h"
#include "lookup.h"


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/namei.h>

/*
 * There is no need to lock the wrapfs_super_info's rwsem as there is no
 * way anyone can have a reference to the superblock at this point in time.
 */
static int teadfs_read_super(struct super_block* sb, void* raw_data, int silent)
{
	int err = 0;
	struct super_block* lower_sb;
	struct path lower_path;
	char* dev_name = (char*)raw_data;
	struct inode* inode;

	if (!dev_name) {
		LOG_ERR("wrapfs: read_super: missing dev_name argument\n");
		err = -EINVAL;
		goto out;
	}

	/* parse lower path */
	err = kern_path(dev_name, LOOKUP_FOLLOW | LOOKUP_DIRECTORY,
		&lower_path);
	if (err) {
		LOG_ERR(""wrapfs: error accessing "
			"lower directory '%s'\n", dev_name);
		goto out;
	}

	/* allocate superblock private data */
	sb->s_fs_info = teadfs_zalloc(sizeof(struct teadfs_sb_info), GFP_KERNEL);
	if (!teadfs_get_super_block(sb)) {
		printk(KERN_CRIT "wrapfs: read_super: out of memory\n");
		err = -ENOMEM;
		goto out_free;
	}

	/* set the lower superblock field of upper superblock */
	lower_sb = lower_path.dentry->d_sb;
	atomic_inc(&lower_sb->s_active);
	teadfs_set_lower_super(sb, lower_sb);

	/* inherit maxbytes from lower file system */
	sb->s_maxbytes = lower_sb->s_maxbytes;

	/*
	 * Our c/m/atime granularity is 1 ns because we may stack on file
	 * systems whose granularity is as good.
	 */
	sb->s_time_gran = 1;

	sb->s_op = &wrapfs_sops;
	sb->s_xattr = wrapfs_xattr_handlers;

	sb->s_export_op = &wrapfs_export_ops; /* adding NFS support */

	/* get a new inode and allocate our root dentry */
	inode = teadfs_get_inode(d_inode(lower_path.dentry, sb);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_sput;
	}
	sb->s_root = d_make_root(inode);
	if (!sb->s_root) {
		err = -ENOMEM;
		goto out_iput;
	}
	d_set_d_op(sb->s_root, &wrapfs_dops);

	/* link the upper and lower dentries */
	sb->s_root->d_fsdata = NULL;
	err = new_dentry_private_data(sb->s_root);
	if (err)
		goto out_freeroot;

	/* if get here: cannot have error */

	/* set the lower dentries for s_root */
	teadfs_set_lower_path(sb->s_root, &lower_path);

	/*
	 * No need to call interpose because we already have a positive
	 * dentry, which was instantiated by d_make_root.  Just need to
	 * d_rehash it.
	 */
	d_rehash(sb->s_root);
	if (!silent)
		LOG_ERR("wrapfs: mounted on top of %s type %s\n",
			dev_name, lower_sb->s_type->name);
	goto out; /* all is well */

	/* no longer needed: free_dentry_private_data(sb->s_root); */
out_freeroot:
	dput(sb->s_root);
out_iput:
	iput(inode);
out_sput:
	/* drop refs we took earlier */
	atomic_dec(&lower_sb->s_active);
	kfree(WRAPFS_SB(sb));
	sb->s_fs_info = NULL;
out_free:
	path_put(&lower_path);

out:
	return err;
}

static struct dentry* teadfs_mount(struct file_system_type* fs_type, int flags,
    const char* dev_name, void* raw_data) {
	int err = 0;
	struct super_block* lower_sb;
	struct path lower_path;
	char* dev_name = (char*)raw_data;
	struct inode* inode;

	void* lower_path_name = (void*)dev_name;

	return mount_nodev(fs_type, flags, lower_path_name,
		teadfs_read_super);
}


static struct file_system_type teadfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "teadfs",
    .mount = teadfs_mount,
    .kill_sb = generic_shutdown_super,
    .fs_flags = 0
};
 
static int __init my_module_init(void) {
    int rc;

    LOG_DBG("ENTRY\n");
    do {
        rc = register_filesystem(&teadfs_fs_type);
        if (rc) {
            LOG_DBG("Failed to register filesystem\n");
            break;
        }
    } while (0);
    LOG_DBG("LEVAL\n");
    return 0;
}
 
static void __exit my_module_exit(void) {
    LOG_DBG("ENTRY\n");
    unregister_filesystem(&teadfs_fs_type);
    LOG_DBG("LEVAL\n");
}
 
module_init(my_module_init);
module_exit(my_module_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple kernel module example");
MODULE_VERSION("0.1");


