#include "kp_header.h"

DEFINE_HASHTABLE(dependency_table, 12);

void add_dependency_to_hash(u64 start, u64 end)
{
    struct dependency_hash_node *entry;
    hash_for_each_possible(dependency_table, entry, node, start)
    {
        if (entry->start == start && entry->end == end)
            return;
    }

    entry = kmalloc(sizeof(struct dependency_hash_node), GFP_KERNEL);
    if (!entry)
    {
        kp_pr_err("Failed to allocate memory for hash entry\n");
        return;
    }

    entry->start = start;
    entry->end = end;
    hash_add(dependency_table, &entry->node, start);
}

// given api_address, find and append all its dependencies to dependency_table
void find_dependencies(const KPDep *kpdep, u64 api_address)
{
    size_t j;

    for (j = 0; j < kpdep->count; j++)
    {
        if (kpdep->all_deps[j].api_address == api_address)
        {
            size_t k;
            for (k = 0; k < kpdep->all_deps[j].count; k++)
            {
                add_dependency_to_hash(kpdep->all_deps[j].dependencies[k].start, kpdep->all_deps[j].dependencies[k].end);
            }
            break;
        }
    }
}

void print_resolved_deps(void)
{
    u64 iter;
    struct dependency_hash_node *entry;

    kp_pr_info("Unique dependencies:\n");
    hash_for_each(dependency_table, iter, entry, node)
    {
        kp_pr_info("Dependency: (0x%llx, 0x%llx)\n", entry->start, entry->end);
    }
}

void write_resolved_deps()
{
    u64 iter;
    struct dependency_hash_node *entry;
    struct file *filp;
    loff_t pos;
    mm_segment_t old_fs;

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    filp = filp_open("/path/you/choose", O_WRONLY|O_CREAT, 0644);
    set_fs(old_fs);

    if (IS_ERR(filp)) 
	{
        kp_pr_err("Cannot create resolved_offsets\n");
        return;
    }
    set_fs(old_fs);

    pos = 0;
    hash_for_each(dependency_table, iter, entry, node)
    {
        char string[0x32];
        sprintf(string, "{0x%llx, 0x%llx},", entry->start, entry->end);
        kernel_write(filp, string, strlen(string), &pos);
    }

    filp_close(filp, NULL);
}


void free_dependency_table(void) 
{
    runtime_deps *entry;
    struct hlist_node *tmp;
    int bkt;

    kp_pr_info("free runtime_deps\n");

    hash_for_each_safe(dependency_table, bkt, tmp, entry, node) 
    {
        hash_del(&entry->node);
        kfree(entry);            
    }
}