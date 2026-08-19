The end goal of writing a kernel is to get to **userspace**, or, in other words, going from ring 0 to ring 3.

While one might expect that ring 3 GDT entries would be sufficient, it is more complicated. All of the following tasks must be completed:

- Add two new GDT entries, at least, configured for ring 3.
    
    - These entries are needed for the user's code and data segments, one each.
        
- Set up a barebones **64-bit TSS** with an `RSP0` stack.
    
    - When an interrupt, exception, or other privilege transition occurs while the CPU is in user mode, the CPU needs to know where the ring 0 kernel stack is located.
        
    - In 64-bit mode this location is stored in the `RSP0` entry of the TSS.
        
- Ensure the user program and user stack are mapped as **user-accessible pages**.
    
- Set up an IDT entry or `SYSCALL` mechanism for ring 3 system calls, optional for initially entering ring 3.
    

Unlike 32-bit x86, long mode does not support hardware task switching through the TSS. The TSS is still required for privilege-level stack switching and can also provide Interrupt Stack Table entries.

## Contents

1. Requirements
    
2. GDT
    
3. The TSS
    
4. Entering Ring 3
    
    1. `iretq` method
        
    2. `sysexit` method
        
    3. `sysretq` method
        
5. Multitasking considerations
    

# Requirements

- A working x86-64 long-mode kernel
    
- Ring 0 GDT and IDT
    
- IRQ and exception handling
    
- Paging
    
- A kernel stack
    
- Plans for software multitasking or task switching
    

# GDT

Following is an example of an ordinary 8-byte GDT entry structure in C, utilizing bit fields:

```c
struct gdt_entry_bits {
    unsigned int limit_low              : 16;
    unsigned int base_low               : 24;
    unsigned int accessed               : 1;
    unsigned int read_write             : 1;
    unsigned int conforming_expand_down : 1;
    unsigned int code                   : 1;
    unsigned int code_data_segment      : 1;
    unsigned int DPL                    : 2;
    unsigned int present                : 1;
    unsigned int limit_high             : 4;
    unsigned int available              : 1;
    unsigned int long_mode              : 1;
    unsigned int big                    : 1;
    unsigned int gran                   : 1;
    unsigned int base_high              : 8;
} __attribute__((packed));

typedef struct gdt_entry_bits gdt_entry_bits;
```

This structure is still eight bytes and can therefore represent normal code and data descriptors.

However, **a 64-bit TSS descriptor is 16 bytes**, so it cannot be represented by one of these entries. We will deal with the TSS separately.

A simple GDT layout is:

```text
Index    Selector        Description

0        0x00            Null
1        0x08            Ring 0 code
2        0x10            Ring 0 data
3        0x18            Ring 3 code
4        0x20            Ring 3 data
5        0x28            TSS, lower half
6        0x30            TSS, upper half
```

The user selectors will have their bottom two RPL bits set:

```text
User code: 0x18 | 3 = 0x1B
User data: 0x20 | 3 = 0x23
```

Using the structure above, the ring 3 segments can be initialized as follows:

```c
static gdt_entry_bits gdt[7];

/*
 * gdt[0] = null
 * gdt[1] = ring 0 code
 * gdt[2] = ring 0 data
 */

gdt_entry_bits *ring3_code = &gdt[3];
gdt_entry_bits *ring3_data = &gdt[4];

ring3_code->limit_low = 0xFFFF;
ring3_code->base_low = 0;

ring3_code->accessed = 0;
ring3_code->read_write = 1;
ring3_code->conforming_expand_down = 0;

ring3_code->code = 1;
ring3_code->code_data_segment = 1;

ring3_code->DPL = 3;
ring3_code->present = 1;

ring3_code->limit_high = 0xF;
ring3_code->available = 0;

/*
 * This is the important x86-64 change.
 *
 * L = 1 selects 64-bit code.
 * D/B must be 0 when L = 1.
 */
ring3_code->long_mode = 1;
ring3_code->big = 0;

ring3_code->gran = 1;
ring3_code->base_high = 0;


/*
 * Start with a copy because most descriptor attributes are similar.
 */
*ring3_data = *ring3_code;

ring3_data->code = 0;

/*
 * The L bit applies to code segments, not normal data segments.
 */
ring3_data->long_mode = 0;

/*
 * There is no need for a 32-bit stack/data segment in 64-bit mode.
 */
ring3_data->big = 0;
```

