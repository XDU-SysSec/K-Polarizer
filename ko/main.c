// all kp code should include this header
#include "kp_header.h"

extern void (*kp_hook)(struct vm_fault *);

ptr_info **ptr_infos;
dep_info **dep_infos;
u64 pinfo_cnt;
u64 clsinfo_cnt;
u64 dinfo_cnt;
u64 secinfo_cnt;
int **page_permutation_table;
cls_info **cls_infos;
section_info **section_infos;
spinlock_t kp_spinlock;
u64 kp_so_base;
KPDep kp_deps;

int (*origin_do_mprotect_pkey)(unsigned long start, size_t len,
	unsigned long prot, int pkey);


// typedef struct ptr_info
// {
// 	u64 base; // base address (only meaningful for relative pointers)
// 	u64 va, // virtual address
// 	u64 trg, // target address
// 	u64 size, // size of the ptr
// 	u64 value, // original value of the ptr
// 	u64 is_rel // flag to indicate whether this is a relative pointer
// }ptr_info;

// I once considered using filemap_map_pages(), but it cannot trigger major page faults...
static void prefault_all_pages(struct vm_area_struct *first_fault_vma)
{
	const char *so_name;
	struct mm_struct *mm;
	struct vm_area_struct *vma_iter;

	// if(current->prefaults_flag)
	// {
	// 	return;
	// }
	// current->prefaults_flag = true;
	so_name = first_fault_vma->vm_file->f_path.dentry->d_name.name;

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
			unsigned long start = vma_iter->vm_start, end = vma_iter->vm_end, addr_iter = start;
			void *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
			while(addr_iter < end)
			{
				copy_from_user(buf, (void __user *)addr_iter, PAGE_SIZE);
				// if current vma is writable, triggers CoW
				if(addr_in_writable_vma(addr_iter))
				{
					kp_pr_info("triggerring CoW\n");
					copy_to_user((void __user *)addr_iter, buf, PAGE_SIZE);
				}

				addr_iter += PAGE_SIZE;
			}
			kfree(buf);
		}
    }
    up_read(&mm->mmap_sem);
	kp_pr_info("%s's first fault finished, app: %s\n", so_name, current->comm);
}

