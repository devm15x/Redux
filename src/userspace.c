#include <stdint.h>
#include <stddef.h>

#include "paging.h"
#include "debug.h"

#define USER_CODE_ADDRESS 0x0000000040000000ULL
#define USER_STACK_TOP    0x0000000080000000ULL

extern void jump_usermode(
    uint64_t rip,
    uint64_t rsp,
    uint64_t api
);

extern uint8_t user_test_start[];
extern uint8_t user_test_end[];

void usermode_test(void)
{
    qemu_debug_print("[USER] Setting up code page...\n");

    uintptr_t code_phys =
        paging_alloc_page();

    if (code_phys == 0)
    {
        qemu_debug_print("[USER] FAIL: code alloc\n");
        return;
    }

    if (!paging_map_page(
            USER_CODE_ADDRESS,
            code_phys,
            PAGE_USER |
            PAGE_WRITABLE))
    {
        qemu_debug_print("[USER] FAIL: code map\n");
        return;
    }

    uint8_t *code =
        (uint8_t *)
        paging_phys_to_virt(
            code_phys
        );

    size_t code_size =
        (size_t)(
            user_test_end -
            user_test_start
        );

    if (code_size == 0 ||
        code_size > PAGE_SIZE)
    {
        qemu_debug_print(
            "[USER] FAIL: bad code size\n"
        );
        return;
    }

    for (size_t i = 0;
         i < code_size;
         i++)
    {
        code[i] =
            user_test_start[i];
    }

    volatile uint8_t *user =
        (volatile uint8_t *)
        USER_CODE_ADDRESS;

    qemu_debug_print("[USER] Bytes: ");

    for (int i = 0; i < 8; i++)
    {
        qemu_debug_hex8(user[i]);
        qemu_debug_putc(' ');
    }

    qemu_debug_putc('\n');

    qemu_debug_print(
        "[USER] Code copied successfully\n"
    );

    qemu_debug_print(
        "[USER] Setting up stack...\n"
    );

    uintptr_t stack_phys =
        paging_alloc_page();

    if (stack_phys == 0)
    {
        qemu_debug_print(
            "[USER] FAIL: stack alloc\n"
        );
        return;
    }

    if (!paging_map_page(
            USER_STACK_TOP - PAGE_SIZE,
            stack_phys,
            PAGE_USER |
            PAGE_WRITABLE))
    {
        qemu_debug_print(
            "[USER] FAIL: stack map\n"
        );
        return;
    }

    qemu_debug_print(
        "[USER] Entering Ring 3...\n"
    );

    jump_usermode(
        USER_CODE_ADDRESS,
        USER_STACK_TOP,
        0
    );

    qemu_debug_print(
        "[USER] FAIL: returned!\n"
    );
}