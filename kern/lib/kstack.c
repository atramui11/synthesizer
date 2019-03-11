#include <lib/x86.h>
#include <lib/seg.h>

#include "kstack.h"

uintptr_t *get_kstack_pointer(void)
{
    return (uintptr_t *) ROUNDDOWN(read_esp(), KSTACK_SIZE);
}

int get_kstack_cpu_idx(void)
{
    struct kstack *ks = (struct kstack *) get_kstack_pointer();
    return ks->cpu_idx;
}
