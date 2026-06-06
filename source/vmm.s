    .globl vmm_flush_tlb
vmm_flush_tlb:
    sfence.vma zero, zero
    ret

    .globl vmm_flush_tlb_asid
vmm_flush_tlb_asid:
    sfence.vma zero, a0
    ret

    .globl vmm_invalidate_tlb_entry
vmm_invalidate_tlb_entry:
    sfence.vma a0, zero
    ret
    