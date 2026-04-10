#include "kp_header.h"

static section_info *get_sec_info(char *sec_name)
{
	int sh_iter;
	for(sh_iter = 0; sh_iter < SEC_INFO_NUM; sh_iter++)
	{
		if(section_infos[sh_iter] == NULL)
		{
			break;
		}

		if(strcmp(sec_name, section_infos[sh_iter]->name) == 0)
		{
			return section_infos[sh_iter];
		}
	}
	return NULL;
}

// TODO:
bool addr_in_randomized_area(u64 addr)
{
	if(cls_infos[0]->cls_start <= addr && addr < cls_infos[clsinfo_cnt - 1]->cls_end)
	{
		return true;
	}
	return false;
}

// FIXME: pgoff is offset, maybe inapproriate
u64 transform_addr(u64 old_addr)
{
	if(page_permutation_table == NULL)
	{
		kp_pr_err("error in transform_addr: ppt hasn't initialized\n");
		return 0x0;
	}

	if(!addr_in_randomized_area(old_addr))
	{
		return old_addr;
	}

	// get the new pgoff by consulting page_permutation_table
	pgoff_t old_pgoff = old_addr >> PAGE_SHIFT, new_pgoff;
	
	// naive approach to compute new_pgoff
	int ppt_iter;
	for(ppt_iter = 0; ppt_iter < PPT_SIZE; ppt_iter++)
	{
		if(page_permutation_table[ppt_iter][1] == old_pgoff)
		{
			new_pgoff = page_permutation_table[ppt_iter][0];
			break;
		}
	}

	return (new_pgoff << PAGE_SHIFT) + (old_addr % PAGE_SIZE );
}

u64 get_original_addr(u64 new_addr)
{
	if(page_permutation_table == NULL)
	{
		kp_pr_err("error in transform_addr: ppt hasn't initialized\n");
		return 0x0;
	}

	if(!addr_in_randomized_area(new_addr))
	{
		return new_addr;
	}

	pgoff_t new_pgoff = new_addr >> PAGE_SHIFT, old_pgoff;

	int ppt_iter;
	for(ppt_iter = 0; ppt_iter < PPT_SIZE; ppt_iter++)
	{
		if(page_permutation_table[ppt_iter][0] == new_pgoff)
		{
			old_pgoff = page_permutation_table[ppt_iter][1];
			break;
		}
	}

	return (old_pgoff << PAGE_SHIFT) + (new_addr % PAGE_SIZE );
}

/* 
 * update ptr_info when page_permutation_table is initialized
 * 
 * FIXME: let's assume no pointer overflow happens
 */
void update_single_ptr_info(ptr_info *pinfo)
{
	pinfo->new_base = transform_addr(pinfo->base);
    pinfo->new_va = transform_addr(pinfo->va);
    pinfo->new_trg = transform_addr(pinfo->trg);

    if(pinfo && pinfo->is_rel)
    {
        u64 delta_base = pinfo->new_base - pinfo->base;
        u64 delta_trg = pinfo->new_trg - pinfo->trg;

        pinfo->new_value = pinfo->value + delta_trg - delta_base;
    }
    else
    {
        pinfo->new_value = pinfo->new_trg;
    }
}

void update_all_ptr_info_entries(void)
{
    int pinfo_iter;

    if(page_permutation_table == NULL)
    {
        kp_pr_err("error in update_all_ptr_infos(): ppt hasn't initialzed\n");
        return;
    }

    for(pinfo_iter = 0; pinfo_iter < PTR_INFO_NUM; pinfo_iter++)
    {
		if(ptr_infos[pinfo_iter])
		{
			update_single_ptr_info(ptr_infos[pinfo_iter]);
		}
    }
}

/*
 * Returns true if addr belongs to a writable memory area.
 * In that case, we can use copy_to_user() directly, as the mapping is already private.
 * Otherwise, we should resort to page_cache_write() to interact with the page cache directly to avoid CoW.
 */
bool addr_in_writable_vma(u64 addr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, addr);
	if(vma && vma->vm_flags & VM_WRITE)
	{
		up_read(&mm->mmap_sem);
		return true;
	}
	up_read(&mm->mmap_sem);
	return false;
}

// FIXME: update to page_cache_write()
static void patch_single_ptr(ptr_info *pinfo, u64 so_base)
{
	u64 runtime_va;

	// make sure that we already prefualted all pages
	if(!current->prefaults_flag)
	{
		kp_pr_err("error in patch_single_ptr(): prefaults_flag == false\n");
	}

	runtime_va = so_base + pinfo->new_va;
	kp_write((void *)runtime_va, &pinfo->new_value, pinfo->size);
}

