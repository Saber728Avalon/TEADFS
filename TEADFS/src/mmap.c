#include "teadfs_log.h"
#include "teadfs_header.h"
#include "user_com.h"
#include "mem.h"
#include "protocol.h"
#include "file.h"

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/fs_stack.h>


#define ENCRYPT_FILE_HEADER_SIZE 256

/**
 * teadfs_read_lower
 * @data: The read data is stored here by this function
 * @offset: Byte offset in the lower file from which to read the data
 * @size: Number of bytes to read from @offset of the lower file and
 *        store into @data
 * @ecryptfs_inode: The eCryptfs inode
 *
 * Read @size bytes of data at byte offset @offset from the lower
 * inode into memory location @data.
 *
 * Returns bytes read on success; 0 on EOF; less than zero on error
 */
int teadfs_read_lower(char* data, loff_t offset, size_t size,
	struct file* file)
{
	struct teadfs_file_info* file_info;
	int rc = 0;
	int encrypt_len = 0;

	LOG_DBG("ENTRY\n");
	do {
		file_info = teadfs_file_to_private(file);
		if (!file_info->lower_file) {
			rc = -EIO;
			break;
		}
		if (OFR_DECRYPT == file_info->access) {
			offset += ENCRYPT_FILE_HEADER_SIZE;
		}
		// read
		rc = kernel_read(file_info->lower_file, offset, data, size);
		if (rc < 0) {
			LOG_ERR("kernel_read error:%d\n", file, file_info->lower_file);
			break;
		}
		//encrypt file, will send to user mode
		if (OFR_DECRYPT == file_info->access) {
			encrypt_len = teadfs_request_read(offset, data, rc, data, size);
			if (encrypt_len <= 0) {
				break;
			}
			rc = encrypt_len;
		}

	} while (0);
	LOG_DBG("LEVAL rc ; [%d]\n", rc);
	return rc;
}


/**
 * teadfs_write_lower
 * @ecryptfs_inode: The eCryptfs inode
 * @data: Data to write
 * @offset: Byte offset in the lower file to which to write the data
 * @size: Number of bytes from @data to write at @offset in the lower
 *        file
 *
 * Write data to the lower file.
 *
 * Returns bytes written on success; less than zero on error
 */
int teadfs_write_lower(struct file* file, char* data,
	loff_t offset, size_t size)
{
	struct teadfs_file_info* file_info;
	ssize_t rc;
	int encrypt_len = 0;
	char* buf = NULL;

	LOG_DBG("ENTRY\n");
	do {

		file_info = teadfs_file_to_private(file);
		if (!file_info->lower_file) {
			rc = -EIO;
			break;
		}
		buf = teadfs_zalloc(size, GFP_KERNEL);
		if (!buf) {
			rc = -EIO;
			break;
		}
		// cann't edit file, in encrypt open.
		if (OFR_ENCRYPT == file_info->access) {
			rc = -EIO;
			break;
		}
		//send to user mode
		if (OFR_DECRYPT == file_info->access) {
			encrypt_len = teadfs_request_write(offset, data, size, buf, size);
			if (encrypt_len < 0) {
				rc = -EIO;
				break;
			}
			data = buf;
			size = encrypt_len;
			offset += ENCRYPT_FILE_HEADER_SIZE;
		}
		//write data to file
		rc = kernel_write(file_info->lower_file, data, size, offset);
		if (rc < 0) {
			LOG_ERR("kernel_read error:%d\n", file, file_info->lower_file);
			break;
		}
		mark_inode_dirty_sync(file->f_inode);
	} while (0);

	if (buf) {
		teadfs_free(buf);
	}
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}

/**
 * teadfs_readpage
 * @file: An eCryptfs file
 * @page: Page from eCryptfs inode mapping into which to stick the read data
 *
 * Read in a page, decrypting if necessary.
 *
 * Returns zero on success; non-zero on error.
 */
