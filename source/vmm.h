#ifndef STRATUS_VMM_H
#define STRATUS_VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Stratus vmm.h
// (c) Connor J. Link. All Rights Reserved.

/* virtual memory constants */
#define VMM_PAGE_SIZE (1U << 12)
#define VMM_PAGE_SHIFT 12
#define VMM_PAGE_MASK (~(VMM_PAGE_SIZE - 1))
#define VMM_PTRS_PER_PAGE (VMM_PAGE_SIZE / sizeof(uint32_t))
#define VMM_PT_LEVELS 2
#define VMM_PAGE_ALIGN(address) (((address) + (VMM_PAGE_SIZE - 1)) & VMM_PAGE_MASK)

#define SATP_MODE_SV32      1U
#define SATP_MODE_SHIFT     31

#define SATP_ASID_BITS      9
#define SATP_ASID_SHIFT     22
#define SATP_ASID_MASK      0x1FFU

#define SATP_PPN_BITS       22
#define SATP_PPN_MASK       0x3FFFFFU

enum
{
    PTE_V_BIT = 1 << 0,
    PTE_R_BIT = 1 << 1,
    PTE_W_BIT = 1 << 2,
    PTE_X_BIT = 1 << 3,
    PTE_U_BIT = 1 << 4,
    PTE_G_BIT = 1 << 5,
    PTE_A_BIT = 1 << 6,
    PTE_D_BIT = 1 << 7,
};
#define PTE_RSW 0x0000FFFF00000000ULL

enum
{
    VMM_PERM_READ_BIT   = 1 << 0,
    VMM_PERM_WRITE_BIT  = 1 << 1,
    VMM_PERM_EXEC_BIT   = 1 << 2,
    VMM_PERM_USER_BIT   = 1 << 3,
    VMM_PERM_GLOBAL_BIT = 1 << 4,
    VMM_PERM_IO_BIT     = 1 << 5, // non-cacheable MMIO region
};


/* type definitions */
typedef uint64_t vaddr_t;
typedef uint64_t paddr_t;
typedef uint64_t ppn_t;
typedef uint64_t pte_t;

typedef enum {
    VMM_SUCCESS = 0,
    VMM_ERR_INVAL_ADDR,
    VMM_ERR_OUT_OF_MEMORY,
    VMM_ERR_PERMISSION_DENIED,
    VMM_ERR_ALREADY_MAPPED
} vmm_result_t;

typedef struct __attribute__((aligned(VMM_PAGE_SIZE))) {
    pte_t entries[VMM_PTRS_PER_PAGE];
} page_table_t;

typedef struct vma_t {
    vaddr_t start;
    vaddr_t end;
    // VMM_PERM_xx flags
    uint16_t perms;
    // backed either by RAM or file/binary for program loading
    bool is_anonymous;
    struct vma_t *next;
} vma_t;

typedef struct vm_space_t {
    page_table_t* root;
    uint64_t asid;
    vma_t* vmas;
    vaddr_t heap_break;
    vaddr_t stack_top;
    uint32_t reference_count;
} vm_space_t;


/* global state */
extern vm_space_t *current_vm_space;
extern vm_space_t kernel_vm_space;
extern uint8_t *phys_mem_start;
extern size_t phys_mem_size;
extern void *free_page_list_head;


/* physical memory */
void vmm_init(void);
void vmm_phys_init(void);
ppn_t vmm_alloc_page(void);
void vmm_free_page(ppn_t ppn);


/* address space context management */
vm_space_t *vmm_create_context(void);
void vmm_destroy_context(vm_space_t *space);
void vmm_switch_context(vm_space_t *space);


/* region mapping */
vmm_result_t vmm_map(vm_space_t *space, vaddr_t va, paddr_t pa, uint16_t perms);
vmm_result_t vmm_unmap(vm_space_t *space, vaddr_t va);
vmm_result_t vmm_map_region(vm_space_t *space, vaddr_t va, size_t size, uint16_t perms);
vmm_result_t vmm_map_io(vm_space_t *space, vaddr_t va, paddr_t pa, size_t size);
vmm_result_t vmm_map_kernel_identity(void);


/* page tables */
pte_t* vmm_get_pte(vm_space_t *space, vaddr_t va, bool create);
paddr_t vmm_translate(vm_space_t *space, vaddr_t va);
vaddr_t vmm_paddr_to_vaddr(paddr_t pa);

static inline pte_t vmm_pte_read(pte_t *pte)
{
    // NOTE: no data cache flush required here only because Horizon does not implement one
    return *pte;
}
static inline void vmm_pte_write(pte_t *pte, pte_t value)
{
    // NOTE: no data cache flush required here only because Horizon does not implement one
    *pte = value;
}


/* userspace memory management */
void *vmm_alloc_region(vm_space_t *space, size_t size, uint16_t perms);
int vmm_load_binary(vm_space_t *space, const void *data, size_t size);


/* page management */
void vmm_handle_page_fault(vaddr_t fault_addr, uint64_t status);
// NOTE: implemented in assembly
void vmm_flush_tlb_asid(void);
void vmm_flush_tlb_asid(uint64_t asid);
void vmm_invalidate_tlb_entry(vaddr_t va);


/* performance counters */
size_t vmm_get_total_pages(void);
size_t vmm_get_free_pages(void);


#endif