// ppt refers to page permutation table, which is a n-row, 2-column array (n == # of clusters).
// ppt stores the initial pgoff order (1st column) and randomized pgoff order (2nd column)
// 1st column: the init of 1st column is straightforward: generate a 1-d array according to the pgoff of all randomized pages.
// 2nd column: shuffle all clusters, and init the second column of ppt according to the shuffling result.
// !!NOTE!!: ppt DOESNOT reflect the addr relation directly, the actual address transformation is done in transform_addr()
// I think page_permutation_table is really a bad name, what should we use instead?
static void init_page_permutation_table(cls_info **cls_infos)
{
	int **ppt, ppt_iter, row_iter;
	int *column1, *column2;

	int *origin_order, *shuffled_order;
	int num_pages = 0;

	// init column1, which represents the original pgoff order
	{
		int c1_iter, order_iter;

		column1 = kzalloc(sizeof(int) * PPT_SIZE, GFP_KERNEL);
		origin_order = kmalloc(sizeof(int) * clsinfo_cnt, GFP_KERNEL);
		for(order_iter = 0; order_iter < clsinfo_cnt; order_iter++)
		{
			origin_order[order_iter] = order_iter;
		}

		c1_iter = 0;
		for(order_iter = 0; order_iter < clsinfo_cnt; order_iter++)
		{
			cls_info *ci = cls_infos[origin_order[order_iter]];
			int pgoff_iter;
			for(pgoff_iter = ci->pgoff; pgoff_iter < ci->pgoff + ci->size; pgoff_iter++)
			{
				column1[c1_iter++] = pgoff_iter;
			}
		}
		kp_pr_info("c1 %d\n", c1_iter);
		num_pages = c1_iter;
	}

	// init column2, which represents the shuffled pgoff order
	{
		int c2_iter, order_iter;

		column2 = kzalloc(sizeof(int) * PPT_SIZE, GFP_KERNEL);

		shuffled_order = kmalloc(sizeof(int) * clsinfo_cnt, GFP_KERNEL);
		for(order_iter = 0; order_iter < clsinfo_cnt; order_iter++)
		{
			shuffled_order[order_iter] = order_iter;
		}
		// FIXME: improve the shuffle algorithm
		for(order_iter = 0; order_iter < clsinfo_cnt; order_iter++)
		{
			int swap_idx = (order_iter * 3) % clsinfo_cnt;
			int temp = shuffled_order[order_iter];
			shuffled_order[order_iter] = shuffled_order[swap_idx];
			shuffled_order[swap_idx] = temp;
		}

		c2_iter = 0;
		for(order_iter = 0; order_iter < clsinfo_cnt; order_iter++)
		{
			cls_info *ci = cls_infos[shuffled_order[order_iter]];
			int pgoff_iter;
			for(pgoff_iter = ci->pgoff; pgoff_iter < ci->pgoff + ci->size; pgoff_iter++)
			{
				column2[c2_iter++] = pgoff_iter;
			}
		}
		kp_pr_info("c2 %d\n", c2_iter);
		if(num_pages != c2_iter)
		{
			kp_pr_err("error in init_page_permutation_table: c1.size != c2.size\n");
			kfree(origin_order);
			kfree(shuffled_order);
			kfree(column1);
			kfree(column2);
			return;
		}
	}

	ppt = kzalloc(sizeof(int *) * PPT_SIZE, GFP_KERNEL);
	for(ppt_iter = 0; ppt_iter < PPT_SIZE; ppt_iter++)
	{
		ppt[ppt_iter] = kzalloc(sizeof(int) * 2, GFP_KERNEL);
	}
	for(row_iter = 0; row_iter < num_pages; row_iter++)
	{
		ppt[row_iter][0] = column1[row_iter];
		ppt[row_iter][1] = column2[row_iter];
		// kp_pr_info("%x %x\n", ppt[row_iter][0], ppt[row_iter][1]);
	}

	page_permutation_table = ppt;

	kfree(column1);
	kfree(column2);
	kfree(origin_order);
	kfree(shuffled_order);
}

/*
 * parse all metadata
 */
static void parse_all_infos(void)
{
	char *buf = read_file_to_buf(PTR_INFO_PATH);
	if(buf)
	{
		parse_ptr_info(buf);
		vfree(buf);
	}
	else
	{
		kp_pr_err("cannot read from ptrs\n");
		return;
	}

	buf = read_file_to_buf(DEP_INFO_PATH);
	if(buf)
	{
		parse_dep_info(buf);
		vfree(buf);
	}
	else
	{
		kp_pr_err("cannot read from clsrange\n");
		return;
	}

	buf = read_file_to_buf(CLS_INFO_PATH);
	if(buf)
	{
		parse_clsrange(buf);
		vfree(buf);
	}
	else
	{
		kp_pr_err("cannot read from clsrange\n");
		return;
	}


	parse_sec_info(KP_LIB_PATH);
}

/*
 * main function of K-Polarizer
 * kp_hook is redirected to this function
 */

static void kp_main(struct vm_fault* vmf)
{
	ktime_t t1, t2, t3, t4;
	s64 duration;
	struct vm_area_struct *vma;

	vma = vmf->vma;
	if(vma->vm_file && strcmp(vma->vm_file->f_path.dentry->d_name.name, "libc.so.rewritten") == 0)
	{
		
		if(current->prefaults_flag == false)
		{
			t1 = ktime_get();
			current->prefaults_flag = true;
			// spin_lock(&kp_spinlock);
			kp_so_base = find_so_base("libc.so.rewritten");
			prefault_all_pages(vma);
#ifdef KP_RANDOMIZATION
			do_randomization();
#endif
			// spin_unlock(&kp_spinlock);
			t2 = ktime_get();
			duration = ktime_to_ns(ktime_sub(t2, t1));
			kp_pr_info("%s's randomization took %d ms\n", current->comm, duration / 1000000);
		}	
	}
	
	{
		if(vma->vm_file && current->prefaults_flag == true && current->dep_resolved_flag == false)
		{
			if(strcmp(vma->vm_file->f_path.dentry->d_name.name, current->comm) == 0)
			{
				t3 = ktime_get();
				kp_so_base = find_so_base("libc.so.rewritten");
				dependency_resolving(vmf);
				if(current->dep_resolved_flag == true && current->kp_debloat_flag == false)
				{
					current->kp_debloat_flag = true;
					do_debloating();
					// dump_runtime_bin();
					t4 = ktime_get();
					duration = ktime_to_ns(ktime_sub(t4, t3));
					kp_pr_info("%s's debloating took %d ms\n", current->comm, duration / 1000000);
				}
			}
		}
	}
}