static int teadfs_readpage(struct file* file, struct page* page)
{
	int rc = 0;
	char* virt;
	loff_t offset;

	LOG_DBG("ENTRY\n");
	do {
		offset = (((loff_t)page->index) << PAGE_CACHE_SHIFT);
		virt = kmap(page);
		rc = teadfs_read_lower(virt, offset, PAGE_CACHE_SIZE, file);
		if (rc > 0)
			rc = 0;
		kunmap(page);
		flush_dcache_page(page);

		if (rc)
			ClearPageUptodate(page);
		else
			SetPageUptodate(page);
		unlock_page(page);
	} while (0);

	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}

/**
 * ecryptfs_get_locked_page
 *
 * Get one page from cache or lower f/s, return error otherwise.
 *
 * Returns locked and up-to-date page (if ok), with increased
 * refcnt.
 */
struct page* teadfs_get_locked_page(struct inode* inode, loff_t index)
{
	struct page* page = read_mapping_page(inode->i_mapping, index, NULL);
	if (!IS_ERR(page))
		lock_page(page);
	return page;
}


/**
 * teadfs_write
 * @ecryptfs_inode: The eCryptfs file into which to write
 * @data: Virtual address where data to write is located
 * @offset: Offset in the eCryptfs file at which to begin writing the
 *          data from @data
 * @size: The number of bytes to write from @data
 *
 * Write an arbitrary amount of data to an arbitrary location in the
 * eCryptfs inode page cache. This is done on a page-by-page, and then
 * by an extent-by-extent, basis; individual extents are encrypted and
 * written to the lower page cache (via VFS writes). This function
 * takes care of all the address translation to locations in the lower
 * filesystem; it also handles truncate events, writing out zeros
 * where necessary.
 *
 * Returns zero on success; non-zero otherwise
 */
static int teadfs_write(struct dentry* dentry, struct inode* ecryptfs_inode, char* data, loff_t offset,
	size_t size)
{
	struct page* ecryptfs_page;
	char* ecryptfs_page_virt;
	loff_t ecryptfs_file_size = i_size_read(ecryptfs_inode);
	loff_t data_offset = 0;
	loff_t pos;
	int rc = 0;
	struct file* file;
	int flags = O_RDWR;

	LOG_DBG("ENTRY\n");
	do {
		/*
		 * if we are writing beyond current size, then start pos
		 * at the current size - we'll fill in zeros from there.
		 */
		if (offset > ecryptfs_file_size) //文件偏移位置，超过了文件末尾.修改到文件末尾
			pos = ecryptfs_file_size;
		else
			pos = offset;

		file = teadfs_get_lower_file(dentry, NULL, flags);
		if (IS_ERR(file)) {
			rc = PTR_ERR(file);
			LOG_ERR("%s: Error encrypting "
				"page; rc = [%d]\n", __func__, rc);
			break;
		}
		rc = kernel_write(file, data, size, pos);
		teadfs_put_lower_file(NULL, file);
		if (rc < 0) {
			LOG_ERR("kernel_read error:%d\n", file, rc);
			break;
		}
		pos += size;
		if (pos > ecryptfs_file_size) {
			i_size_write(ecryptfs_inode, pos);
		}
	} while (0);
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}

