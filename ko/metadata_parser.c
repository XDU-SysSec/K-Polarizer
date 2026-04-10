#include "kp_header.h"


char *read_file_to_buf(const char *fname)
{
	struct file *filp = NULL;
    mm_segment_t oldfs;
    unsigned long bytes_read;
	char *buf;
	int fsize;

    oldfs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open(fname, O_RDONLY, 0);
    set_fs(oldfs);
    if (IS_ERR(filp))
	{
        kp_pr_err("Cannot open file: %s\n", fname);
        goto error;
    }

	fsize = i_size_read(file_inode(filp)); // obtain file size
	buf = vmalloc(fsize + 1); // vmalloc is preferred when fsize is very large (+1 for '\0')
	if(buf)
	{
		bytes_read = kernel_read(filp, buf, fsize, &filp->f_pos);
		if(bytes_read == fsize)
		{
			kp_pr_info("read 0x%lx bytes\n", bytes_read);
		}
		else
		{
			filp_close(filp, NULL);
			goto error;
		}
	}
	else
	{
		kp_pr_err("vmalloc failed\n");
		filp_close(filp, NULL);
		goto error;
	}
    
    filp_close(filp, NULL);
	buf[fsize] = '\0';
	return buf;

error:
	vfree(buf);
	return NULL;
}

/*
 * this function has side effects, it would change **buf**
 * format of ptr info: base, va, trg, size, value, is_rel, new_base, new_va, new_trg, new_value
 */ 
void parse_ptr_info(char *buf)
{
	char *line = buf;
    char *next_line;

    while ((next_line = strchr(line, '\n')) != NULL) 
	{
		ptr_info *pinfo;
        
		*next_line = '\0'; // replace \n with \0, so that line is a C string
		
		pinfo = (ptr_info *)vmalloc(sizeof(ptr_info));
		memset(pinfo, 0x0, sizeof(ptr_info));
		sscanf(line, "%llx,%llx,%llx,%llx,%llx,%llx", &pinfo->va, &pinfo->base, &pinfo->trg, &pinfo->size, &pinfo->value, &pinfo->is_rel);
		ptr_infos[pinfo_cnt] = pinfo;
		pinfo_cnt++;

		line = next_line + 1;
    }
	// some weird file may not end with '\n'
    if (*line)
	{
		ptr_info *pinfo;
		pinfo = (ptr_info *)vmalloc(sizeof(ptr_info));
		memset(pinfo, 0x0, sizeof(ptr_info));
		sscanf(line, "%llx,%llx,%llx,%llx,%llx,%llx", &pinfo->va, &pinfo->base, &pinfo->trg, &pinfo->size, &pinfo->value, &pinfo->is_rel);
		ptr_infos[pinfo_cnt] = pinfo;
		pinfo_cnt++;
		return;
	}

	// int piter;
	// for(piter = 0; piter < pinfo_cnt; piter++)
	// {
	// 	if(ptr_infos[piter]->va == 0x4cb2c)
	// 	{
	// 		kp_pr_info("test.ptr0x4cb2c: %llx %llx %llx\n", ptr_infos[piter]->va, ptr_infos[piter]->trg, ptr_infos[piter]->value);
	// 	}
	// }
}


// this function has side effects, it would change buf
// TODO: implement dep_info parser
void parse_dep_info(char *buf)
{
	char *line = buf;
    char *next_line;


    while ((next_line = strchr(line, '\n')) != NULL) 
	{
		u64 api_addr, fstart, fend;
		APIDep apidep;

		*next_line = '\0'; // Replace newline with null terminator

		sscanf(line, "0x%llx: ", &api_addr);
		line = strchr(line, ':');
		line++;
		init_apidep(&apidep, api_addr);

		while (sscanf(line, "(0x%llx,0x%llx)", &fstart, &fend) == 2) 
		{
			add_dependency(&apidep, fstart, fend);
			// kp_pr_info("(%llx, %llx)", fstart, fend);
			line = strchr(line, ';'); // move to next '}'
			if (!line)
			{
				break;
			}
			line++; // move past ";"
		}

		add_apidep(&kp_deps, &apidep);

        line = next_line + 1;
    }
	// some weird file may not end with '\n'
    if (*line)
	{

	}

	// print_kpdep(&kp_deps);

	return;
}

