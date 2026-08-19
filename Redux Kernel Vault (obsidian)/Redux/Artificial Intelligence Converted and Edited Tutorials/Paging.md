Your current paging code is already capable of walking Limine's active page tables and inserting new 4 KiB mappings.

The goal now is to upgrade it so you can do this:

```text
Kernel
  |
  +-- allocate physical page
  |
  +-- map it USER
  |
  +-- copy program code
  |
  +-- map USER stack
  |
  +-- IRETQ
          |
          v
       Ring 3
```

We are going to do this in stages.

# 1. Clean up the paging flags

You currently use flags such as:

```c
PAGE_PRESENT
PAGE_WRITABLE
PAGE_USER
```

Put all the important constants in `paging.h`.

For example:

```c
#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stdbool.h>

#include <limine.h>

#define PAGE_SIZE       4096ULL

#define PAGE_PRESENT    (1ULL << 0)
#define PAGE_WRITABLE   (1ULL << 1)
#define PAGE_USER       (1ULL << 2)
#define PAGE_PS         (1ULL << 7)

#define PAGE_ADDRESS_MASK \
    0x000FFFFFFFFFF000ULL

void paging_init(
    struct limine_memmap_response *memmap,
    uint64_t hhdm_offset
);

bool paging_map_page(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    uint64_t flags
);

uintptr_t paging_alloc_page(void);

void *paging_phys_to_virt(
    uintptr_t physical
);

#endif
```

Notice that we are exposing two new functions:

```c
uintptr_t paging_alloc_page(void);
void *paging_phys_to_virt(uintptr_t physical);
```

We will need them when loading userspace programs.

---

# 2. Make `phys_to_virt` public

Right now you have:

```c
static void *phys_to_virt(
    uintptr_t physical
)
```

Change it to:

```c
void *paging_phys_to_virt(
    uintptr_t physical
)
{
    return (void *)(
        physical +
        g_hhdm_offset
    );
}
```

Then replace your existing calls:

```c
phys_to_virt(...)
```

with:

```c
paging_phys_to_virt(...)
```

Why?

Because your future ELF loader will need to do this:

```text
physical page
      |
      v
HHDM virtual pointer
      |
      v
copy ELF bytes into it
```

---

# 3. Turn your table allocator into a general physical page allocator

Currently you have:

```c
static uintptr_t allocate_table_page(void)
```

But user programs also need physical pages.

Rename the underlying allocator to:

```c
uintptr_t paging_alloc_page(void)
```

Use:

```c
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

    /*
     * Always return zero-filled pages.
     */
    for (size_t i = 0;
         i < PAGE_SIZE;
         i++)
    {
        virtual[i] = 0;
    }

    return physical;
}
```

Then your page-table allocator becomes trivial:

```c
static uintptr_t allocate_table_page(void)
{
    return paging_alloc_page();
}
```

Now one allocator can supply:

```text
page tables
user code
user data
user stack
```

For now, this is still a bump allocator.

That is fine.

Later you can replace it with a bitmap PMM without changing the rest of the paging API.

---

# 4. Improve `get_next_table`

Your current version has one dangerous problem.

A page-directory entry can represent a huge page.

If the `PAGE_PS` bit is set, the entry is not pointing at another page table.

Therefore, change your function to:

```c
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
        /*
         * This is a huge-page mapping rather than
         * another page table.
         *
         * We cannot walk through it.
         */
        if (entry & PAGE_PS)
        {
            return NULL;
        }

        /*
         * A user mapping requires USER permission
         * through every level of the page-table walk.
         */
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

    /*
     * No table exists yet.
     * Create one.
     */
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
```

This is especially important for userspace.

---

# 5. Understand the USER bit

This is the single most important rule for Ring 3 paging.

Suppose your final PTE contains:

```text
USER = 1
```

but the PML4 entry contains:

```text
USER = 0
```

Ring 3 still cannot access the page.

You need:

```text
PML4 entry
USER = 1
    |
    v
PDPT entry
USER = 1
    |
    v
PD entry
USER = 1
    |
    v
PT entry
USER = 1
    |
    v
user page
```