The important difference from a 32-bit tutorial is:

```c
ring3_code->long_mode = 1;
ring3_code->big = 0;
```

not:

```c
ring3_code->long_mode = 0;
ring3_code->big = 1;
```

The latter creates a compatibility-mode 32-bit code segment rather than a 64-bit userspace code segment.

Long mode mostly ignores the traditional base and limit of code/data segments, but the descriptor privilege level and code-segment attributes still matter.

In actuality, the CPU can be placed into user mode once suitable ring 3 segments and user-accessible page mappings exist. However, without a TSS, returning to ring 0 through an interrupt or exception cannot safely switch to an appropriate kernel stack.

That is where the TSS comes in.

# The TSS

The TSS historically supported hardware multitasking.

**x86-64 long mode does not support hardware task switching.**

Instead, an operating system normally implements task switching in software.

The TSS still has several important jobs in long mode:

- It contains `RSP0`, `RSP1`, and `RSP2`.
    
- It contains the Interrupt Stack Table, or IST.
    
- It can contain an I/O permission bitmap.
    

For getting to ring 3, the field we care about most is:

```c
RSP0
```

`RSP0` tells the processor which stack pointer to use when it needs a ring 0 stack during a privilege transition.

## 64-bit TSS structure

The 64-bit TSS looks like this:

```c
struct tss_entry_struct {
    uint32_t reserved0;

    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;

    uint64_t reserved1;

    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;

    uint64_t reserved2;

    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;
```

Notice what is **not** present:

```c
esp0
ss0
eip
eax
ebx
...
```

Those belonged to the legacy task-state format.

For our purposes, most of the x86-64 TSS can initially remain zero.

Create one:

```c
static tss_entry_t tss_entry;
```

You will also need a kernel stack.

For example:

```c
#define KERNEL_STACK_SIZE 16384

static uint8_t kernel_stack[KERNEL_STACK_SIZE]
    __attribute__((aligned(16)));
```

The top of this stack is:

```c
(uint64_t)(uintptr_t)(
    kernel_stack + KERNEL_STACK_SIZE
)
```

We place that address in `RSP0`.

```c
void init_tss(void)
{
    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.rsp0 =
        (uint64_t)(uintptr_t)(
            kernel_stack + KERNEL_STACK_SIZE
        );

    /*
     * Put the I/O bitmap beyond the end of the TSS.
     * This means we are not supplying an I/O permission bitmap.
     */
    tss_entry.iomap_base =
        sizeof(tss_entry);
}
```

Unlike the old 32-bit TSS, there is no `SS0` field to initialize here.

## 64-bit TSS descriptor

The TSS descriptor is different from ordinary GDT entries.

In long mode a TSS descriptor is **16 bytes**.

Therefore:

```text
gdt[5]
gdt[6]
```

together form one TSS descriptor.

A convenient structure is:

```c
struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  limit_high_flags;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

typedef struct tss_descriptor tss_descriptor_t;
```

This structure is exactly 16 bytes.

Now create the TSS descriptor:

```c
void write_tss(tss_descriptor_t *g)
{
    uint64_t base =
        (uint64_t)(uintptr_t)&tss_entry;

    uint32_t limit =
        sizeof(tss_entry) - 1;

    memset(g, 0, sizeof(*g));

    g->limit_low =
        limit & 0xFFFF;

    g->base_low =
        base & 0xFFFF;

    g->base_middle =
        (base >> 16) & 0xFF;

    /*
     * 0x89:
     *
     * bit 7    = present
     * DPL      = 0
     * S        = 0, system descriptor
     * type 9   = available 64-bit TSS
     */
    g->access = 0x89;

    g->limit_high_flags =
        (limit >> 16) & 0x0F;

    g->base_high =
        (base >> 24) & 0xFF;

    g->base_upper =
        (base >> 32) & 0xFFFFFFFF;

    g->reserved = 0;
}
```

