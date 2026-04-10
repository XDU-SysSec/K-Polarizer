#include "kp_header.h"


// as long as this function is exclusive, 
void do_debloating()
{
    u64 addr_iter, dep_iter, so_base, debloated_area_size, deps_iter;
    struct dependency_hash_node *entry;

    spin_lock(&kp_spinlock);
    so_base = kp_so_base;
    void *origin_pages;
    debloated_area_size = cls_infos[clsinfo_cnt - 1]->cls_end - cls_infos[0]->cls_start;
    origin_pages = vmalloc(debloated_area_size);
    copy_from_user(origin_pages, so_base + cls_infos[0]->cls_start, debloated_area_size);

    // page purging
    for(addr_iter = cls_infos[0]->cls_start; addr_iter < cls_infos[clsinfo_cnt - 1]->cls_end; addr_iter += PAGE_SIZE)
    {
        struct page* page;
        pte_t *ptep;
        spinlock_t *ptlp;
        u64 runtime_addr;
        
        runtime_addr = addr_iter + so_base;

        ptep = addr_to_ptep(runtime_addr, &ptlp); // this function acquires a split page table lock
        if(ptep == NULL)
        {
            kp_pr_info("while page purging, ptep null\n");
        }
        else
        {
            page = pte_page(*ptep);
            pte_unmap_unlock(ptep, ptlp); // NOTE: As page_purging() eventually acquires a split page table lock, a dead-lock might happen if we put pte_unmap_unlock() after page_purging()
            // kp_pr_info("%llx mapcnt %d\n", addr_iter, page_mapcount(page));
            if(page && page_mapcount(page) == 1)
            {
                page_purging(runtime_addr);
            }
        }
    }

#ifdef RUNTIME_DEP_RESOLVING
    hash_for_each(dependency_table, deps_iter, entry, node)
    {
        void *func_bin, *in_mem_content;
        u64 fsize;

        fsize = entry->end - entry->start;
        in_mem_content = kmalloc(fsize, GFP_KERNEL);

#ifdef KP_RANDOMIZATION
        u64 randomized_start, randomized_end;
        randomized_start = transform_addr(entry->start);
        randomized_end = transform_addr(entry->end);
        fsize = entry->end - entry->start;

        func_bin = origin_pages + randomized_start - cls_infos[0]->cls_start;

        
        copy_from_user(in_mem_content, so_base + randomized_start, fsize);
        // if this function hasn't been written to
        if(memchr_inv(in_mem_content, 0, fsize) == NULL)
        {
            kp_write(so_base + randomized_start, func_bin, fsize);
        }
#else
        func_bin = origin_pages + entry->start - cls_infos[0]->cls_start;
        copy_from_user(in_mem_content, so_base + entry->start, fsize);
        if(memchr_inv(in_mem_content, 0, fsize) == NULL)
        {
            kp_write(so_base + entry->start, func_bin, fsize);
        }
#endif
        kfree(in_mem_content);
    }

#elif defined(LEGACY_DEP_RESOLVING)
    // append all dependencies, legacy usage
    {
        for(dep_iter = 0; dep_iter < dinfo_cnt; dep_iter++)
        {
            void *func_bin;
            u64 fstart, fend;
            fstart = dep_infos[dep_iter]->start;
            fend = dep_infos[dep_iter]->end;
            func_bin = origin_pages + fstart - cls_infos[0]->cls_start;
            kp_write(so_base + fstart, func_bin, fend - fstart);
        }
    }
#endif
    vfree(origin_pages);
    spin_unlock(&kp_spinlock);
}

/* This function is called upon the first access to _start()
 * We simply assume the main executable is position dependent (compiled with -no-pie), so that its base address would be 0x400000...
 * We also assume .text section is aligned by 0x1000 (this is achieved by a customized linker script)...
 * Empirically, the traversal starts from the 3rd entry of .got.plt
 */
