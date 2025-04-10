#ifndef TEADFS_HEADER_H
#define TEADFS_HEADER_H

#include <linux/fs.h>
#include <linux/path.h>

/* wrapfs super-block data in memory */
struct teadfs_sb_info {
	struct super_block* lower_sb;
};


/* wrapfs dentry data in memory */
struct teadfs_dentry_info {
	struct path lower_path;
};


/* superblock to private data */
static struct teadfs_sb_info* teadfs_get_super_block(struct super_block *super) {
	return ((struct teadfs_sb_info*)(super)->s_fs_info);
};
static inline struct super_block*
teadfs_superblock_to_lower(struct super_block* super)
{
	return  teadfs_get_super_block(super)->lower_sb;
}
	
static void teadfs_set_lower_super(struct super_block* super, struct super_block* val)
{
	((struct teadfs_sb_info *)super)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path* dst, const struct path* src) {
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
static void teadfs_set_lower_path(const struct dentry* dentry, struct path* lower_path) {
	pathcpy(lower_path, &((struct teadfs_dentry_info*)dentry->d_fsdata)->lower_path);
	path_get(lower_path);
	return;
}



#endif //TEADFS_HEADER_H