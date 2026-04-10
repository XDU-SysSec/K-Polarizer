#include "kp_header.h"

pte_t *addr_to_ptep(u64 vaddr, spinlock_t **ptlp)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;


    pgd = pgd_offset(current->mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) 
    {
        kp_pr_alert("Invalid PGD\n");
        return NULL;
    }


    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) 
    {
        kp_pr_alert("Invalid P4D\n");
        return NULL;
    }


    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud)) 
    {
        kp_pr_alert("Invalid PUD\n");
        return NULL;
    }


    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) 
    {
        kp_pr_alert("Invalid PMD\n");
        return NULL;
    }

    // use spinlock to prevent concurrent modification, 
    // do not release ptlp here, it should be relased by the caller
    ptep = pte_offset_map_lock(current->mm, pmd, vaddr, ptlp);
    if (pte_none(*ptep)) 
    {
        kp_pr_alert("Invalid PTE\n");
        pte_unmap_unlock(ptep, *ptlp);
        return NULL;
    }

    return ptep;
}