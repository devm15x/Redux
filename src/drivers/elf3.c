#include "elf3.h"
#include "paging.h"
#include <stddef.h>
#include <stdint.h>

#define ELF_PROGRAM_MEMORY_SIZE (1024 * 1024)

uint64_t zero_start;
uint64_t zero_count;
const uint8_t *ring3source;
void *p; 
void *z;
static uint8_t program_memory[ELF_PROGRAM_MEMORY_SIZE]
    __attribute__((aligned(4096), section(".program_memory")));

static void elf_copy(
    void *destination,
    const void *source,
    size_t count
)
{
    uint8_t *dest_bytes = destination;
    const uint8_t *source_bytes = source;

    for (size_t i = 0; i < count; i++)
    {
        dest_bytes[i] = source_bytes[i];
    }
}

uint64_t page_start;
uint64_t segment_end;
uint64_t page_end;
uint64_t address;
static void elf_zero(void *destination, size_t count)
{
    uint8_t *bytes = destination;

    for (size_t i = 0; i < count; i++)
    {
        bytes[i] = 0;
    }
}

// 0 = SUCSESS
// 1 = Allocation Error
// 2 = Mapping Error
uint64_t current_page;
uintptr_t page_allocate;
int elf_allocate_pages() {
    current_page = page_start;
    while (current_page < page_end){
        page_allocate = paging_alloc_page();
        if (page_allocate == 0) {
            return 1;
        }
        if (!paging_map_page(current_page, page_allocate, PAGE_USER)) {
            return 2;
        }
        current_page += PAGE_SIZE;
    }
    return 0;
}

static int range_is_valid(
    uint64_t offset,
    uint64_t length,
    uint64_t total_size
)
{
    if (offset > total_size)
    {
        return 0;
    }

    if (length > total_size - offset)
    {
        return 0;
    }

    return 1;
}

static elf_load_info_t elf_failure(elf_load_result_t result)
{
    elf_load_info_t info;

    info.entry = 0;
    info.result = result;

    return info;
}

