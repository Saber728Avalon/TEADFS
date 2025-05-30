#ifndef TEADFS_HEADER_H
#define TEADFS_HEADER_H

#include "config.h"
#include "protocol.h"

#include <linux/fs.h>
#include <linux/path.h>
#include <linux/wait.h>
#if defined(CONFIG_BDICONFIG_BDI)
	#include <linux/backing-dev.h>
#endif
#include "teadfs_log.h"

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
	spinlock_t lock; /* protects lower_path */
	struct path lower_path;
};

/* file private data. */
struct teadfs_file_info {
	struct file* lower_file;
	enum OPEN_FILE_RESULT access;
	char* file_path;
	int file_path_length;
	char* file_path_buf;
};


/* inode private data. */
struct teadfs_inode_info {
	struct inode vfs_inode;
	struct inode* lower_inode;
	struct mutex lower_file_mutex;
	//double buffer, decrypt data
	struct address_space i_decrypt;
	atomic_t lower_file_count;
	int file_decrypt;
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
/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path* dst, const struct path* src) {
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void teadfs_get_lower_path(const struct dentry* dent, struct path* lower_path) {
	struct teadfs_dentry_info* lower_dentry = teadfs_dentry_to_private(dent);
	spin_lock(&(lower_dentry->lock));
	LOG_DBG("ENTRY %d\n", lower_dentry->lower_path.dentry->d_lockref.count);
	pathcpy(lower_path, &(lower_dentry->lower_path));
	//path_get(lower_path);
	spin_unlock(&(lower_dentry->lock));
	LOG_DBG("LEAVE\n");
	return;
}
static inline void teadfs_put_lower_path(const struct dentry* dent, struct path* lower_path) {
	struct teadfs_dentry_info* lower_dentry = teadfs_dentry_to_private(dent);
	spin_lock(&(lower_dentry->lock));
	LOG_DBG("ENTRY %d\n", lower_dentry->lower_path.dentry->d_lockref.count);
	//path_put(lower_path);
	spin_unlock(&(lower_dentry->lock));
	LOG_DBG("LEAVE\n");
	return;
}
static inline void teadfs_set_lower_path(const struct dentry* dent, struct path* lower_path) {
	struct teadfs_dentry_info* lower_dentry = teadfs_dentry_to_private(dent);
	LOG_DBG("ENTRY\n");
	spin_lock(&(lower_dentry->lock));
	pathcpy(&(((struct teadfs_dentry_info*)(dent)->d_fsdata)->lower_path), lower_path);
	spin_unlock(&(lower_dentry->lock));
	LOG_DBG("LEAVE %d\n", lower_dentry->lower_path.dentry->d_lockref.count);
	return;
}
static inline void wrapfs_put_reset_lower_path(const struct dentry* dent) {
	struct path lower_path;
	LOG_DBG("ENTRY %d\n", dent->d_lockref.count);
	spin_lock(&(((struct teadfs_dentry_info*)(dent)->d_fsdata)->lock));
	pathcpy(&lower_path, &(((struct teadfs_dentry_info*)(dent)->d_fsdata)->lower_path));
	((struct teadfs_dentry_info*)(dent)->d_fsdata)->lower_path.dentry = NULL;
	((struct teadfs_dentry_info*)(dent)->d_fsdata)->lower_path.mnt = NULL;
	spin_unlock(&(((struct teadfs_dentry_info*)(dent)->d_fsdata)->lock));
	path_put(&lower_path);
	LOG_DBG("ENTRY\n");
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