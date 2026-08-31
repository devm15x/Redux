// please kill me
// ram sucks
#include "paging.h"
#include "debug.h"
#include <stdint.h>
#include <stddef.h>

static uint64_t g_hhdm_offset = 0;

static uintptr_t next_free_page = 0;
static uintptr_t free_region_end = 0;
#define TEST_ADDRESS 0x0000000040000000ULL

void *paging_phys_to_virt(
    uintptr_t physical
)
{
    return (void *)(
        physical +
        g_hhdm_offset
    );
}

static uintptr_t read_cr3(void)
{
    uintptr_t value;

    __asm__ volatile(
        "mov %%cr3, %0"
        : "=r"(value)
    );

    return value;
}

uintptr_t paging_alloc_page(void)
{
    if (next_free_page == 0 ||
        next_free_page + PAGE_SIZE >
        free_region_end)
    {
        return 0;
    }

    uintptr_t physical =
        next_free_page;

    next_free_page += PAGE_SIZE;

    uint8_t *virtual =
        (uint8_t *)
        paging_phys_to_virt(
            physical
        );

    for (size_t i = 0;
         i < PAGE_SIZE;
         i++)
    {
        virtual[i] = 0;
    }

    return physical;
}

static uintptr_t allocate_table_page(void)
{
    return paging_alloc_page();
}


static uint64_t *get_next_table(
    uint64_t *table,
    size_t index,
    uint64_t flags
)
{
    uint64_t entry =
        table[index];

    if (entry & PAGE_PRESENT)
    {

        if (entry & PAGE_PS)
        {
            return NULL;
        }


        if (flags & PAGE_USER)
        {
            table[index] |=
                PAGE_USER;

            entry =
                table[index];
        }

        uintptr_t physical =
            entry &
            PAGE_ADDRESS_MASK;

        return
            (uint64_t *)
            paging_phys_to_virt(
                physical
            );
    }

    uintptr_t new_table =
        allocate_table_page();

    if (new_table == 0)
    {
        return NULL;
    }

    uint64_t table_flags =
        PAGE_PRESENT |
        PAGE_WRITABLE;

    if (flags & PAGE_USER)
    {
        table_flags |=
            PAGE_USER;
    }

    table[index] =
        new_table |
        table_flags;

    return
        (uint64_t *)
        paging_phys_to_virt(
            new_table
        );
}

void paging_init(
    struct limine_memmap_response *memmap,
    uint64_t hhdm_offset
)
{
    g_hhdm_offset =
        hhdm_offset;

    next_free_page = 0;
    free_region_end = 0;

    if (memmap == NULL)
    {
        return;
    }

    for (uint64_t i = 0;
         i < memmap->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            memmap->entries[i];

        if (entry->type !=
            LIMINE_MEMMAP_USABLE)
        {
            continue;
        }

        uintptr_t start =
            (entry->base + 4095)
            & ~((uintptr_t)4095);

        uintptr_t end =
            (entry->base +
             entry->length)
            & ~((uintptr_t)4095);

        if (end > start)
        {
            next_free_page =
                start;

            free_region_end =
                end;

            break;
        }
    }
}

bool paging_map_page(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    uint64_t flags
)
{
    virtual_address &=
        ~(PAGE_SIZE - 1);

    physical_address &=
        ~(PAGE_SIZE - 1);

    uintptr_t cr3 =
        read_cr3();

    uintptr_t pml4_physical =
        cr3 &
        PAGE_ADDRESS_MASK;

    uint64_t *pml4 =
        (uint64_t *)
        paging_phys_to_virt(
            pml4_physical
        );

    size_t pml4_index =
        (virtual_address >> 39) &
        0x1FF;

    size_t pdpt_index =
        (virtual_address >> 30) &
        0x1FF;

    size_t pd_index =
        (virtual_address >> 21) &
        0x1FF;

    size_t pt_index =
        (virtual_address >> 12) &
        0x1FF;

    uint64_t intermediate_flags =
        0;

    if (flags & PAGE_USER)
    {
        intermediate_flags |=
            PAGE_USER;
    }

    uint64_t *pdpt =
        get_next_table(
            pml4,
            pml4_index,
            intermediate_flags
        );

    if (pdpt == NULL)
    {
        return false;
    }

    uint64_t *pd =
        get_next_table(
            pdpt,
            pdpt_index,
            intermediate_flags
        );

    if (pd == NULL)
    {
        return false;
    }

    uint64_t *pt =
        get_next_table(
            pd,
            pd_index,
            intermediate_flags
        );

    if (pt == NULL)
    {
        return false;
    }

    pt[pt_index] =
        (physical_address &
         PAGE_ADDRESS_MASK)
        |
        flags
        |
        PAGE_PRESENT;

    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_address)
        : "memory"
    );

    return true;
}

bool paging_test(void)
{
    qemu_debug_print("[PAGING] Starting USER page test...\n");

    uintptr_t physical =
        paging_alloc_page();

    if (physical == 0)
    {
        qemu_debug_print("[PAGING] FAIL: allocation failed\n");
        return false;
    }

    if (!paging_map_page(
            TEST_ADDRESS,
            physical,
            PAGE_USER |
            PAGE_WRITABLE))
    {
        qemu_debug_print("[PAGING] FAIL: user mapping failed\n");
        return false;
    }

    qemu_debug_print("[PAGING] User page mapped\n");

    volatile uint64_t *test =
        (volatile uint64_t *)
        TEST_ADDRESS;

    const uint64_t test_value =
        0xCAFEBABEDEADBEEFULL;

    /*
     * Ring 0 is allowed to access user pages,
     * so this tests the mapping without entering Ring 3 yet.
     */
    *test = test_value;

    if (*test != test_value)
    {
        qemu_debug_print("[PAGING] FAIL: virtual readback mismatch\n");
        return false;
    }

    /*
     * Check the same physical page through the HHDM.
     */
    volatile uint64_t *physical_view =
        (volatile uint64_t *)
        paging_phys_to_virt(
            physical
        );

    if (*physical_view != test_value)
    {
        qemu_debug_print("[PAGING] FAIL: physical readback mismatch\n");
        return false;
    }

    qemu_debug_print("[PAGING] USER PAGE SUCCESS!\n");

    return true;
}