elf_load_info_t load_elf3_binary(
    const void *file_buffer,
    size_t file_size
)
{   
    if (file_buffer == 0)
    {
        return elf_failure(ELF_LOAD_NULL_BUFFER);
    }
    const uint8_t *bfr = NULL;
    if (file_size < sizeof(elf64_header_t))
    {
        return elf_failure(ELF_LOAD_FILE_TOO_SMALL);
    }

    const uint8_t *file_bytes = file_buffer;
    const elf64_header_t *header =
        (const elf64_header_t *)file_buffer;

    if (header->e_ident[0] != 0x7F ||
        header->e_ident[1] != 'E' ||
        header->e_ident[2] != 'L' ||
        header->e_ident[3] != 'F')
    {
        return elf_failure(ELF_LOAD_BAD_MAGIC);
    }

    if (header->e_ident[4] != ELF_CLASS_64)
    {
        return elf_failure(ELF_LOAD_NOT_64_BIT);
    }

    if (header->e_ident[5] != ELF_DATA_LITTLE)
    {
        return elf_failure(ELF_LOAD_WRONG_ENDIAN);
    }

    if (header->e_machine != ELF_MACHINE_X86_64)
    {
        return elf_failure(ELF_LOAD_WRONG_MACHINE);
    }

    if (header->e_type != ELF_TYPE_EXEC &&
        header->e_type != ELF_TYPE_DYN)
    {
        return elf_failure(ELF_LOAD_WRONG_TYPE);
    }

    if (header->e_ehsize != sizeof(elf64_header_t) ||
        header->e_phentsize != sizeof(elf64_pheader_t))
    {
        return elf_failure(ELF_LOAD_BAD_HEADER_SIZE);
    }

    if (header->e_phnum == 0)
    {
        return elf_failure(ELF_LOAD_BAD_PROGRAM_HEADERS);
    }

    uint64_t program_header_size =
        (uint64_t)header->e_phnum *
        (uint64_t)header->e_phentsize;

    if (!range_is_valid(
            header->e_phoff,
            program_header_size,
            file_size))
    {
        return elf_failure(ELF_LOAD_BAD_PROGRAM_HEADERS);
    }

    uint64_t lowest_address = UINT64_MAX;
    uint64_t highest_address = 0;
    int found_loadable_segment = 0;

    /*
     * First pass:
     * validate loadable segments and determine the address range.
     */
    for (uint16_t i = 0; i < header->e_phnum; i++)
    {
        const elf64_pheader_t *program_header =
            (const elf64_pheader_t *)(
                file_bytes +
                header->e_phoff +
                ((uint64_t)i * header->e_phentsize)
            );

        /*
         * Ignore non-loadable and empty placeholder segments.
         */
        if (program_header->p_type != ELF_PROGRAM_LOAD ||
            program_header->p_memsz == 0)
        {
            continue;
        }

        found_loadable_segment = 1;

        if (program_header->p_filesz >
            program_header->p_memsz)
        {
            return elf_failure(ELF_LOAD_BAD_SEGMENT);
        }

        
        if (!range_is_valid(
                program_header->p_offset,
                program_header->p_filesz,
                file_size))
        {
            return elf_failure(ELF_LOAD_BAD_SEGMENT);
        }
        
        if (program_header->p_vaddr < lowest_address)
        {
            lowest_address = program_header->p_vaddr;
        }
        
        if (program_header->p_memsz >
            UINT64_MAX - program_header->p_vaddr)
        {
            return elf_failure(ELF_LOAD_BAD_SEGMENT);
        }
        segment_end = program_header->p_vaddr + program_header->p_memsz;
        
        if (segment_end > highest_address)
        {
            highest_address = segment_end;
        }
        ring3source = bfr + program_header->p_offset;
    }

    if (!found_loadable_segment ||
        lowest_address == UINT64_MAX ||
        highest_address <= lowest_address)
    {
        return elf_failure(ELF_LOAD_BAD_SEGMENT);
    }

    uint64_t required_memory =
        highest_address - lowest_address;

    if (required_memory > ELF_PROGRAM_MEMORY_SIZE)
    {
        return elf_failure(ELF_LOAD_PROGRAM_TOO_LARGE);
    }


    /*
     * Clear the arena so old program data cannot leak into the next one.
     */

    /*
     * Second pass:
     * copy each loadable segment into the executable arena.
     */
    for (uint16_t i = 0; i < header->e_phnum; i++)
    {
        const elf64_pheader_t *program_header =
            (const elf64_pheader_t *)(
                file_bytes +
                header->e_phoff +
                ((uint64_t)i * header->e_phentsize)
            );

        /*
         * This must match the first pass.
         */
        if (program_header->p_type != ELF_PROGRAM_LOAD ||
            program_header->p_memsz == 0)
        {
            continue;
        }
            address = program_header->p_vaddr;
            page_start = address & ~(PAGE_SIZE - 1);
            segment_end = program_header -> p_vaddr + program_header-> p_memsz;
            page_end = (segment_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            int result = elf_allocate_pages();
            if (result == 1) {
                return elf_failure(ELF_PAGING_BAD);
            }
            else if (result == 2) {
                return elf_failure(ELF_PAGING_BAD);
            }
            p = (void*)(uintptr_t)address; 
            zero_start = address + program_header->p_filesz;
            zero_count = program_header->p_memsz - program_header -> p_filesz;
            z = (void*)(uintptr_t)zero_start; 

        
        uint64_t destination_offset =
            program_header->p_vaddr -
            lowest_address;

        if (destination_offset >= ELF_PROGRAM_MEMORY_SIZE)
        {
            return elf_failure(ELF_LOAD_PROGRAM_TOO_LARGE);
        }
        

        if (program_header->p_memsz >
            ELF_PROGRAM_MEMORY_SIZE - destination_offset)
        {
            return elf_failure(ELF_LOAD_PROGRAM_TOO_LARGE);
        }

        uint8_t *destination =
            p;
        const uint8_t *source =
            file_bytes + program_header->p_offset;

        elf_copy(
            destination,
            source,
            (size_t)program_header->p_filesz
        );

        if (program_header->p_memsz >
            program_header->p_filesz)
        {
            elf_zero(
                destination + program_header->p_filesz,
                (size_t)(
                    program_header->p_memsz -
                    program_header->p_filesz
                )
            );
        }
    }

    if (header->e_entry < lowest_address ||
        header->e_entry >= highest_address)
    {
        return elf_failure(ELF_LOAD_BAD_ENTRY);
    }

    uint64_t entry_offset =
        header->e_entry - lowest_address;

    if (entry_offset >= required_memory ||
        entry_offset >= ELF_PROGRAM_MEMORY_SIZE)
    {
        return elf_failure(ELF_LOAD_BAD_ENTRY);
    }

    elf_load_info_t info;
    void * e = (void*)(uintptr_t)header->e_entry; 
    info.entry = e;
    info.result = ELF_LOAD_OK;

    return info;
}

const char *elf3_load_error_string(elf_load_result_t result)
{
    switch (result)
    {
        case ELF_LOAD_OK:
            return "Success";

        case ELF_LOAD_NULL_BUFFER:
            return "Null file buffer";

        case ELF_LOAD_FILE_TOO_SMALL:
            return "File is too small";

        case ELF_LOAD_BAD_MAGIC:
            return "Invalid ELF magic";

        case ELF_LOAD_NOT_64_BIT:
            return "ELF is not 64-bit";

        case ELF_LOAD_WRONG_ENDIAN:
            return "ELF is not little-endian";

        case ELF_LOAD_WRONG_MACHINE:
            return "ELF is not for x86-64";

        case ELF_LOAD_WRONG_TYPE:
            return "Unsupported ELF type";

        case ELF_LOAD_BAD_HEADER_SIZE:
            return "Invalid ELF header size";

        case ELF_LOAD_BAD_PROGRAM_HEADERS:
            return "Invalid program-header table";

        case ELF_LOAD_BAD_SEGMENT:
            return "Invalid loadable segment";

        case ELF_LOAD_PROGRAM_TOO_LARGE:
            return "Program exceeds the 1 MiB arena";

        case ELF_LOAD_BAD_ENTRY:
            return "Invalid ELF entry point";
        case ELF_PAGING_BAD:
            return "Bad Page!";
        default:
            return "Unknown ELF error";
    }
    
}