If your GDT is stored as seven 8-byte entries:

```c
static uint64_t gdt[7];
```

you can write the TSS descriptor starting at entry five:

```c
init_tss();

write_tss(
    (tss_descriptor_t *)&gdt[5]
);
```

This writes all 16 bytes:

```text
gdt[5]  first half
gdt[6]  second half
```

After constructing the **entire GDT**, load it with `lgdt`.

Only after the new GDT has been loaded should the task register be loaded.

## flush_tss

The implementation of `flush_tss` in Intel/NASM syntax is:

```asm
; C declaration:
; void flush_tss(void);

bits 64

global flush_tss

flush_tss:
    mov ax, (5 * 8) | 0
    ltr ax
    ret
```

Entry five has selector:

```text
5 * 8 = 40 = 0x28
```

so this is equivalent to:

```asm
flush_tss:
    mov ax, 0x28
    ltr ax
    ret
```

The TSS descriptor's DPL is **not** what determines which ring the user process executes in.

The privilege level of executing code is determined primarily by `CS` and its associated descriptor.

At this point the kernel is ready to enter ring 3.

# User-accessible paging

There is one additional requirement that is unavoidable in x86-64: **paging**.

Long mode requires paging, and user code must reside in pages accessible from CPL 3.

For a normal 4 KiB page-table entry:

```c
#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE   (1ULL << 1)
#define PAGE_USER    (1ULL << 2)
```

A user code page should have at least:

```c
PAGE_PRESENT | PAGE_USER
```

A writable user data or stack page should have:

```c
PAGE_PRESENT | PAGE_WRITE | PAGE_USER
```

The `USER` bit must also permit user access through the relevant upper paging structures.

For ordinary four-level paging:

```text
PML4
  |
  | U/S = 1
  v
PDPT
  |
  | U/S = 1
  v
PD
  |
  | U/S = 1
  v
PT
  |
  | U/S = 1
  v
User page
```

If the final page-table entry is marked user-accessible but an upper paging entry forbids user access, CPL 3 code will receive a page fault when trying to access the page.

You will need at least:

- a user-accessible executable code page
    
- a user-accessible writable stack
    

For example, suppose the user stack occupies:

```text
0x000000007FFFF000
```

through:

```text
0x000000007FFFFFFF
```

Then its initial stack pointer could be:

```text
0x0000000080000000
```

and the page below that address should be mapped:

```text
Present = 1
Writable = 1
User = 1
```

# Entering Ring 3

The x86 is a tricky CPU. There is no ordinary instruction equivalent to:

```text
set_ring(3);
```

Instead, we perform a controlled privilege-level transition.

Several techniques exist.

## iretq method

One of the simplest ways to initially enter ring 3 is to make the processor believe it is returning from an interrupt that originally occurred in ring 3.

In 64-bit mode, we use:

```asm
iretq
```

rather than the 32-bit:

```asm
iret
```

First, declare a function:

```c
extern void jump_usermode(
    uint64_t entry,
    uint64_t user_stack
);
```

Under the System V AMD64 calling convention:

```text
RDI = first argument
RSI = second argument
```

So:

```text
RDI = user RIP
RSI = user RSP
```

The assembly is:

```asm
bits 64

global jump_usermode

jump_usermode:
    ;
    ; RDI = address of user code
    ; RSI = top of user stack
    ;

    ;
    ; Load user data selectors.
    ;
    mov ax, (4 * 8) | 3

    mov ds, ax
    mov es, ax

    ;
    ; FS and GS do not need to be loaded this way
    ; unless your kernel specifically uses segment selectors
    ; for them.
    ;

    ;
    ; Set up the stack frame IRETQ expects when
    ; returning to a less-privileged level.
    ;

    ;
    ; User SS
    ;
    push (4 * 8) | 3

    ;
    ; User RSP
    ;
    push rsi

    ;
    ; RFLAGS
    ;
    pushfq
    pop rax

    ;
    ; Make sure the reserved bit is set.
    ;
    or rax, 0x2

    ;
    ; Enable interrupts in userspace if desired.
    ;
    or rax, 0x200

    push rax

    ;
    ; User CS
    ;
    push (3 * 8) | 3

    ;
    ; User RIP
    ;
    push rdi

    ;
    ; Beam me to ring 3.
    ;
    iretq
```