Your upgraded `get_next_table()` handles this by doing:

```c
if (flags & PAGE_USER)
{
    table[index] |= PAGE_USER;
}
```

on existing intermediate entries.

Without this, you'd get:

```text
IRETQ
  |
  v
Ring 3
  |
  v
first instruction fetch
  |
  v
#PF 💥
```

---

# 6. Upgrade `paging_map_page`

Your existing function is close.

Use this version:

```c
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
```

Now you can map both kernel and user pages.

---

# 7. Test the mapper before Ring 3

Before attempting user mode, prove that your mapper works.

Pick a temporary virtual address:

```c
#define TEST_ADDRESS 0x400000ULL
```

Then:

```c
uintptr_t physical =
    paging_alloc_page();

if (physical == 0)
{
    /*
     * allocation failed
     */
}
```

Map it:

```c
bool success =
    paging_map_page(
        TEST_ADDRESS,
        physical,
        PAGE_WRITABLE
    );
```

Now write through the virtual address:

```c
volatile uint64_t *test =
    (volatile uint64_t *)
    TEST_ADDRESS;

*test =
    0x123456789ABCDEF0ULL;
```

Then read it:

```c
if (*test ==
    0x123456789ABCDEF0ULL)
{
    /*
     * paging works!
     */
}
```

Do this while still in Ring 0.

If this fails, don't attempt Ring 3 yet.

---

# 8. Map your first USER page

Now make the same mapping user-accessible.

Use:

```c
#define USER_CODE_ADDRESS \
    0x0000000000400000ULL
```

Allocate:

```c
uintptr_t user_code_physical =
    paging_alloc_page();

if (user_code_physical == 0)
{
    /*
     * allocation failed
     */
}
```

Then:

```c
if (!paging_map_page(
        USER_CODE_ADDRESS,
        user_code_physical,
        PAGE_USER |
        PAGE_WRITABLE))
{
    /*
     * mapping failed
     */
}
```

Now you have:

```text
Virtual 0x400000
       |
       v
physical page
       |
       +-- Present
       +-- Writable
       +-- User
```

Ring 3 will be able to access it.

---

# 9. Put a tiny program in the page

For the first test, don't use your ELF loader yet.

Put a tiny sequence of machine code directly into the physical page.

For example:

```asm
.loop:
    pause
    jmp .loop
```

The machine code is:

```text
F3 90 EB FC
```

So:

```c
uint8_t *code =
    (uint8_t *)
    paging_phys_to_virt(
        user_code_physical
    );

code[0] = 0xF3;
code[1] = 0x90;
code[2] = 0xEB;
code[3] = 0xFC;
```

Now address:

```text
0x400000
```

contains a userspace program that loops forever.

That is perfect for the first test because it doesn't depend on:

```text
ELF
syscalls
framebuffer
keyboard
libc
Redux API
```

It just lives.

---

# 10. Map a user stack

Your Ring 3 program needs its own stack.

Let's choose:

```c
#define USER_STACK_TOP \
    0x0000000080000000ULL
```

Allocate:

```c
uintptr_t user_stack_physical =
    paging_alloc_page();
```

Map it one page below the top:

```c
if (!paging_map_page(
        USER_STACK_TOP - PAGE_SIZE,
        user_stack_physical,
        PAGE_USER |
        PAGE_WRITABLE))
{
    /*
     * mapping failed
     */
}
```

The layout becomes:

```text
0x80000000
    ^
    |
 initial RSP

0x7FFFFFFF
    |
    | user stack page
    |
0x7FFFF000
```

The stack grows downward.

So the initial user RSP is:

```c
uint64_t user_rsp =
    USER_STACK_TOP;
```

---

# 11. Add a guard page

Don't map:

```text
0x7FFFE000
```

Then your layout is:

```text
0x80000000
    |
    v
+-------------------+
| USER STACK        |
|                   |
+-------------------+
0x7FFFF000

+-------------------+
| UNMAPPED          |  <- guard page
+-------------------+
0x7FFFE000
```