static int __init Kpolarizer_init(void)
{   
	ktime_t t1, t2, t3, t4;
	s64 parse_all_infos_duration;

	kp_hook = kp_main;
    kp_pr_info("K-Polarizer hooks initialized\n");

	// malloc everything...
	pinfo_cnt = 0x0;
	ptr_infos = (ptr_info **)vmalloc(sizeof(ptr_info *) * PTR_INFO_NUM);
	clsinfo_cnt = 0x0;
	cls_infos = (cls_info **)kmalloc(sizeof(cls_info *) * CLS_INFO_NUM, GFP_KERNEL);
	secinfo_cnt = 0x0;
	section_infos = (section_info **)kmalloc(sizeof(section_info *) * SEC_INFO_NUM, GFP_KERNEL);
	dinfo_cnt = 0x0;
	dep_infos = (dep_info **)vmalloc(sizeof(dep_info *) * DEP_INFO_NUM);

	init_kpdep(&kp_deps);

	t1 = ktime_get();
	parse_all_infos();
	t2 = ktime_get();

	parse_all_infos_duration = ktime_to_ns(ktime_sub(t2, t1));
    kp_pr_info("parse_all_infos execution time: %lld ms\n", parse_all_infos_duration / 1000000);

	init_page_permutation_table(cls_infos);

	t3 = ktime_get();
	// update_all_ptr_info_entries();
	t4 = ktime_get();
	kp_pr_info("update_all_ptr_info_entries() took %lld ms\n", ktime_to_ns(ktime_sub(t4, t3)) / 1000000);

	origin_do_mprotect_pkey = (void*)kallsyms_lookup_name("do_mprotect_pkey");
	if(origin_do_mprotect_pkey)
	{
		kp_pr_info("found do_mprotect_pkey\n");
	}
	else
	{
		kp_pr_alert("cannot find do_mprotect_pkey\n");
	}

	return 0;
}

static void __exit Kpolarizer_exit(void)
{
	int ptr_info_iter, cls_info_iter, ppt_iter, sec_info_iter, dep_info_iter;
    kp_hook = NULL;

	// free ptr_infos
	for(ptr_info_iter = 0; ptr_info_iter < PTR_INFO_NUM; ptr_info_iter++)
	{
		vfree(ptr_infos[ptr_info_iter]);
	}
	vfree(ptr_infos);
	pinfo_cnt = 0x0;

	for(cls_info_iter = 0; cls_info_iter < CLS_INFO_NUM; cls_info_iter++)
	{
		kfree(cls_infos[cls_info_iter]);
	}
	kfree(cls_infos);
	clsinfo_cnt = 0x0;

	for(ppt_iter = 0; ppt_iter < PPT_SIZE; ppt_iter++)
	{
		kfree(page_permutation_table[ppt_iter]);
	}
	kfree(page_permutation_table);

	for(sec_info_iter = 0; sec_info_iter < SEC_INFO_NUM; sec_info_iter++)
	{
		kfree(section_infos[sec_info_iter]);
	}
	kfree(section_infos);

	for(dep_info_iter = 0; dep_info_iter < DEP_INFO_NUM; dep_info_iter++)
	{
		kfree(dep_infos[dep_info_iter]);
	}
	vfree(dep_infos);

	free_kpdep(&kp_deps);

	free_dependency_table();

    kp_pr_info("K-Polarizer hooks unbound\n");
}



module_init(Kpolarizer_init);
module_exit(Kpolarizer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("anonymous");
MODULE_DESCRIPTION("K-Polarizer's LKM");