With the GDT layout used earlier:

```text
(3 * 8) | 3 = 0x1B
(4 * 8) | 3 = 0x23
```

So this can also be written as:

```asm
bits 64

global jump_usermode

jump_usermode:
    mov ax, 0x23
    mov ds, ax
    mov es, ax

    push 0x23
    push rsi

    pushfq
    pop rax
    or rax, 0x202
    push rax

    push 0x1B
    push rdi

    iretq
```

The `iretq` frame looks conceptually like:

```text
Higher addresses

+---------------------+
| SS = 0x23           |
+---------------------+
| User RSP            |
+---------------------+
| RFLAGS              |
+---------------------+
| CS = 0x1B           |
+---------------------+
| User RIP            | <- current RSP
+---------------------+

Lower addresses
```

`iretq` pops this state and enters the user code at CPL 3.

A C call could look like:

```c
jump_usermode(
    (uint64_t)(uintptr_t)test_user_function,
    USER_STACK_TOP
);
```

The address of `test_user_function` must be mapped as user-accessible.

The user stack must also be mapped as user-accessible.

Once `iretq` succeeds, execution begins in ring 3.

A very simple test function could be:

```c
__attribute__((noreturn))
void test_user_function(void)
{
    while (1) {
        __asm__ volatile ("pause");
    }
}
```

If you have a working exception handler, another useful test is deliberately executing a privileged instruction:

```c
__attribute__((noreturn))
void test_user_function(void)
{
    __asm__ volatile ("cli");

    while (1) {
    }
}
```

`CLI` is privileged, so executing it from CPL 3 should produce a General Protection exception rather than successfully disabling interrupts.

If your exception output reports that fault, congratulations: **your code actually reached ring 3**.

## sysexit method

The original 32-bit version of this tutorial uses `SYSENTER`/`SYSEXIT` and the `IA32_SYSENTER_CS` MSR to initially jump into userspace.

For a pure x86-64 kernel, this is generally not the method you want for initial userspace entry.

The long-mode-native fast system-call pair is:

```text
SYSCALL
SYSRET
```

Therefore, for this x86-64 version of the tutorial, use either:

```text
IRETQ
```

for the initial transition, or configure:

```text
SYSRETQ
```

after the relevant system-call MSRs have been initialized.

The `iretq` method is generally much easier to get working first.

## sysret method

`SYSRET` is specifically useful in x86-64 when paired with `SYSCALL`.

Unlike `iretq`, `SYSRETQ` does not read an interrupt-return frame from the stack.

Instead, it takes important return information from registers.

In 64-bit mode:

```text
RCX  = user RIP
R11  = user RFLAGS
```

Before using `SYSCALL` and `SYSRET`, the kernel must configure the appropriate model-specific registers.

Important MSRs include:

```c
#define IA32_EFER   0xC0000080
#define IA32_STAR   0xC0000081
#define IA32_LSTAR  0xC0000082
#define IA32_FMASK  0xC0000084
```

`EFER.SCE` enables the `SYSCALL`/`SYSRET` extensions.

For example:

```c
static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t low;
    uint32_t high;

    __asm__ volatile (
        "rdmsr"
        : "=a"(low),
          "=d"(high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(
    uint32_t msr,
    uint64_t value
) {
    uint32_t low =
        value & 0xFFFFFFFF;

    uint32_t high =
        value >> 32;

    __asm__ volatile (
        "wrmsr"
        :
        : "c"(msr),
          "a"(low),
          "d"(high)
    );
}
```

Enable `SYSCALL`/`SYSRET`:

```c
uint64_t efer =
    rdmsr(IA32_EFER);

efer |= 1;

wrmsr(
    IA32_EFER,
    efer
);
```

The `STAR` MSR controls the code-segment bases used by `SYSCALL` and `SYSRET`.

