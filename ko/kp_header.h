#ifndef KP_HEADER_H
#define KP_HEADER_H


#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched/coredump.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/memremap.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/export.h>
#include <linux/delayacct.h>
#include <linux/init.h>
#include <linux/pfn_t.h>
#include <linux/writeback.h>
#include <linux/memcontrol.h>
#include <linux/mmu_notifier.h>
#include <linux/swapops.h>
#include <linux/elf.h>
#include <linux/gfp.h>
#include <linux/migrate.h>
#include <linux/string.h>
#include <linux/dma-debug.h>
#include <linux/debugfs.h>
#include <linux/userfaultfd_k.h>
#include <linux/dax.h>
#include <linux/oom.h>
#include <linux/numa.h>
#include <asm/io.h>
#include <asm/mmu_context.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>

// for u64
#include <linux/types.h>

// for time calculation
#include <linux/ktime.h>
#include <linux/timekeeping.h>


#include <linux/sched.h>

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include <linux/kallsyms.h> // for kallsyms_lookup_name()

#include <stdarg.h> // for variadic functions, e.g.,kp_pr_info()

#include <linux/hashtable.h>

// FIXME: these ugly macros shoud be fixed...
#define PPT_SIZE 0x1000 // reserved size of page_permutation_table
#define PTR_INFO_NUM 0x10000 
#define CLS_INFO_NUM 0x64
#define SEC_INFO_NUM 0x64
#define DEP_INFO_NUM 0x1000
#define DEP_INFO_PATH "/path/to/libc.dep.offset"
#define PTR_INFO_PATH "/path/to/libc.ptrs"
#define CLS_INFO_PATH "/path/to/libc.clsrange"
#define KP_LIB_PATH "/path/to/libc.so.rewritten"

#define RUNTIME_DEP_RESOLVING // dependency resolving mode
#define KP_RANDOMIZATION // enable fine-grained randomization
// #define KP_DEBUG // comment out this line to disable all kp_pr_xxx()

extern DECLARE_HASHTABLE(dependency_table, 12); // declare the hash table (defined in hash_dep.c)

// each cluster is a randomization unit
typedef struct cls_info
{
	u64 cls_start;
	u64 cls_end;
	u64 size;
	u64 pgoff;
}cls_info;

// records essential info for each section
// .rela.plt .rela.dyn .dynsym .got .got.plt .data.rel.ro .dynamic .init_array .fini_array
typedef struct section_info
{
	char name[0x24]; // I think 0x24 is big enough...
	Elf64_Addr addr;
	Elf64_Off offset;
	Elf64_Xword size;
}section_info;

typedef struct
{
    u64 start;
    u64 end;
} Dependency;

// single apt to its deps
typedef struct
{
    u64 api_address;          
    Dependency *dependencies;  // all dependencies for current api
    size_t count;              
    size_t capacity;           
} APIDep;

// all api to their deps
typedef struct
{
    APIDep *all_deps;          // all api to their deps
    size_t count;              
    size_t capacity;           
} KPDep;

typedef struct dependency_hash_node {
    u64 start;
    u64 end;
    struct hlist_node node;
} runtime_deps;


extern ptr_info **ptr_infos;
extern dep_info **dep_infos;
extern u64 pinfo_cnt;
extern u64 clsinfo_cnt;
extern u64 dinfo_cnt;
extern u64 secinfo_cnt;
extern int **page_permutation_table;
extern cls_info **cls_infos;
extern section_info **section_infos;
extern spinlock_t kp_spinlock;
extern u64 kp_so_base;
extern KPDep kp_deps; // records all dependencies

extern int (*origin_do_mprotect_pkey)(unsigned long start, size_t len,
	unsigned long prot, int pkey);

char *read_file_to_buf(const char *fname);

void parse_ptr_info(char *buf);

void parse_dep_info(char *buf);

void parse_clsrange(char *buf);

void parse_sec_info(char *fpath);

bool addr_in_randomized_area(u64 addr);

u64 transform_addr(u64 old_addr);

u64 get_original_addr(u64 new_addr); // try a reverse mapping

void update_single_ptr_info(ptr_info *pinfo);

void update_all_ptr_info_entries(void);

u64 page_cache_write(u64 to, u64 from, u64 bytes);

void page_purging(u64 addr);

ssize_t read_disk(void *buf, const char *fname, loff_t offset);

void patch_got(void);

void patch_dynsym(void);

void patch_rela_plt(void);

void patch_rela_dyn(void);

void patch_data_rel_ro(void);

void patch_got_plt(void);

void patch_init_array(void);

void patch_fini_array(void);

void patch_dynamic(void);

// void patch_single_ptr(ptr_info *pinfo);

void patch_all_ptrs(void);

u64 find_so_base(char *so_name);

bool addr_in_writable_vma(u64 addr);

void dump_runtime_bin(void);

void write_randomized_layout(void);

pte_t *addr_to_ptep(u64 vaddr, spinlock_t **ptl);

void do_debloating(void);

void parse_all_dep_info(char *buf);

void test_page_cache_write(void);

void dependency_resolving(struct vm_fault* vmf); // resolve all dependencies by traversing .got.plt

void do_randomization(void);

u64 kp_write(u64 to, u64 from, u64 bytes);

void kp_pr_info(const char *fmt, ...);

void kp_pr_alert(const char *fmt, ...);

void kp_pr_err(const char *fmt, ...);

void init_apidep(APIDep *apidep, u64 api_address);

void add_dependency(APIDep *apidep, u64 start, u64 end);

void init_kpdep(KPDep *kpdep);

void add_apidep(KPDep *kpdep, APIDep *apidep);

void print_kpdep(const KPDep *kpdep);

void free_kpdep(KPDep *kpdep);

void add_dependency_to_hash(u64 start, u64 end);

void find_dependencies(const KPDep *kpdep, u64 api_address);

void print_resolved_deps(void);

void write_resolved_deps(void);

void free_dependency_table(void);

#endif