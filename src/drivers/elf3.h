#ifndef REDUX_ELF_H
#define REDUX_ELF_H

#include <stddef.h>
#include <stdint.h>

#define ELF_CLASS_64       2
#define ELF_DATA_LITTLE    1
#define ELF_MACHINE_X86_64 0x3E

#define ELF_TYPE_EXEC      2
#define ELF_TYPE_DYN       3

#define ELF_PROGRAM_LOAD   1

typedef struct
{
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf64_header_t;

typedef struct
{
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} __attribute__((packed)) elf64_pheader_t;

typedef enum
{
    ELF_LOAD_OK = 0,
    ELF_LOAD_NULL_BUFFER,
    ELF_LOAD_FILE_TOO_SMALL,
    ELF_LOAD_BAD_MAGIC,
    ELF_LOAD_NOT_64_BIT,
    ELF_LOAD_WRONG_ENDIAN,
    ELF_LOAD_WRONG_MACHINE,
    ELF_LOAD_WRONG_TYPE,
    ELF_LOAD_BAD_HEADER_SIZE,
    ELF_LOAD_BAD_PROGRAM_HEADERS,
    ELF_LOAD_BAD_SEGMENT,
    ELF_LOAD_PROGRAM_TOO_LARGE,
    ELF_PAGING_BAD,
    ELF_LOAD_BAD_ENTRY
} elf_load_result_t;

typedef struct
{
    void *entry;
    elf_load_result_t result;
} elf_load_info_t;

elf_load_info_t load_elf_binary(
    const void *file_buffer,
    size_t file_size
);

const char *elf_load_error_string(elf_load_result_t result);

#endif
