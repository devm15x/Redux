OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

PHDRS
{
    limine_requests PT_LOAD FLAGS(6);
    text            PT_LOAD FLAGS(5);
    rodata          PT_LOAD FLAGS(4);
    data            PT_LOAD FLAGS(6);
    program         PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 0xffffffff80000000;

    .limine_requests :
    {
        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :limine_requests

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .text :
    {
        *(.text .text.*)
    } :text

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .rodata :
    {
        *(.rodata .rodata.*)
    } :rodata

    .note.gnu.build-id :
    {
        *(.note.gnu.build-id)
    } :rodata

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .data :
    {
        *(.data .data.*)
    } :data

    .bss :
    {
        *(.bss .bss.*)
        *(COMMON)
    } :data

    . = ALIGN(CONSTANT(MAXPAGESIZE));

    .program_memory :
    {
        *(.program_memory)
    } :program

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.note .note.*)
    }
}