/**
 * truncate_upper
 * @dentry: The ecryptfs layer dentry
 * @ia: Address of the ecryptfs inode's attributes
 * @lower_ia: Address of the lower inode's attributes
 *
 * Function to handle truncations modifying the size of the file. Note
 * that the file sizes are interpolated. When expanding, we are simply
 * writing strings of 0's out. When truncating, we truncate the upper
 * inode and update the lower_ia according to the page index
 * interpolations. If ATTR_SIZE is set in lower_ia->ia_valid upon return,
 * the caller must use lower_ia in a call to notify_change() to perform
 * the truncation of the lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
int truncate_upper(struct dentry* dentry, struct iattr* ia,
	struct iattr* lower_ia) {
	int rc = 0;
	struct inode* inode = dentry->d_inode;
	loff_t i_size = i_size_read(inode);
	loff_t lower_size_before_truncate;
	loff_t lower_size_after_truncate;
	char* buf = NULL;

	if (unlikely((ia->ia_size == i_size))) {
		lower_ia->ia_valid &= ~ATTR_SIZE;
		return 0;
	}
	do {
		LOG_INF("resize:%lld --> %lld\n", i_size, ia->ia_size);
		/* Switch on growing or shrinking file */
		if (ia->ia_size > i_size) {//说明文件在扩展
			lower_ia->ia_valid &= ~ATTR_SIZE;
			/* Write a single 0 at the last position of the file;
			 * this triggers code that will fill in 0's throughout
			 * the intermediate portion of the previous end of the
			 * file and the new and of the file */
			buf = teadfs_zalloc(ia->ia_size - i_size, GFP_KERNEL);
			if (!buf) {
				LOG_ERR("Error attempting to allocate memory\n");
				rc = -ENOMEM;
				break;
			}
			memset(buf, 0, ia->ia_size - i_size);
			rc = teadfs_write(dentry, inode, buf,
				ia->ia_size, ia->ia_size - i_size);
			if (rc > 0) {
				LOG_ERR("teadfs_write Error %d\n", rc);
				rc = 0;
				break;
			}
			teadfs_free(buf);
		} else { /* ia->ia_size < i_size_read(inode) */  //文件被截断
		 /* We're chopping off all the pages down to the page
		  * in which ia->ia_size is located. Fill in the end of
		  * that page from (ia->ia_size & ~PAGE_CACHE_MASK) to
		  * PAGE_CACHE_SIZE with zeros. */
			truncate_setsize(inode, ia->ia_size);
			lower_ia->ia_size = ia->ia_size;
			lower_ia->ia_valid |= ATTR_SIZE;

		}
	} while (0);
	return rc;
}

/**
 * teadfs_truncate
 * @dentry: The ecryptfs layer dentry
 * @new_length: The length to expand the file to
 *
 * Simple function that handles the truncation of an eCryptfs inode and
 * its corresponding lower inode.
 *
 * Returns zero on success; non-zero otherwise
 */
static int teadfs_truncate(struct dentry* dentry, loff_t new_length)
{
	struct iattr ia = { .ia_valid = ATTR_SIZE, .ia_size = new_length };
	struct iattr lower_ia = { .ia_valid = 0 };
	int rc;
	struct dentry* lower_dentry;
	struct path lower_path;

	LOG_DBG("ENTRY\n");
	teadfs_get_lower_path(dentry, &lower_path);
	lower_dentry = lower_path.dentry;
	rc = truncate_upper(dentry, &ia, &lower_ia);
	if (!rc && lower_ia.ia_valid & ATTR_SIZE) {
		mutex_lock(&lower_dentry->d_inode->i_mutex);
		rc = notify_change(lower_dentry, &lower_ia, NULL);
		mutex_unlock(&lower_dentry->d_inode->i_mutex);
	}
	teadfs_put_lower_path(dentry, &lower_path);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}



/**
 * ecryptfs_write_begin
 * @file: The eCryptfs file
 * @mapping: The eCryptfs object
 * @pos: The file offset at which to start writing
 * @len: Length of the write
 * @flags: Various flags
 * @pagep: Pointer to return the page
 * @fsdata: Pointer to return fs data (unused)
 *
 * This function must zero any hole we create
 *
 * Returns zero on success; non-zero otherwise
 */
static int treadfs_write_begin(struct file* file,
	struct address_space* mapping,
	loff_t pos, unsigned len, unsigned flags,
	struct page** pagep, void** fsdata)
{
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	struct page* page;
	loff_t prev_page_end_size;
	int rc = 0;
	char* virt;
	loff_t offset;

	struct teadfs_file_info* file_info = teadfs_file_to_private(file);
	struct dentry* teadfs_dentry = file->f_path.dentry;

	LOG_INF("ENTRY file:%px name:%s\n", file, teadfs_dentry->d_name.name);


	//find page. if not exist create page.
	page = grab_cache_page_write_begin(mapping, index, flags);
	if (!page)
		return -ENOMEM;
	*pagep = page;

	LOG_DBG("ENTRY\n");

