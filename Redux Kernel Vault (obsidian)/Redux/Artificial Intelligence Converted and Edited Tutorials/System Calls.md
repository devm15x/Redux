# System Calls in Redux

System calls are how a normal user program asks the Redux kernel to do something privileged.

A user program runs in:

    Ring 3

The kernel runs in:

    Ring 0

Ring 3 code cannot directly access kernel memory, hardware, page tables, or other privileged resources.

So instead, a program does something like:

    print("Hello!")

and the userspace library eventually performs:

    syscall

The CPU then enters the Redux kernel.

The basic idea is:

    User program
        |
        | syscall
        v
    Redux kernel
        |
        | handle request
        v
    Return to user program

For Redux, the easiest proper x86-64 method is:

    SYSCALL / SYSRETQ

---

# 1. What a Redux syscall looks like

Suppose a user program wants Redux to print some text.

We might define:

    syscall 0 = print
    syscall 1 = clear
    syscall 2 = get key
    syscall 3 = put pixel

The user program puts the syscall number in:

    RAX

and arguments in registers.

For example:

    RAX = syscall number
    RDI = argument 1
    RSI = argument 2
    RDX = argument 3
    R10 = argument 4
    R8  = argument 5
    R9  = argument 6

So printing could look like:

    RAX = 0
    RDI = address of string

then:

    syscall

Redux sees:

    RAX = 0

and knows:

    "The program wants SYS_PRINT."

---

# 2. Why use SYSCALL?

You could technically use:

    int 0x80

like old 32-bit operating systems did.

That still works in long mode if you configure the IDT properly.

But x86-64 has a special instruction specifically for system calls:

    syscall

and a matching return instruction:

    sysretq

For Redux, this is the better system to learn.

The flow becomes:

    Ring 3
      |
      | syscall
      v
    Ring 0
      |
      | Redux syscall handler
      v
    Ring 3

---

# 3. Setting up SYSCALL

Before userspace can execute:

    syscall

Redux has to tell the CPU where the syscall handler is.

This is done using special CPU registers called MSRs.

The important ones are:

    IA32_EFER   = 0xC0000080
    IA32_STAR   = 0xC0000081
    IA32_LSTAR  = 0xC0000082
    IA32_FMASK  = 0xC0000084

You do not access these like normal RAM.

You use:

    rdmsr
    wrmsr

instructions.

---

# 4. Enable SYSCALL

Inside:

    IA32_EFER

there is a bit called:

    SCE

which means:

    System Call Extensions

It is bit 0.

Redux needs to enable it.

For example:

    uint64_t efer = rdmsr(0xC0000080);

    efer |= 1;

    wrmsr(0xC0000080, efer);

After this, the CPU allows:

    syscall
    sysretq

---

# 5. Tell the CPU where Redux's syscall handler is

Redux needs an assembly function such as:

    syscall_entry

For example:

    global syscall_entry

    syscall_entry:
        ; syscall code goes here

Then Redux writes its address into:

    IA32_LSTAR

Like this:

    wrmsr(
        0xC0000082,
        (uint64_t)syscall_entry
    );

Now when userspace executes:

    syscall

the CPU jumps to:

    syscall_entry

inside Redux.

---

# 6. What SYSCALL automatically saves

When userspace executes:

    syscall

the CPU saves two important things.

It puts the userspace instruction pointer into:

    RCX

So:

    RCX = old user RIP

It also saves the userspace flags into:

    R11

So:

    R11 = old user RFLAGS

This means Redux must remember:

    RCX is special
    R11 is special

Do not treat them like normal syscall argument registers.

That is why we use:

    R10

for argument 4 instead of RCX.

---

# 7. The really important part: RSP does NOT change

This is probably the most important thing to understand about x86-64 SYSCALL.

When Redux receives an interrupt from Ring 3, the CPU can switch to:

    TSS.RSP0

automatically.

But:

    syscall

does NOT do that.

Imagine the user program has:

    RSP = user stack

Then it executes:

    syscall

The CPU enters Ring 0...

but:

    RSP = STILL THE USER STACK

😬

Redux must not start using that stack as its kernel stack.

Do not immediately do:

    push rax
    push rbx
    call syscall_handler

because those pushes would still be going onto user memory.

Redux needs to save the user RSP and switch to its own kernel stack first.

---

# 8. Redux needs a kernel stack

Each running program should eventually have a kernel stack.

For example:

    user stack

    0x00007FFF........
           |
           | syscall
           v

    Redux syscall entry
           |
           | switch RSP
           v

    kernel stack

The kernel stack might be stored in some per-CPU or per-task structure.

Conceptually:

    struct cpu_data {
        uint64_t user_rsp;
        uint64_t kernel_rsp;
    };

Redux can then save:

    current user RSP

and replace it with:

    kernel RSP

---

# 9. SWAPGS

x86-64 provides:

    swapgs

This is useful for accessing per-CPU kernel information.

Redux could store things like:

    user_rsp
    kernel_rsp
    current_process