// legacy usage, just stick to it
void parse_all_dep_info(char *buf)
{
	u64 start, end;
	kp_pr_info("parsing .deps\n");
	while (sscanf(buf, "{0x%llx,0x%llx}", &start, &end) == 2) 
	{
		dep_info *dinfo;
        dinfo = kmalloc(sizeof(dep_info), GFP_KERNEL);
		dinfo->start = start;
		dinfo->end = end;
		// kp_pr_info("dep: %llx %llx\n", start, end);
		dep_infos[dinfo_cnt++] = dinfo;

        buf = strchr(buf, '}'); // move to next '}'
        if (!buf)
		{
			break;
		}
        buf += 2; // move past "},"
    }

}

void parse_clsrange(char *buf)
{
	char *line = buf;
    char *next_line;

    while ((next_line = strchr(line, '\n')) != NULL) 
	{
		cls_info *clsinfo;

		*next_line = '\0'; // Replace newline with null terminator

		clsinfo = (cls_info *)kmalloc(sizeof(cls_info), GFP_KERNEL);
		sscanf(line, "%llx,%llx", &clsinfo->cls_start, &clsinfo->cls_end);
		clsinfo->pgoff = clsinfo->cls_start >> PAGE_SHIFT;
		clsinfo->size = (clsinfo->cls_end - clsinfo->cls_start) >> PAGE_SHIFT;
		cls_infos[clsinfo_cnt++] = clsinfo;

        line = next_line + 1;
    }
	// some weird file may not end with '\n'
    if (*line)
	{
		cls_info *clsinfo;

		clsinfo = (cls_info *)kmalloc(sizeof(cls_info), GFP_KERNEL);
		sscanf(line, "%llx,%llx", &clsinfo->cls_start, &clsinfo->cls_end);
		clsinfo->pgoff = clsinfo->cls_start >> PAGE_SHIFT;
		clsinfo->size = (clsinfo->cls_end - clsinfo->cls_start) >> PAGE_SHIFT;
		cls_infos[clsinfo_cnt++] = clsinfo;
	}

	int i;
	for(i = 0; i < clsinfo_cnt; i++)
	{
		kp_pr_info("%llx %llx\n", cls_infos[i]->pgoff, cls_infos[i]->size);
	}	
}


void parse_sec_info(char *fpath)
{
    loff_t pos;
    Elf64_Ehdr elf_header;
    Elf64_Shdr *section_header;
    char *sh_strtab;
    int sh_iter;
	mm_segment_t oldfs;
	struct file *filp;

	oldfs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open(fpath, O_RDONLY, 0);
    set_fs(oldfs);
    if (IS_ERR(filp)) 
	{
        kp_pr_err("Cannot open file: %s\n", fpath);
        return;
    }

	pos = 0;
    kernel_read(filp, &elf_header, sizeof(Elf64_Ehdr), &pos);

    if (memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0) 
	{
        kp_pr_alert("Invalid ELF file\n");
        return;
    }

    section_header = kmalloc(sizeof(Elf64_Shdr) * elf_header.e_shnum, GFP_KERNEL);
    if (section_header == NULL) 
	{
        kp_pr_alert("Memory allocation error\n");
        return;
    }

    pos = elf_header.e_shoff;
    kernel_read(filp, section_header, sizeof(Elf64_Shdr) * elf_header.e_shnum, &pos);
    sh_strtab = kmalloc(sizeof(char ) * (section_header[elf_header.e_shstrndx].sh_size), GFP_KERNEL);
    if (sh_strtab == NULL) 
	{
        kp_pr_alert("Memory allocation error\n");
        kfree(section_header);
        return;
    }
    pos = section_header[elf_header.e_shstrndx].sh_offset;
    kernel_read(filp, sh_strtab, section_header[elf_header.e_shstrndx].sh_size, &pos);
    for (sh_iter = 0; sh_iter < elf_header.e_shnum; sh_iter++) 
	{
		section_info *si_p;
		char *name;

		si_p = (section_info *)kzalloc(sizeof(section_info), GFP_KERNEL);
        name = sh_strtab + section_header[sh_iter].sh_name;
		strcpy(si_p->name, name);
		si_p->addr = section_header[sh_iter].sh_addr;
		si_p->offset = section_header[sh_iter].sh_offset;
		si_p->size = section_header[sh_iter].sh_size;
		section_infos[secinfo_cnt++] = si_p;

		// kp_pr_info("sec: %s %llx %lx\n", si_p->name, si_p->addr, si_p->size);
    }

    kfree(section_header);
    kfree(sh_strtab);
	filp_close(filp, NULL);
}