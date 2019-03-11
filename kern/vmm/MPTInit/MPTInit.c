#include "import.h"

/**
 * Initializes the page structures, moves to the kernel page structure (0),
 * and turns on the paging.
 */
void paging_init(unsigned int mbi_addr)
{
    pt_spinlock_init();
    pdir_init_kern(mbi_addr);
    pt_spinlock_acquire();
    set_pdir_base(0);
    enable_paging();
    pt_spinlock_release();
}

void paging_init_ap(void)
{
    pt_spinlock_acquire();
    set_pdir_base(0);
    enable_paging();
    pt_spinlock_release();
}