// patch all pointers according to ptr_infos
void patch_all_ptrs()
{
	int ptr_iter;
	// u64 so_base;

	if(ptr_infos == NULL)
	{
		kp_pr_err("error in patch_all_ptrs(): ptr_infos == NULL\n");
		return;
	}
	

	for(ptr_iter = 0; ptr_iter < PTR_INFO_NUM; ptr_iter++)
	{
		if(ptr_infos[ptr_iter])
		{
			patch_single_ptr(ptr_infos[ptr_iter], kp_so_base);
		}
	}
}

// some sections sotre aligned pointers, so 8-byte stride fits
static void fixed_stride_patcher(Elf64_Addr start, Elf64_Addr end, int stride)
{
	Elf64_Addr addr_iter;

	for(addr_iter = start; addr_iter < end; addr_iter += stride)
	{
		Elf64_Addr origin_value, new_value;
		copy_from_user(&origin_value, (void *)addr_iter, stride);

		if(origin_value == NULL)
		{
			continue;
		}

		new_value = transform_addr(origin_value);
		kp_write((void *)addr_iter, &new_value, stride);
	}
}

void patch_got()
{
	// u64 so_base;
	section_info *si_p;
	Elf64_Addr sec_start, sec_end;

	si_p = get_sec_info(".got");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	
	if(si_p)
	{
		sec_start += kp_so_base;
		sec_end += kp_so_base;
		fixed_stride_patcher(sec_start, sec_end, 8);
	}
	
}

// entsize: 24
void patch_dynsym()
{
	// u64 so_base;
	Elf64_Sym symbol_entry;
	Elf64_Addr sec_start, sec_end, addr_iter;
	section_info *si_p;
	si_p = get_sec_info(".dynsym");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	sec_start += kp_so_base;
	sec_end += kp_so_base;

	for(addr_iter = sec_start; addr_iter < sec_end; addr_iter += sizeof(Elf64_Sym))
	{
		copy_from_user(&symbol_entry, (void *)addr_iter, sizeof(Elf64_Sym));
		symbol_entry.st_value = transform_addr(symbol_entry.st_value);
		kp_write((void *)addr_iter, &symbol_entry, sizeof(Elf64_Sym));
	}
}

// patch .rela.plt & .rela.dyn (entsize: 24)
static void patch_rela(Elf64_Addr start, Elf64_Addr end)
{
	Elf64_Addr addr_iter;
	for(addr_iter = start; addr_iter < end; addr_iter += sizeof(Elf64_Rela))
	{
		Elf64_Rela rela_entry;
		copy_from_user(&rela_entry, (void *)addr_iter, sizeof(Elf64_Rela));

		rela_entry.r_addend = transform_addr(rela_entry.r_addend);
		rela_entry.r_offset = transform_addr(rela_entry.r_offset);
		kp_write((void *)addr_iter, &rela_entry, 24);
	}
}

// .rela.plt
void patch_rela_plt()
{
	section_info *si_p;
	Elf64_Addr sec_start, sec_end;

	si_p = get_sec_info(".rela.plt");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	
	if(si_p)
	{
		sec_start += kp_so_base;
		sec_end += kp_so_base;
		patch_rela(sec_start, sec_end);
	}
}

// .rela.dyn
void patch_rela_dyn()
{
	section_info *si_p;
	Elf64_Addr sec_start, sec_end;

	si_p = get_sec_info(".rela.dyn");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	
	if(si_p)
	{
		sec_start += kp_so_base;
		sec_end += kp_so_base;
		patch_rela(sec_start, sec_end);
	}
}

// FIXME: do we still need this function?
void patch_data_rel_ro()
{
	section_info *si_p = get_sec_info(".data.rel.ro");
	Elf64_Addr sec_start = si_p->addr;
	Elf64_Addr sec_end = si_p->addr + si_p->size;
}

// .got.plt
void patch_got_plt()
{
	section_info *si_p;
	Elf64_Addr sec_start, sec_end;

	si_p = get_sec_info(".got.plt");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	
	if(si_p)
	{
		sec_start += kp_so_base;
		sec_end += kp_so_base;
		fixed_stride_patcher(sec_start, sec_end, 8);
	}
}

