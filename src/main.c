// The Redux Kernel 
// by devm15
// Copyright (C) devm15 2026 
// Have fun
// from the local idiot XD
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "limine.h"

#include "program3.h"
#include "psf.h"
#include "terminal.h"
#include "drivers/ascii.h"
#include "drivers/keyboard.h"
#include "drivers/ata.h"
#include "shell.h"
#include "drivers/ff.h"
#include "program0.h"
#include "idt.h"
#include "gdt.h"
#include "apic.h"
#include "panic.h"
#include "paging.h"
#include "clock.h"
#include "acpi.h"
#include "userspace.h"
#include "syscall.h"
#define USER_CODE_ADDRESS 0x0000000040000000ULL
#define USER_STACK_TOP    0x0000000080000000ULL

//here are some attributes, now go have fun and leave me alone
__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] =
    LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] =
    LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0
};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] =
    LIMINE_REQUESTS_END_MARKER;


static uint64_t get_usable_ram_kb(void)
{
    if (memmap_request.response == NULL)
    {
        return 0;
    }

    uint64_t usable_bytes = 0;

    for (uint64_t i = 0;
         i < memmap_request.response->entry_count;
         i++)
    {
        struct limine_memmap_entry *entry =
            memmap_request.response->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE)
        {
            usable_bytes += entry->length;
        }
    }

    return usable_bytes / 1024;
}

static void halt(void)
{
    for (;;)
    {
        asm volatile("hlt");
    }
}

static FATFS g_filesystem;
//psst
//if you think redux is horribly written
//i tell you this
//get it done, fix it up, if it works, screw it
//now get lost
void kmain(void)
{
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
    {
        halt();
    }

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1)
    {
        halt();
    }

    struct limine_framebuffer *framebuffer =
        framebuffer_request.response->framebuffers[0];

    terminal_initialize(framebuffer);
    psf_init(framebuffer);
    terminal_clear(framebuffer, 0x00081A33);

    println("Redux early boot.");
    println("Terminal OK.");

    __asm__ volatile("cli");

    println("Loading GDT...");
    gdt_init();
    println("GDT OK.");

    println("Loading IDT...");
    idt_init();
    println("IDT OK.");

    println("Checking APIC...");

    if (!check_apic())
    {
        panic("APIC not supported.");
    }

    println("APIC supported.");

    if (rsdp_request.response == NULL ||
        rsdp_request.response->address == NULL)
    {
        panic("ACPI RSDP not found.");
    }

    if (hhdm_request.response == NULL)
    {
        panic("HHDM not available.");
    }
    paging_init(
        memmap_request.response,
        hhdm_request.response->offset
    );

    uintptr_t lapic_phys =
        cpu_get_apic_base();

    uintptr_t lapic_virt =
        hhdm_request.response->offset +
        lapic_phys;

if (!paging_map_page(
        lapic_virt,
        lapic_phys,
        PAGE_WRITABLE |
        PAGE_PCD
    ))
{
    panic("Could not map LAPIC.");
}
    void *rsdp =
        rsdp_request.response->address;

    uint64_t hhdm_offset =
        hhdm_request.response->offset;




    print("LAPIC physical base: ");
    print_uint64((uint64_t)lapic_phys);
    println("");

    print("LAPIC virtual base: ");
    print_uint64((uint64_t)lapic_virt);
    println("");

    apic_set_virtual_base(
        lapic_virt
    );

    (void)rsdp;

    println("Enabling Local APIC...");

    if (!enable_apic())
    {
        panic("Local APIC initialization failed.");
    }

    println("Local APIC enabled.");

    println("Initializing ACPI...");

    if (!acpi_init(
    rsdp,
    hhdm_offset
    ))
    {
        panic("Could not initialize ACPI PM timer.");
    }
    println("ACPI PM timer found.");
println("Calibrating Local APIC timer...");

uint32_t ticks_per_ms =
    calibrate_lapic_timer();

if (ticks_per_ms == 0)
{
    panic("LAPIC timer calibration failed.");
}

print("LAPIC ticks/ms: ");
print_uint64(ticks_per_ms);
println("");

uint32_t cursor_ticks =
    redux_clock_ms_to_ticks(
        500
    );

    if (cursor_ticks == 0)
    {
        panic("Could not calculate cursor timer.");
    }

    print("LAPIC ticks/500ms: ");
    print_uint64(cursor_ticks);
    println("");

    init_one_shot_clock(
        0x40
    );

    redux_clock_set_reload(
        cursor_ticks
    );

    arm_redux_clock(
        cursor_ticks
    );

    paging_test();
    syscall_init(); 
    usermode_test();

    __asm__ volatile("sti");
    terminal_clear(
        framebuffer,
        0x00081A33
    );

    terminal_set_cursor(0, 0);

    println("Redux Kernel v0.1.0 Milestone 1");
    terminal_putchar('\n');

    println("Copyright (C) 2026-present devm15");
    println("Licensed under the MIT License.");

    print("Available Memory: ");
    print_uint64(get_usable_ram_kb());
    println(" KB");

    ata_initialize();

    println("Initializing storage...");

    FRESULT result =
        f_mount(
            &g_filesystem,
            "0:",
            1
        );

    if (result == FR_OK)
    {
        println("Filesystem mounted.");
    }
    else
    {
        print("Mount failed. Error ");
        print_uint64(result);
        println("");
    }

    program_initialize(framebuffer);
    program_initialize3(framebuffer);
    shell_init(framebuffer);
    while (1)
    {
        shell_update();
        terminal_cursor_update();
    }
}

//Here is the code I had used to test kernel panics for the first time.
// 15:21 @ 17/08/2026
//volatile int zero = 0;
//volatile int result = 123 / zero;

//(void)result;