in a CPU-local structure.

Then syscall entry can begin something like:

    syscall_entry:
        swapgs

        mov [gs:user_rsp], rsp
        mov rsp, [gs:kernel_rsp]

Now:

    RSP = Redux kernel stack

and Redux can safely start pushing registers.

---

# 10. Saving registers

Now Redux should save whatever register state it needs.

For example:

    push r11
    push rcx

    push rax
    push rdi
    push rsi
    push rdx
    push r10
    push r8
    push r9

At this point the kernel has enough information to handle the syscall and eventually return to userspace.

Conceptually:

    kernel stack

    +----------------+
    | user RFLAGS    | <- R11
    +----------------+
    | user RIP       | <- RCX
    +----------------+
    | RAX            |
    +----------------+
    | RDI            |
    +----------------+
    | RSI            |
    +----------------+
    | RDX            |
    +----------------+
    | R10            |
    +----------------+
    | R8             |
    +----------------+
    | R9             |
    +----------------+

---

# 11. Calling Redux C code

The assembly syscall entry should normally be very small.

Its job is mostly:

    enter kernel
    switch stacks
    save registers
    call C
    restore registers
    return

The actual syscall dispatcher can be written in C.

For example:

    uint64_t syscall_handler(
        uint64_t number,
        uint64_t arg1,
        uint64_t arg2,
        uint64_t arg3
    )
    {
        switch (number) {
            case 0:
                // print
                break;

            case 1:
                // clear
                break;

            default:
                return -1;
        }

        return 0;
    }

The assembly handler can pass:

    RAX -> syscall number
    RDI -> argument 1
    RSI -> argument 2
    RDX -> argument 3

to the Redux dispatcher.

You must also keep normal x86-64 C stack alignment correct before using:

    call

---

# 12. A simple Redux syscall ABI

Redux needs to decide how programs communicate with the kernel.

A simple ABI could be:

    RAX = syscall number

    RDI = argument 1
    RSI = argument 2
    RDX = argument 3
    R10 = argument 4
    R8  = argument 5
    R9  = argument 6

    RAX = return value

For example:

## SYS_PRINT

    RAX = 0
    RDI = pointer to string

## SYS_CLEAR

    RAX = 1

## SYS_GET_KEY

    RAX = 2

Return:

    RAX = key value

## SYS_PUT_PIXEL

    RAX = 3
    RDI = x
    RSI = y
    RDX = color

This fits Redux quite nicely because these are already the kinds of services that Redux programs need.

---

# 13. Dispatching syscalls

The easiest first version is a switch statement.

For example:

    uint64_t syscall_dispatch(
        uint64_t number,
        uint64_t a1,
        uint64_t a2,
        uint64_t a3
    )
    {
        switch (number) {

            case SYS_PRINT:
                kernel_print((const char *)a1);
                return 0;

            case SYS_CLEAR:
                clear();
                return 0;

            case SYS_GET_KEY:
                return keyboard_get_scancode();

            case SYS_PUT_PIXEL:
                put_pixel(
                    a1,
                    a2,
                    a3
                );
                return 0;

            default:
                return (uint64_t)-1;
        }
    }

Later Redux could use a syscall table instead.

For example:

    syscall_table[RAX]

Because function pointers are 64-bit, an assembly table lookup would use:

    syscall_table + RAX * 8

not:

    syscall_table + EAX * 4

---

# 14. Returning a result

The simplest rule is:

    RAX = syscall return value

For example:

    SYS_GET_KEY

could return:

    RAX = keyboard scancode

A failed syscall could return:

    RAX = -1

or Redux could eventually define proper error numbers.

---

# 15. Returning to userspace

Before returning, Redux restores the saved user state.

Eventually we need:

    RCX = user RIP
    R11 = user RFLAGS
    RSP = user RSP

Then:

    swapgs
    sysretq

Conceptually:

    Redux kernel
        |
        | restore user RSP
        | restore RCX
        | restore R11
        |
        | sysretq
        v
    User program

`SYSRETQ` uses:

    RCX -> RIP
    R11 -> RFLAGS

to return to userspace.

---

# 16. A simplified syscall entry

A very simplified handler looks like this:

    syscall_entry:
        swapgs

        ; Save userspace stack.
        mov [gs:user_rsp], rsp

        ; Switch to Redux kernel stack.
        mov rsp, [gs:kernel_rsp]

        ; Save return information.
        push r11
        push rcx

        ; Save syscall registers.
        push rdi
        push rsi
        push rdx
        push r10
        push r8
        push r9

        ; RAX contains syscall number.

        call syscall_dispatch

        ; RAX now contains return value.

        pop r9
        pop r8
        pop r10
        pop rdx
        pop rsi
        pop rdi

        pop rcx
        pop r11

        ; Restore userspace stack.
        mov rsp, [gs:user_rsp]

        swapgs

        sysretq

This is intentionally simplified.