// .init_array
void patch_init_array()
{
	// u64 so_base;
	section_info *si_p;
	Elf64_Addr sec_start, sec_end;

	si_p = get_sec_info(".init_array");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	
	if(si_p)
	{
		sec_start += kp_so_base;
		sec_end += kp_so_base;
		fixed_stride_patcher(sec_start, sec_end, 8);
	}
}

// .fini_array
void patch_fini_array()
{
	// u64 so_base;
	section_info *si_p;
	Elf64_Addr sec_start, sec_end;

	si_p = get_sec_info(".fini_array");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	
	if(si_p)
	{
		sec_start += kp_so_base;
		sec_end += kp_so_base;
		fixed_stride_patcher(sec_start, sec_end, 8);
	}
}

// .dynamic
// entsize: 16
void patch_dynamic()
{
	// u64 so_base;
	Elf64_Addr sec_start, sec_end, addr_iter;
	Elf64_Dyn dynamic_entry;
	section_info *si_p;
	si_p = get_sec_info(".dynamic");
	sec_start = si_p->addr;
	sec_end = si_p->addr + si_p->size;
	sec_start += kp_so_base;
	sec_end += kp_so_base;

	for(addr_iter = sec_start; addr_iter < sec_end; addr_iter++)
	{
		copy_from_user(&dynamic_entry, (void *)addr_iter, sizeof(Elf64_Dyn));

		switch(dynamic_entry.d_tag)
		{
			case DT_RELA:
				break;
			case DT_PLTGOT:
				break;
			case DT_JMPREL:
				break;
			case DT_SYMTAB:
				break;
			case DT_STRTAB:
				break;
			case 0x6ffffef5: //DT_GNU_HASH
				break;
			case DT_INIT:
				break;
			case DT_FINI:
				break;
			case 25: // DT_INIT_ARRAY
				break;
			case 26: // DT_FINI_ARRAY
				break;
			case DT_VERSYM:
				break;
			case DT_VERDEF:
				break;
			case DT_VERNEED:
				break;
			default:
				continue;
		}

		dynamic_entry.d_un.d_ptr = (Elf64_Addr)transform_addr((Elf64_Addr)dynamic_entry.d_un.d_ptr);
		kp_write((void *)addr_iter, &dynamic_entry, sizeof(Elf64_Dyn));
	}
}

void write_randomized_layout()
{
	void *origin_page;
	u64 rand_start, rand_end, addr_iter;


	origin_page = kzalloc(PAGE_SIZE, GFP_KERNEL);
	rand_start = cls_infos[0]->cls_start;
	rand_end = cls_infos[clsinfo_cnt - 1]->cls_end;

	for(addr_iter = rand_start; addr_iter < rand_end; addr_iter += PAGE_SIZE)
	{
		u64 new_addr, runtime_addr;
		pgoff_t pgoff;

		new_addr = transform_addr(addr_iter);
		pgoff = addr_iter >> PAGE_SHIFT;

		int rv = read_disk(origin_page, KP_LIB_PATH, pgoff);
		// kp_pr_info("%llx %x %d\n", new_addr, pgoff, rv);
		runtime_addr = new_addr + kp_so_base;
		kp_write((void *)runtime_addr, origin_page, PAGE_SIZE);
	}
	kfree(origin_page);
}

// randomize the protected library
// IMPORTANT: kp's key feature
void do_randomization()
{
	spin_lock(&kp_spinlock);

	// update ptr_infos according to the randomized layout
	update_all_ptr_info_entries();

	// write non-pointers
	kp_pr_info("write_randomized_layout\n");
	write_randomized_layout();

	kp_pr_info("patch_all_ptrs\n");
	patch_all_ptrs();

	// manually patch other sections that are not included in ptr_infos
	{
		kp_pr_info("patch dynamic\n");
		patch_dynamic();
		kp_pr_info("patch dynsym\n");
		patch_dynsym();
		// patch_data_rel_ro(); // already included in ptr_infos, so comment out
		kp_pr_info("patch init_array\n");
		patch_init_array();
		kp_pr_info("patch fini_array\n");
		patch_fini_array();
		kp_pr_info("patch got\n");
		patch_got();
		kp_pr_info("patch gotplt\n");
		patch_got_plt();
		kp_pr_info("patch rela_dyn\n");
		patch_rela_dyn();
		kp_pr_info("patch relaplt\n");
		patch_rela_plt();
	}

	kp_pr_info("patch finished %llx\n", kp_so_base);
	spin_unlock(&kp_spinlock);
}