If userspace blows the stack, you'll get a page fault instead of corrupting something else.

Very useful.

---

# 12. Check the mapping manually

Before `iretq`, test from Ring 0:

```c
volatile uint8_t *user_code =
    (volatile uint8_t *)
    USER_CODE_ADDRESS;

if (user_code[0] != 0xF3)
{
    /*
     * mapping is wrong
     */
}
```

Ring 0 can access user pages too.

This proves:

```text
virtual mapping exists
physical page contains code
page tables are correct enough for kernel access
```

---

# 13. Now connect it to your Ring 3 code

Once your GDT contains:

```text
0x08 kernel code
0x10 kernel data

0x1B user code
0x23 user data

0x28 TSS
```

and your TSS has:

```c
tss.rsp0 =
    kernel_stack_top;
```

you can do:

```c
jump_usermode(
    USER_CODE_ADDRESS,
    USER_STACK_TOP
);
```

Your assembly builds:

```text
SS      0x23
RSP     0x80000000
RFLAGS  0x202
CS      0x1B
RIP     0x400000
```

then:

```asm
iretq
```

The CPU should begin executing:

```text
F3 90 EB FC
```

at CPL 3.

---

# 14. How do you know it worked?

Attach GDB and interrupt execution.

Inspect:

```gdb
info registers cs ss rip rsp
```

You want something like:

```text
cs   = 0x1b
ss   = 0x23
rip  = 0x400000
rsp  ≈ 0x80000000
```

The important part is:

```text
CS & 3 = 3
```

because:

```text
0x1B & 3
=
3
```

🎉

That means the processor is executing Ring 3 code.

---

# 15. Add a page-fault debugger BEFORE doing this

Do not go into Ring 3 without a useful page-fault handler.

Add:

```c
static uint64_t read_cr2(void)
{
    uint64_t value;

    __asm__ volatile(
        "mov %%cr2, %0"
        : "=r"(value)
    );

    return value;
}
```

When exception 14 happens:

```c
uint64_t fault_address =
    read_cr2();
```

Print:

```text
PAGE FAULT
CR2 = ...
ERROR = ...
```

The error code tells you why.

Relevant bits:

```text
Bit 0
0 = not present
1 = protection violation

Bit 1
0 = read
1 = write

Bit 2
0 = supervisor
1 = user

Bit 3
reserved-bit violation

Bit 4
instruction fetch
```

For example:

```text
error = 0x4
```

means roughly:

```text
user-mode access
to a non-present page
```

While:

```text
error = 0x5
```

means:

```text
user-mode access
caused a protection violation
```

This is insanely useful when bringing up Ring 3.

---

# 16. Upgrade your physical allocator later

Your current allocator takes the first Limine region:

```c
if (end > start)
{
    next_free_page =
        start;

    free_region_end =
        end;

    break;
}
```

This means:

```text
Region 1
████████████

Region 2
████████████████████████████

Region 3
████████
```

You only use:

```text
Region 1
```

and completely ignore the others.

For getting Ring 3 working:

**that's okay.**

Later replace it with:

```text
bitmap PMM
```

where every physical page has a bit:

```text
0 = free
1 = used
```

Then:

```c
paging_alloc_page()
```

can find any free page in RAM rather than consuming one region linearly.

But don't derail Ring 3 work to build that right now.

---

# 17. Upgrade the ELF loader

Once your tiny four-byte user program works, bring back your ELF loader.

Your current ELF loader probably does something conceptually like:

```text
PT_LOAD
   |
   v
copy into kernel arena
```

For real userspace, change it to:

```text
PT_LOAD
   |
   +-- determine virtual range
   |
   +-- allocate physical pages
   |
   +-- map PAGE_USER
   |
   +-- copy ELF data through HHDM
   |
   +-- zero BSS
   |
   v
user virtual memory
```

For every page touched by a loadable segment:

```c
uintptr_t physical =
    paging_alloc_page();

paging_map_page(
    virtual_page,
    physical,
    PAGE_USER |
    appropriate_flags
);
```

