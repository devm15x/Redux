#ifndef REDUX_PAGING_H
#define REDUX_PAGING_H

#include <stdint.h>
#include <stdbool.h>
#include <limine.h>

#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_WRITABLE  (1ULL << 1)
#define PAGE_USER      (1ULL << 2)
#define PAGE_PWT       (1ULL << 3)
#define PAGE_PCD       (1ULL << 4)
#define PAGE_NX        (1ULL << 63)

void paging_init(
    struct limine_memmap_response *memmap,
    uint64_t hhdm_offset
);

bool paging_map_page(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    uint64_t flags
);

#endif