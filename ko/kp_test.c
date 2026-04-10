// test utilities for K-Polarizer
#include "kp_header.h"

void dump_runtime_bin()
{
    struct vm_area_struct *vma_iter;
    struct file *filp;
    loff_t pos = 0;
    mm_segment_t old_fs;

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open("/path/you/choose", O_WRONLY|O_CREAT, 0644);
    set_fs(old_fs);
    void *data = kmalloc(PAGE_SIZE, GFP_KERNEL);

    if (IS_ERR(filp)) 
	{
        kp_pr_err("Cannot open file\n");
        return;
    }

    set_fs(old_fs);
    for(vma_iter = current->mm->mmap; vma_iter; vma_iter = vma_iter->vm_next)
    {
        if(vma_iter->vm_file && strcmp(vma_iter->vm_file->f_path.dentry->d_name.name, "libc.so.rewritten") == 0)
        {
            u64 addr_iter;
            kp_pr_info("found vma %llx - %llx\n", vma_iter->vm_start - find_so_base("libc.so.rewritten"), vma_iter->vm_end - find_so_base("libc.so.rewritten"));
            
            for(addr_iter = vma_iter->vm_start; addr_iter < vma_iter->vm_end; addr_iter += PAGE_SIZE)
            {
                copy_from_user(data, (void __user *)addr_iter, PAGE_SIZE);
                kernel_write(filp, data, PAGE_SIZE, &pos);
            }
            
        }
    }

    filp_close(filp, NULL);
    kfree(data);
    return;
}

// works just fine....
void test_page_cache_write()
{
    struct vm_area_struct *vma_iter;
    loff_t pos = 0;

    for(vma_iter = current->mm->mmap; vma_iter; vma_iter = vma_iter->vm_next)
    {
        if(vma_iter->vm_file && strcmp(vma_iter->vm_file->f_path.dentry->d_name.name, "hello") == 0)
        {
            u64 addr_iter;
            for(addr_iter = vma_iter->vm_start; addr_iter < vma_iter->vm_end; addr_iter += PAGE_SIZE)
            {
                if(addr_iter - find_so_base("hello") == 0x0)
                {
                    void *origin_page;
                    pgoff_t pgoff;

                    pgoff = (addr_iter - find_so_base("hello")) >> PAGE_SHIFT;
                    origin_page = kmalloc(PAGE_SIZE, GFP_KERNEL);
                    read_disk(origin_page, "/path/you/choose", pgoff);
                    page_cache_write(addr_iter, origin_page, PAGE_SIZE);

                    kfree(origin_page);
                }
                
            }
            
        }
    }

    return;
}