---

# 18. ELF permissions

An ELF `PT_LOAD` segment has flags:

```text
PF_R
PF_W
PF_X
```

Eventually map them appropriately.

### Executable code

```text
USER
PRESENT
not writable
executable
```

### Writable data

```text
USER
PRESENT
WRITABLE
NX
```

### Stack

```text
USER
PRESENT
WRITABLE
NX
```

At first, though, you can keep user code writable.

Security perfection can wait until:

```text
"holy crap we're actually in Ring 3"
```

has happened. 😭

---

# 19. Add NX later

The x86-64 page-table NX bit is:

```c
#define PAGE_NO_EXECUTE \
    (1ULL << 63)
```

But don't start using it until you've checked and enabled NX support appropriately through EFER.

So initially:

```c
#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_WRITABLE  (1ULL << 1)
#define PAGE_USER      (1ULL << 2)
```

is enough.

---

# 20. Your upgraded `paging.c`

At this stage, your file should roughly have:

```text
paging.c

g_hhdm_offset
next_free_page
free_region_end

paging_phys_to_virt()
read_cr3()

paging_alloc_page()

allocate_table_page()

get_next_table()

paging_init()

paging_map_page()
```

The important conceptual change is:

```text
OLD

allocate_table_page()
      |
      v
only paging structures


NEW

paging_alloc_page()
      |
      +---- page tables
      |
      +---- user code
      |
      +---- user data
      |
      +---- user stacks
```

That prepares your pager to become an actual memory subsystem.

---

# 21. First complete user-mode test

Once paging + GDT + TSS are ready:

```c
#define USER_CODE_ADDRESS \
    0x400000ULL

#define USER_STACK_TOP \
    0x80000000ULL
```

Allocate code:

```c
uintptr_t code_phys =
    paging_alloc_page();
```

Map code:

```c
paging_map_page(
    USER_CODE_ADDRESS,
    code_phys,
    PAGE_USER |
    PAGE_WRITABLE
);
```

Install tiny program:

```c
uint8_t *code =
    paging_phys_to_virt(
        code_phys
    );

code[0] = 0xF3;
code[1] = 0x90;
code[2] = 0xEB;
code[3] = 0xFC;
```

Allocate stack:

```c
uintptr_t stack_phys =
    paging_alloc_page();
```

Map stack:

```c
paging_map_page(
    USER_STACK_TOP -
        PAGE_SIZE,
    stack_phys,
    PAGE_USER |
    PAGE_WRITABLE
);
```

Then:

```c
jump_usermode(
    USER_CODE_ADDRESS,
    USER_STACK_TOP
);
```

Expected journey:

```text
Redux kernel
CPL 0

    |
    | GDT ready
    | TSS ready
    | user page ready
    | user stack ready
    |
    v

IRETQ

    |
    v

RIP = 0x400000
CS  = 0x1B
SS  = 0x23
CPL = 3

    |
    v

PAUSE
JMP
PAUSE
JMP
PAUSE
JMP

forever
```

If GDB shows:

```text
CS = 0x1B
```

you have officially crossed the border into **real x86-64 userspace**.

# 22. What comes after that?

Do not immediately give Ring 3 your current `redux_api` kernel function pointers.

Real userspace should not do:

```c
api->println(...)
```

where that pointer directly enters arbitrary kernel C code.

Instead, your architecture becomes:

```text
HELLO.elf
   |
   | syscall
   v
Redux kernel
   |
   +-- PRINT
   +-- READ KEY
   +-- FILE OPEN
   +-- FILE READ
   +-- FILE WRITE
```

So after you've proven Ring 3 works, your next major project is:

```text
SYSCALL
+
SYSRETQ
```

Then your current Redux API can evolve from:

```text
table of kernel function pointers
```

into:

```text
userspace wrapper functions
        |
        v
     syscall
        |
        v
kernel syscall dispatcher
```

And _that_ is when Redux goes from “kernel that can load programs” to “OS with an actual protected userspace.” 💻🔐