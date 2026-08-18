#include "paging.h"

#include <stdint.h>
#include <stddef.h>

static uint64_t g_hhdm_offset = 0;

static uintptr_t next_free_page = 0;
static uintptr_t free_region_end = 0;

static void *phys_to_virt(
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

static uintptr_t allocate_table_page(void)
{
    if (next_free_page == 0 ||
        next_free_page + 4096 >
        free_region_end)
    {
        return 0;
    }

    uintptr_t physical =
        next_free_page;

    next_free_page += 4096;

    uint64_t *virtual =
        (uint64_t *)
        phys_to_virt(
            physical
        );

    for (size_t i = 0;
         i < 512;
         i++)
    {
        virtual[i] = 0;
    }

    return physical;
}

static uint64_t *get_next_table(
    uint64_t *table,
    size_t index,
    uint64_t flags
)
{
    uint64_t entry =
        table[index];

    if (!(entry & PAGE_PRESENT))
    {
        uintptr_t new_table =
            allocate_table_page();

        if (new_table == 0)
        {
            return NULL;
        }

        table[index] =
            new_table |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            flags;

        entry =
            table[index];
    }
    else if (flags & PAGE_USER)
    {
        table[index] |=
            PAGE_USER;

        entry =
            table[index];
    }

    uintptr_t physical =
        entry &
        0x000FFFFFFFFFF000ULL;

    return
        (uint64_t *)
        phys_to_virt(
            physical
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
        ~((uintptr_t)0xFFF);

    physical_address &=
        ~((uintptr_t)0xFFF);

    uintptr_t cr3 =
        read_cr3();

    uintptr_t pml4_physical =
        cr3 &
        0x000FFFFFFFFFF000ULL;

    uint64_t *pml4 =
        (uint64_t *)
        phys_to_virt(
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

    uint64_t intermediate_flags = 0;

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
        physical_address |
        flags |
        PAGE_PRESENT;

    __asm__ volatile(
        "invlpg (%0)"
        :
        : "r"(virtual_address)
        : "memory"
    );

    return true;
}