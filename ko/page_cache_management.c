#include "kp_header.h"

// this function decides whether to use copy_to_user() or page_cache_write()
u64 kp_write(u64 to, u64 from, u64 bytes)
{
    struct vm_area_struct *vma;
    u64 rv;

    vma = find_vma(current->mm, to);
    vma->vm_flags += VM_WRITE;
    if(addr_in_randomized_area(to))
    {
        rv = page_cache_write((void *)to, from, bytes);
        
    }
    else
    {
        rv = copy_to_user((void *)to, from, bytes);
    }
    vma->vm_flags -= VM_WRITE;

    return rv;
}

// TODO: given **to**, obtain the corresponding **ptep**, instead of obtaining **ptep** from **vmf**
u64 page_cache_write(u64 to, u64 from, u64 bytes)
{
    struct vm_area_struct *vma;
	pte_t *ptep;
	struct page* page;
	int already_locked;
    spinlock_t *ptl;
    u64 ret;

    vma = find_vma(current->mm, to);
    ptep = addr_to_ptep(to, &ptl);

	VM_BUG_ON(pte_write(*ptep) || !pte_present(*ptep));
    
    page = pte_page(*ptep);

    kp_pr_info("readch 1, ptep: %llx\n", ptep);

	already_locked = PageLocked(page);
	if(!already_locked) 
    {
		lock_page(page);
	}

    // mark the pte as writable
	{
        pte_t writable_pte = pte_mkwrite(*ptep);
        set_pte_at(vma->vm_mm, to & PAGE_MASK, ptep, writable_pte);
        flush_cache_page(vma, to & PAGE_MASK, pte_pfn(*ptep));
        flush_tlb_page(vma, to & PAGE_MASK);
        update_mmu_cache(vma, to & PAGE_MASK, ptep);
    }

    // where the actual write happens
    {
        ret = copy_to_user((void *)to, from, bytes);
        flush_cache_page(vma, to & PAGE_MASK, pte_pfn(*ptep));
        flush_tlb_page(vma, to & PAGE_MASK);
        update_mmu_cache(vma, to & PAGE_MASK, ptep);
    }

    // restore the read-only pte
    {
        pte_t rdonly_pte = pte_mkclean( pte_wrprotect(*ptep) );
        set_pte_at(vma->vm_mm, to & PAGE_MASK, ptep, rdonly_pte);
        flush_cache_page(vma, to & PAGE_MASK, pte_pfn(*ptep));
        flush_tlb_page(vma, to & PAGE_MASK);
        update_mmu_cache(vma, to & PAGE_MASK, ptep);
    }
	
	if(!already_locked) 
    {
		unlock_page(page);
	}

    pte_unmap_unlock(ptep, ptl);

	return ret;
}

// given **addr**, fills the corresponding page with zeroes
void page_purging(u64 addr)
{
    void *empty_page;
    empty_page = kzalloc(PAGE_SIZE, GFP_KERNEL);
    kp_write((void *)addr, empty_page, PAGE_SIZE);
    kfree(empty_page);
}