#include "kp_header.h"

ssize_t read_disk(void *buf, const char *fname, loff_t offset)
{
	int rv = 0;
	struct file *fp;
	struct page *page;
	void *disk_ptr;
	int error;

    fp = filp_open(fname, O_RDONLY , 0);
	if (unlikely(IS_ERR(fp))) 
	{
		kp_pr_err("filp_open failed\n");
	} 
	else 
	{
		page = alloc_page(GFP_KERNEL);
		get_page(page);
		page->mapping = fp->f_mapping;
		page->index = offset;
		__SetPageLocked(page);
		ClearPageError(page);
		rv = fp->f_mapping->a_ops->readpage(fp, page);
		if(rv)
		{
			kp_pr_err("readpage() error\n");
		}
		if(!PageUptodate(page))
		{
			error = lock_page_killable(page);
			if(PageUptodate(page))
			{
				unlock_page(page);
			}
		}
		disk_ptr = kmap(page);
		memcpy(buf, disk_ptr, PAGE_SIZE);
		kunmap(page);
		filp_close(fp, NULL);
		page->mapping = NULL;
		put_page(page);
	}

	return rv;
}