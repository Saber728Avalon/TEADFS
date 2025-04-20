#ifndef TEADFS_HEADER_H
#define TEADFS_HEADER_H

#include "config.h"

#include <linux/fs.h>
#include <linux/path.h>
#include <linux/wait.h>
#if defined(CONFIG_BDICONFIG_BDI)
	#include <linux/backing-dev.h>
#endif


#define TEADFS_SUPER_MAGIC 0x44414554

/* wrapfs super-block data in memory */
struct teadfs_sb_info {
	struct super_block* lower_sb;

#if defined(CONFIG_BDICONFIG_BDI)
	struct backing_dev_info bdi;
#endif
};


/* wrapfs dentry data in memory */
struct teadfs_dentry_info {
	struct path lower_path;
};

/* file private data. */
struct teadfs_file_info {
	struct file* lower_file;
};


/* inode private data. */
struct teadfs_inode_info {
	struct inode vfs_inode;
	struct inode* lower_inode;
	struct mutex lower_file_mutex;
	atomic_t lower_file_count;
};


struct teadfs_msg_ctx {
#define TEADFS_MSG_CTX_STATE_FREE     0x01
#define TEADFS_MSG_CTX_STATE_PENDING  0x02
#define TEADFS_MSG_CTX_STATE_DONE     0x03
#define TEADFS_MSG_CTX_STATE_NO_REPLY 0x04
	u8 state;
	__u64 msg_id;
	size_t request_msg_size;
	char* request_msg;
	size_t response_msg_size;
	char* response_msg;
	struct list_head out_list;
	struct mutex mux;
	wait_queue_head_t wait;
};


/* superblock to private data */
static struct teadfs_sb_info* teadfs_get_super_block(struct super_block *super) {
	return ((struct teadfs_sb_info*)(super)->s_fs_info);
};
static void
teadfs_set_superblock_lower(struct super_block* sb,
	struct super_block* lower_sb)
{
	((struct teadfs_sb_info*)sb->s_fs_info)->lower_sb = lower_sb;
}
static inline struct super_block*
teadfs_superblock_to_lower(struct super_block* super)
{
	return  teadfs_get_super_block(super)->lower_sb;
}
	
static void teadfs_set_lower_super(struct super_block* super, struct teadfs_sb_info* super_info)
{
	super->s_fs_info = super_info;
}



static void
teadfs_set_file_private(struct file* file,
	struct teadfs_file_info* file_info)
{
	file->private_data = file_info;
}

static struct file* teadfs_file_to_lower(struct file* file)
{
	return ((struct teadfs_file_info*)file->private_data)->lower_file;
}

static inline struct teadfs_file_info*
teadfs_file_to_private(struct file* file)
{
	return file->private_data;
}


static inline struct teadfs_dentry_info*
teadfs_dentry_to_private(struct dentry* dentry)
{
	return (struct teadfs_dentry_info*)dentry->d_fsdata;
}

static inline void
teadfs_set_dentry_private(struct dentry* dentry,
	struct teadfs_dentry_info* dentry_info)
{
	dentry->d_fsdata = dentry_info;
}

static inline struct dentry*
teadfs_dentry_to_lower(struct dentry* dentry)
{
	return ((struct teadfs_dentry_info*)dentry->d_fsdata)->lower_path.dentry;
}
static inline struct path*
teadfs_dentry_to_lower_path(struct dentry* dentry)
{
	return &((struct teadfs_dentry_info*)dentry->d_fsdata)->lower_path;
}
static void teadfs_set_lower_path(const struct dentry* dentry, struct path* lower_path) {
	((struct teadfs_dentry_info*)dentry->d_fsdata)->lower_path.dentry = lower_path->dentry;
	((struct teadfs_dentry_info*)dentry->d_fsdata)->lower_path.mnt = lower_path->mnt;
	return;
}




static inline struct teadfs_inode_info*
teadfs_inode_to_private(struct inode* inode)
{
	return container_of(inode, struct teadfs_inode_info, vfs_inode);
}

static inline struct inode* teadfs_inode_to_lower(struct inode* inode)
{
	return teadfs_inode_to_private(inode)->lower_inode;
}

static inline void
teadfs_set_inode_lower(struct inode* inode, struct inode* lower_inode)
{
	teadfs_inode_to_private(inode)->lower_inode = lower_inode;
}

#endif //TEADFS_HEADER_H