A real Redux implementation must also handle:

    stack alignment
    interrupts
    invalid user addresses
    canonical addresses
    page faults
    scheduling
    context switches
    register preservation

But this shows the actual flow.

---

# 17. Userspace side

Redux programs should not need to manually write syscall assembly everywhere.

Instead, Redux can provide a small userspace library.

For example:

    static inline uint64_t redux_syscall1(
        uint64_t number,
        uint64_t arg1
    )
    {
        uint64_t result;

        asm volatile (
            "syscall"
            : "=a"(result)
            : "a"(number),
              "D"(arg1)
            : "rcx", "r11", "memory"
        );

        return result;
    }

Then a userspace print function could become:

    void redux_print(const char *text)
    {
        redux_syscall1(
            SYS_PRINT,
            (uint64_t)text
        );
    }

And programs simply do:

    redux_print("Hello from Ring 3!");

rather than knowing how `SYSCALL` itself works.

---

# 18. User pointers are dangerous

Suppose userspace calls:

    SYS_PRINT

with:

    RDI = pointer to string

Redux must remember:

    RDI came from Ring 3.

Userspace can put ANY number there.

For example:

    RDI = 0

or:

    RDI = 0xFFFFFFFFFFFFFFFF

or:

    RDI = address of kernel memory

Redux must never blindly trust a pointer supplied by userspace.

Eventually Redux should have functions such as:

    copy_from_user()
    copy_to_user()

These functions verify that a user pointer refers to memory that the process is allowed to access.

For example:

    SYS_PRINT
        |
        v
    verify string pointer
        |
        +-- bad -> return error
        |
        +-- good
             |
             v
         print string

This is extremely important.

Ring 3 is not automatically safe just because the CPU has privilege levels.

The kernel still has to check everything Ring 3 gives it.

---

# 19. What about the Redux API structure?

Redux currently has an API structure containing function pointers such as:

    print
    println
    putchar
    clear
    keyboard_get_scancode
    scancode_to_ascii
    put_pixel

That is useful while programs are effectively being given kernel-provided function pointers.

But once Redux has real Ring 3 isolation, user programs should not directly call arbitrary kernel function addresses.

Instead:

    OLD IDEA

    user program
        |
        | function pointer
        v
    kernel function


becomes:

    RING 3 REDUX

    user program
        |
        | Redux userspace API
        v
    syscall
        |
        v
    Redux syscall dispatcher
        |
        v
    kernel function

For example:

    api->println("Hello");

could eventually call a userspace wrapper that does:

    SYS_PRINTLN

rather than directly jumping into a Ring 0 function.

That means Redux can keep a nice API for programs while putting a syscall layer underneath it.

The application doesn't have to care.

---

# 20. Redux syscall layout

A good first Redux syscall table could be:

    0  SYS_PRINT
    1  SYS_PRINTLN
    2  SYS_PUTCHAR
    3  SYS_CLEAR
    4  SYS_GET_SCANCODE
    5  SYS_SCANCODE_TO_ASCII
    6  SYS_PUT_PIXEL
    7  SYS_FRAMEBUFFER_WIDTH
    8  SYS_FRAMEBUFFER_HEIGHT
    9  SYS_EXIT

Later you could add things such as:

    SYS_OPEN
    SYS_READ
    SYS_WRITE
    SYS_CLOSE

when Redux gets proper filesystem access from userspace.

---

# 21. The whole thing

The finished Redux design looks like this:

    +------------------------------+
    |        Redux Program         |
    |            Ring 3            |
    +------------------------------+
                  |
                  | redux_print(...)
                  v
    +------------------------------+
    |      Redux userspace API     |
    +------------------------------+
                  |
                  | RAX = SYS_PRINT
                  | RDI = string
                  | syscall
                  v
    +------------------------------+
    |       syscall_entry          |
    |            Ring 0            |
    +------------------------------+
                  |
                  | save user RSP
                  | load kernel RSP
                  | save registers
                  v
    +------------------------------+
    |      syscall_dispatch()      |
    +------------------------------+
                  |
                  | SYS_PRINT
                  v
    +------------------------------+
    |       Redux kernel code      |
    |          print()             |
    +------------------------------+
                  |
                  | result in RAX
                  v
    +------------------------------+
    |       syscall_entry          |
    +------------------------------+
                  |
                  | restore state
                  | sysretq
                  v
    +------------------------------+
    |        Redux Program         |
    |            Ring 3            |
    +------------------------------+

So the important pieces Redux needs are:

    1. Ring 3 working
    2. GDT user segments
    3. 64-bit TSS
    4. TSS.RSP0 / kernel stacks
    5. SYSCALL enabled in EFER
    6. STAR configured
    7. LSTAR pointing at syscall_entry
    8. FMASK configured
    9. syscall_entry assembly
    10. syscall_dispatch() in C
    11. userspace syscall wrappers
    12. validation of user pointers

Once those exist, Redux has a real boundary between:

    applications

and:

    kernel

instead of applications directly calling kernel function pointers.