	do {
		prev_page_end_size = ((loff_t)index << PAGE_CACHE_SHIFT);
		if (!PageUptodate(page)) {
			offset = (((loff_t)index) << PAGE_CACHE_SHIFT);
			virt = kmap(page);
			rc = teadfs_read_lower(virt, offset, PAGE_CACHE_SIZE, file);
			if (rc > 0)
				rc = 0;
			kunmap(page);
			flush_dcache_page(page);

			if (rc) {
				LOG_ERR("%s: Error attemping to read "
					"lower page segment; rc = [%d]\n",
					__func__, rc);
				ClearPageUptodate(page);
				break;
			}
			else
				SetPageUptodate(page);

		}
		/* If creating a page or more of holes, zero them out via truncate.
		 * Note, this will increase i_size. */
		if (index != 0) {
			if (prev_page_end_size > i_size_read(page->mapping->host)) {
				rc = teadfs_truncate(file->f_path.dentry,
					prev_page_end_size);
				if (rc) {
					LOG_ERR("%s: Error on attempt to "
						"truncate to (higher) offset [%lld];"
						" rc = [%d]\n", __func__,
						prev_page_end_size, rc);
					break;
				}
			}
		}
		/* Writing to a new page, and creating a small hole from start
		 * of page?  Zero it out. */
		if ((i_size_read(mapping->host) == prev_page_end_size)
			&& (pos != 0))
			zero_user(page, 0, PAGE_CACHE_SIZE);
	} while (0);
	
	if (unlikely(rc)) {
		unlock_page(page);
		page_cache_release(page);
		*pagep = NULL;
	}
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}




/**
 * teadfs_write_end
 * @file: The eCryptfs file object
 * @mapping: The eCryptfs object
 * @pos: The file position
 * @len: The length of the data (unused)
 * @copied: The amount of data copied
 * @page: The eCryptfs page
 * @fsdata: The fsdata (unused)
 */
static int teadfs_write_end(struct file* file,
	struct address_space* mapping,
	loff_t pos, unsigned len, unsigned copied,
	struct page* page, void* fsdata)
{
	pgoff_t index = pos >> PAGE_CACHE_SHIFT;
	unsigned from = pos & (PAGE_CACHE_SIZE - 1);
	unsigned to = from + copied;
	struct inode* ecryptfs_inode = mapping->host;
	int rc;
	char* virt;
	loff_t offset;

	struct teadfs_file_info* file_info = teadfs_file_to_private(file);
	struct dentry* teadfs_dentry = file->f_path.dentry;

	LOG_INF("ENTRY file:%px pos:%lld, len:%d, copied:%d name:%s\n", file, pos, len, copied, teadfs_dentry->d_name.name);

	LOG_DBG("ENTRY\n");

	offset = (((loff_t)page->index) << PAGE_CACHE_SHIFT);
	virt = kmap(page);
	rc = teadfs_write_lower(file, virt, offset, to);
	if (rc > 0)
		rc = 0;
	kunmap(page);
	if (!rc) {
		rc = copied;
		fsstack_copy_inode_size(ecryptfs_inode,
			teadfs_inode_to_lower(ecryptfs_inode));
	}

	unlock_page(page);
	page_cache_release(page);

	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}


/**
 * teadfs_writepage
 * @page: Page that is locked before this call is made
 *
 * Returns zero on success; non-zero otherwise
 *
 * This is where we encrypt the data and pass the encrypted data to
 * the lower filesystem.  In OpenPGP-compatible mode, we operate on
 * entire underlying packets.
 */
static int teadfs_writepage(struct page* page, struct writeback_control* wbc)
{
	int rc;
	LOG_DBG("ENTRY\n");

	SetPageUptodate(page);
	unlock_page(page);
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}


static sector_t teadfs_bmap(struct address_space* mapping, sector_t block)
{
	int rc = 0;
	struct inode* inode;
	struct inode* lower_inode;

	LOG_DBG("ENTRY\n");
	inode = (struct inode*)mapping->host;
	lower_inode = teadfs_inode_to_lower(inode);
	if (lower_inode->i_mapping->a_ops->bmap)
		rc = lower_inode->i_mapping->a_ops->bmap(lower_inode->i_mapping,
			block);

	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}


const struct address_space_operations teadfs_aops = {
	.writepage = teadfs_writepage,
	.readpage = teadfs_readpage,
	.write_begin = treadfs_write_begin,
	.write_end = teadfs_write_end,
	.bmap = teadfs_bmap,
};