void dependency_resolving(struct vm_fault* vmf)
{
    loff_t pos;
    Elf64_Ehdr elf_header;
    Elf64_Shdr *section_header;
    char *sh_strtab, *app_path, *fbuf;
    int sh_iter;
    mm_segment_t oldfs;
    struct file *filp;


    fbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	app_path = d_path(&vmf->vma->vm_file->f_path, fbuf, PAGE_SIZE);
    kfree(fbuf);
    oldfs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open(app_path, O_RDONLY, 0);
    set_fs(oldfs);
    if (IS_ERR(filp))
    {
        kp_pr_err("Cannot open file: gcc\n");
        return;
    }

    pos = 0;
    kernel_read(filp, &elf_header, sizeof(Elf64_Ehdr), &pos);

    if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) 
    {
        kp_pr_err("Invalid ELF file\n");
        return;
    }

    // we only do .got.plt traversal upon the execution of _start()
    // FIXME: assume position dependent executable here
    if((elf_header.e_entry - 0x400000) >> PAGE_SHIFT != vmf->pgoff)
    {
        filp_close(filp, NULL);
        return;
    }

    section_header = kmalloc(sizeof(Elf64_Shdr) * elf_header.e_shnum, GFP_KERNEL);
    if (section_header == NULL) 
    {
        kp_pr_err("Memory allocation error\n");
        return;
    }

    pos = elf_header.e_shoff;
    kernel_read(filp, section_header, sizeof(Elf64_Shdr) * elf_header.e_shnum, &pos);
    sh_strtab = kmalloc(sizeof(char ) * (section_header[elf_header.e_shstrndx].sh_size), GFP_KERNEL);
    if (sh_strtab == NULL) 
    {
        kp_pr_err("Memory allocation error\n");
        kfree(section_header);
        return;
    }

    kp_pr_info("read .got.plt\n");
    pos = section_header[elf_header.e_shstrndx].sh_offset;
    kernel_read(filp, sh_strtab, section_header[elf_header.e_shstrndx].sh_size, &pos);
    for (sh_iter = 0; sh_iter < elf_header.e_shnum; sh_iter++) 
    {
        char *name;
        name = sh_strtab + section_header[sh_iter].sh_name;
        if(strcmp(name, ".got.plt") == 0)
        {
            u64 addr_iter, gotplt_start, gotplt_end;
            gotplt_start = section_header[sh_iter].sh_addr;
            gotplt_end = section_header[sh_iter].sh_addr + section_header[sh_iter].sh_size;

            // iteration starts from the 3rd entry
            for(addr_iter = gotplt_start + 24; addr_iter < gotplt_end; addr_iter += 8)
            {
                u64 api_addr, api_iter, dep_iter;
                copy_from_user(&api_addr, (void __user *)addr_iter, 8);
                api_addr -= kp_so_base;
                kp_pr_info(".got.plt %llx: %llx\n", addr_iter, api_addr);
#ifdef KP_RANDOMIZATION
                find_dependencies(&kp_deps, get_original_addr(api_addr)); // if randomization is enabled, api_addr is a randomized address, need to restore the original address
#else
                find_dependencies(&kp_deps, api_addr);
#endif
            }
        }
    }

    // manually add some musl-libc dependencies (__dls2, __dls3, etc.)
    {
        find_dependencies(&kp_deps, 0x274de); // __dls2()
        find_dependencies(&kp_deps, 0x2a5d5); // __dls3()
        find_dependencies(&kp_deps, 0x4d37d); // _dl_start_c()
        find_dependencies(&kp_deps, 0x66769); // memmove() NOTE this function is not included in the original libc.dep.offset (assembly?)
    }

    print_resolved_deps(); // 
    // write_resolved_deps(); // write to a file to ease debugging
    current->dep_resolved_flag = true;

    kfree(section_header);
    kfree(sh_strtab);
    filp_close(filp, NULL); 

}