#include "kp_header.h"


// init APIDep
void init_apidep(APIDep *apidep, u64 api_address)
{
    apidep->api_address = api_address;
    apidep->dependencies = kmalloc(sizeof(Dependency), GFP_KERNEL);  // init to 1
    if (!apidep->dependencies)
    {
        kp_pr_err("Failed to allocate memory for dependencies\n");
        return;
    }
    apidep->count = 0;
    apidep->capacity = 1;
}

// 
static void ensure_apidep_capacity(APIDep *apidep)
{
    if (apidep->count >= apidep->capacity)
    {
        size_t new_capacity = apidep->capacity * 2;
        Dependency *new_dependencies = krealloc(apidep->dependencies, new_capacity * sizeof(Dependency), GFP_KERNEL);
        if (!new_dependencies)
        {
            kp_pr_err("Failed to reallocate memory for dependencies\n");
            return;
        }
        apidep->dependencies = new_dependencies;
        apidep->capacity = new_capacity;
    }
}

// add (start, end) to apidep
void add_dependency(APIDep *apidep, u64 start, u64 end)
{
    ensure_apidep_capacity(apidep);  //
    apidep->dependencies[apidep->count].start = start;
    apidep->dependencies[apidep->count].end = end;
    apidep->count++;
}

// init KPDep
void init_kpdep(KPDep *kpdep)
{
    kpdep->all_deps = kmalloc(sizeof(APIDep), GFP_KERNEL);  // init to 1
    if (!kpdep->all_deps)
    {
        kp_pr_err("Failed to allocate memory for all_deps\n");
        return;
    }
    kpdep->count = 0;
    kpdep->capacity = 1;
}

// 
static void ensure_kpdep_capacity(KPDep *kpdep)
{
    if (kpdep->count >= kpdep->capacity)
    {
        size_t new_capacity = kpdep->capacity * 2;
        APIDep *new_all_deps = krealloc(kpdep->all_deps, new_capacity * sizeof(APIDep), GFP_KERNEL);
        if (!new_all_deps)
        {
            kp_pr_err("Failed to reallocate memory for all_deps\n");
            return;
        }
        kpdep->all_deps = new_all_deps;
        kpdep->capacity = new_capacity;
    }
}

// add apidep to kpdep
void add_apidep(KPDep *kpdep, APIDep *apidep)
{
    ensure_kpdep_capacity(kpdep);  //
    kpdep->all_deps[kpdep->count] = *apidep;
    kpdep->count++;
}

void print_kpdep(const KPDep *kpdep)
{
    size_t i;
    kp_pr_info("KPDep contains %zu APIs\n", kpdep->count);
    for (i = 0; i < kpdep->count; i++)
    {
        size_t j;
        kp_pr_info("API Address: 0x%llx\n", kpdep->all_deps[i].api_address);
        for (j = 0; j < kpdep->all_deps[i].count; j++)
        {
            kp_pr_info("  Dependency %zu: (0x%llx, 0x%llx)\n", j + 1, kpdep->all_deps[i].dependencies[j].start, kpdep->all_deps[i].dependencies[j].end);
        }
    }
}


void free_kpdep(KPDep *kpdep)
{
    size_t i;
    for (i = 0; i < kpdep->count; i++)
    {
        kfree(kpdep->all_deps[i].dependencies);  //
    }
    kfree(kpdep->all_deps);
    kpdep->all_deps = NULL;
    kpdep->count = 0;
    kpdep->capacity = 0;
}