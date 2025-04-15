#include "teadfs_log.h"
#include "teadfs_header.h"

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/fs_stack.h>

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
	struct file* lower_file;
	lower_file = teadfs_file_to_lower(file);
	if (!lower_file)
		return -EIO;
	return kernel_read(lower_file, offset, data, size);
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
	struct file* lower_file;
	ssize_t rc;

	lower_file = teadfs_file_to_lower(lower_file);
	if (!lower_file)
		return -EIO;
	rc = kernel_write(lower_file, data, size, offset);
	mark_inode_dirty_sync(file->f_inode);
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
		rc = teadfs_read_lower(virt, offset, PAGE_CACHE_SIZE, page->mapping->host);
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

	LOG_DBG("LEVAL %d\n", rc);
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
				//rc = ecryptfs_truncate(file->f_path.dentry,
				//	prev_page_end_size);
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
	LOG_DBG("LEVAL %d\n", rc);
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


	LOG_DBG("ENTRY\n");

	offset = (((loff_t)page->index) << PAGE_CACHE_SHIFT);
	virt = kmap(page);
	rc = teadfs_write_lower(ecryptfs_inode, virt, offset, to);
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

	LOG_DBG("LEVAL %d\n", rc);
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
	LOG_DBG("LEVAL %d\n", rc);
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

	LOG_DBG("LEVAL %d\n", rc);
	return rc;
}


const struct address_space_operations teadfs_aops = {
	.writepage = teadfs_writepage,
	.readpage = teadfs_readpage,
	.write_begin = treadfs_write_begin,
	.write_end = teadfs_write_end,
	.bmap = teadfs_bmap,
};