With the GDT layout from this tutorial, the selector layout must be chosen carefully because `SYSRET` derives its selectors rather than accepting arbitrary `CS` and `SS` values from a stack frame.

For this reason, **do not use `SYSRETQ` as your first ring 3 transition unless your GDT layout has been designed around the `SYSCALL`/`SYSRET` selector rules**.

A much better progression is:

```text
IRETQ
  ↓
prove Ring 3 works
  ↓
implement SYSCALL entry
  ↓
implement SYSRETQ
```

Once configured correctly, the final return itself is approximately:

```asm
; RCX = user RIP
; R11 = user RFLAGS
; RSP = user RSP must have been restored appropriately

sysretq
```

Unlike the 32-bit example, simply writing arbitrary values to `IA32_STAR` and issuing `SYSRETQ` is not enough to build a safe syscall mechanism. The kernel must also preserve its own stack and restore the user's stack during system-call return.

# Returning to Ring 0

Once userspace is running, an interrupt or exception may occur.

Imagine this state:

```text
CPL = 3
RSP = user stack
```

The interrupt gate points to ring 0 kernel code.

If the privilege level changes from ring 3 to ring 0, the processor needs a ring 0 stack.

This is where:

```c
tss_entry.rsp0
```

is used.

Conceptually:

```text
            RING 3

       User code running
               |
               |
          interrupt
               |
               v
      CPU needs Ring 0 RSP
               |
               v
         read TSS.RSP0
               |
               v

            RING 0

       Kernel stack active
               |
               v
       interrupt handler
```

This is the long-mode equivalent of the old tutorial's use of `ESP0`.

# Multitasking considerations

There are many subtle aspects of user mode and task switching.

Suppose task A is running in ring 3.

Its user stack might be:

```text
Task A user RSP
```

When an interrupt causes a privilege transition to ring 0, the processor switches to:

```c
tss_entry.rsp0
```

That stack therefore needs to belong to the kernel context for the currently running task.

Now imagine switching to task B.

If task B later enters the kernel but `RSP0` still points to task A's kernel stack, the two tasks can overwrite one another's kernel state.

Therefore, each task should normally have its own kernel stack.

For example:

```c
struct task {
    uint64_t user_rsp;
    uint64_t kernel_stack_top;

    /*
     * Other task state...
     */
};
```

When switching to another task, update:

```c
void set_kernel_stack(uint64_t stack)
{
    tss_entry.rsp0 = stack;
}
```

So a context switch might conceptually do:

```c
void switch_to_task(struct task *next)
{
    set_kernel_stack(
        next->kernel_stack_top
    );

    /*
     * Switch page tables, registers,
     * user context, etc...
     */
}
```

Then:

```text
Task A
  |
  +-- user stack A
  |
  +-- kernel stack A
            ^
            |
          RSP0


context switch


Task B
  |
  +-- user stack B
  |
  +-- kernel stack B
            ^
            |
          RSP0
```

This prevents task B's kernel entry from overwriting task A's kernel stack.

The 64-bit TSS is therefore **not performing task switching**.

Your scheduler performs the task switch in software.

The TSS merely contains information the processor needs during certain privilege transitions.

# Summary

To get to ring 3 in x86-64 long mode:

```text
1. Create Ring 0 code/data GDT descriptors

2. Create Ring 3 code/data GDT descriptors
      Ring 3 code:
          DPL = 3
          L   = 1
          D/B = 0

3. Create a 64-bit TSS
      RSP0 = kernel stack top

4. Create its 16-byte TSS descriptor
      GDT entries 5 + 6

5. Load the GDT
      LGDT

6. Reload segment registers

7. Load the TSS
      LTR 0x28

8. Map user code pages
      Present = 1
      User    = 1

9. Map a user stack
      Present = 1
      Write   = 1
      User    = 1

10. Construct an IRETQ frame
      SS
      RSP
      RFLAGS
      CS
      RIP

11. Execute IRETQ

12. Welcome to Ring 3.
```

Once this works, the next step is implementing a controlled way for userspace to request kernel services, normally through `SYSCALL`/`SYSRET` in a 64-bit operating system.