#include "kp_header.h"

u64 find_so_base(char *so_name)
{
    struct mm_struct *mm;
	struct vm_area_struct *vma_iter;
    u64 so_base;

    so_base = 0xffffffffffffffff;
	mm = current->mm;
	down_read(&mm->mmap_sem);
    for (vma_iter = mm->mmap; vma_iter; vma_iter = vma_iter->vm_next)
	{
		if(!vma_iter->vm_file)
		{
			continue;
		}

		if(strcmp(so_name, vma_iter->vm_file->f_path.dentry->d_name.name) == 0)
		{
			so_base = min(so_base, vma_iter->vm_start);
		}
    }
    up_read(&mm->mmap_sem);

    return so_base;
}