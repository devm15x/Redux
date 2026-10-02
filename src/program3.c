#include "program3.h"

#include "drivers/elf3.h"
#include "drivers/ff.h"
#include "drivers/ascii.h"
#include "drivers/keyboard.h"
#include "userspace.h"
#include "redux_api.h"
#include "terminal.h"
#include "paging.h"

#include <stddef.h>
#include <stdint.h>

#define PROGRAM_FILE_BUFFER_SIZE (1024 * 1024)
#define PROGRAM_DEBUG 0

#define USER_STACK_SIZE 16384
#define USER_STACK_TOP 0x0000000080000000ULL
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

#define USER_API_ADDRESS 0x0000000070000000ULL

static uint8_t program_file_buffer[
    PROGRAM_FILE_BUFFER_SIZE
] __attribute__((aligned(16)));

static struct limine_framebuffer *
    g_program_framebuffer = NULL;

static int program_user_runtime_ready = 0;
static uint64_t user_exit_stub = 0;

void program_initialize3(
    struct limine_framebuffer *framebuffer
)
{
    g_program_framebuffer =
        framebuffer;
}

uint64_t register_stub(uint32_t syscall_number);
uint64_t register_exit_stub(void);

static void program_write_bytes(
    uint64_t address,
    const uint8_t *bytes,
    size_t count
)
{
    uint8_t *destination =
        (uint8_t *)(uintptr_t)address;

    for (
        size_t i = 0;
        i < count;
        i++
    )
    {
        destination[i] =
            bytes[i];
    }
}

static int program_setup_user_runtime(void)
{
    if (program_user_runtime_ready)
    {
        return 1;
    }

    for (
        uint64_t address =
            USER_STACK_BOTTOM;
        address < USER_STACK_TOP;
        address += PAGE_SIZE
    )
    {
        uintptr_t physical =
            paging_alloc_page();

        if (physical == 0)
        {
            return 0;
        }

        if (!paging_map_page(
                address,
                physical,
                PAGE_USER |
                PAGE_WRITABLE))
        {
            return 0;
        }
    }

    uintptr_t api_physical =
        paging_alloc_page();

    if (api_physical == 0)
    {
        return 0;
    }

    if (!paging_map_page(
            USER_API_ADDRESS,
            api_physical,
            PAGE_USER |
            PAGE_WRITABLE))
    {
        return 0;
    }

    redux_api_t *user_api =
        (redux_api_t *)(uintptr_t)
        USER_API_ADDRESS;

    uint8_t *api_bytes =
        (uint8_t *)user_api;

    for (
        size_t i = 0;
        i < sizeof(redux_api_t);
        i++
    )
    {
        api_bytes[i] = 0;
    }

    user_api->version =
        REDUX_API_VERSION;

    user_api->print =
        (void (*)(const char *))
        (uintptr_t)register_stub(3);

    user_api->println =
        (void (*)(const char *))
        (uintptr_t)register_stub(1);

    user_api->putchar =
        (void (*)(char))
        (uintptr_t)register_stub(4);

    user_api->clear =
        (void (*)(uint32_t))
        (uintptr_t)register_stub(5);

    user_api->put_pixel =
        (void (*)(uint32_t, uint32_t, uint32_t))
        (uintptr_t)register_stub(6);

    user_api->get_scancode =
        (uint8_t (*)(void))
        (uintptr_t)register_stub(7);

    user_api->scancode_to_ascii =
        (uint8_t (*)(uint8_t))
        (uintptr_t)register_stub(8);

    user_exit_stub =
        register_exit_stub();

    program_user_runtime_ready = 1;

    return 1;
}

#if PROGRAM_DEBUG

static void debug_print(
    const char *text
)
{
    print("[PROGRAM DEBUG] ");
    println(text);
}

static void debug_print_uint(
    const char *name,
    uint64_t value
)
{
    print("[PROGRAM DEBUG] ");
    print(name);
    print(": ");
    print_uint64(value);
    println("");
}

static void debug_print_hex(
    const char *name,
    uint64_t value
)
{
    static const char digits[] =
        "0123456789ABCDEF";

    char output[19];

    output[0] = '0';
    output[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        uint64_t shift =
            (uint64_t)(15 - i) * 4;

        output[i + 2] =
            digits[
                (value >> shift) &
                0x0F
            ];
    }

    output[18] = '\0';

    print("[PROGRAM DEBUG] ");
    print(name);
    print(": ");
    println(output);
}

static void debug_print_path(
    const char *path
)
{
    print("[PROGRAM DEBUG] Path: ");
    println(path);
}

#define PROGRAM_DEBUG_MESSAGE(text) \
    debug_print(text)

#define PROGRAM_DEBUG_UINT(name, value) \
    debug_print_uint(                   \
        name,                           \
        (uint64_t)(value)               \
    )

#define PROGRAM_DEBUG_HEX(name, value) \
    debug_print_hex(                    \
        name,                           \
        (uint64_t)(uintptr_t)(value)    \
    )

#define PROGRAM_DEBUG_PATH(path) \
    debug_print_path(path)

#else

#define PROGRAM_DEBUG_MESSAGE(text) \
    do                              \
    {                               \
        (void)(text);               \
    }                               \
    while (0)

#define PROGRAM_DEBUG_UINT(name, value) \
    do                                  \
    {                                   \
        (void)(name);                   \
        (void)(value);                  \
    }                                   \
    while (0)

#define PROGRAM_DEBUG_HEX(name, value) \
    do                                 \
    {                                  \
        (void)(name);                  \
        (void)(value);                 \
    }                                  \
    while (0)

#define PROGRAM_DEBUG_PATH(path) \
    do                           \
    {                            \
        (void)(path);            \
    }                            \
    while (0)

#endif

uint64_t stub_next_address =
    USER_API_ADDRESS + 0x100;

uint64_t register_stub(
    uint32_t syscall_number
)
{
    uint8_t generic_stub[] =
    {
        0xB8,
        syscall_number & 0xFF,
        (syscall_number >> 8) & 0xFF,
        (syscall_number >> 16) & 0xFF,
        (syscall_number >> 24) & 0xFF,
        0x0F,
        0x05,
        0xC3
    };

    program_write_bytes(
        stub_next_address,
        generic_stub,
        sizeof(generic_stub)
    );

    uint64_t stub_current_address =
        stub_next_address;

    stub_next_address +=
        sizeof(generic_stub);

    return stub_current_address;
}

uint64_t register_exit_stub(void)
{
    static const uint8_t exit_stub[] =
    {
        0x89, 0xC7,
        0xB8, 0x02, 0x00, 0x00, 0x00,
        0x0F, 0x05,
        0x0F, 0x0B
    };

    uint64_t address =
        stub_next_address;

    program_write_bytes(
        address,
        exit_stub,
        sizeof(exit_stub)
    );

    stub_next_address +=
        sizeof(exit_stub);

    return address;
}

void program_run3(
    const char *path
)
{
    PROGRAM_DEBUG_MESSAGE(
        "program_run entered."
    );

    PROGRAM_DEBUG_HEX(
        "Path pointer",
        path
    );

    if (path == NULL)
    {
        PROGRAM_DEBUG_MESSAGE(
            "Path pointer is null."
        );

        println(
            "Usage: run <file>"
        );

        return;
    }

    if (path[0] == '\0')
    {
        PROGRAM_DEBUG_MESSAGE(
            "Path string is empty."
        );

        println(
            "Usage: run <file>"
        );

        return;
    }

    PROGRAM_DEBUG_PATH(path);

    FIL file;
    FRESULT result;

    PROGRAM_DEBUG_MESSAGE(
        "Calling f_open."
    );

    result = f_open(
        &file,
        path,
        FA_READ
    );

    PROGRAM_DEBUG_UINT(
        "f_open result",
        result
    );

    if (result != FR_OK)
    {
        print(
            "Could not open program. "
            "FatFs error: "
        );

        print_uint64(
            (uint64_t)result
        );

        println("");

        return;
    }

    PROGRAM_DEBUG_MESSAGE(
        "Program file opened."
    );

    FSIZE_t file_size =
        f_size(&file);

    PROGRAM_DEBUG_UINT(
        "Program file size",
        file_size
    );

    if (file_size == 0)
    {
        PROGRAM_DEBUG_MESSAGE(
            "Program file is empty."
        );

        println(
            "Program file is empty."
        );

        result =
            f_close(&file);

        PROGRAM_DEBUG_UINT(
            "f_close result",
            result
        );

        return;
    }

    if (file_size >
        PROGRAM_FILE_BUFFER_SIZE)
    {
        println(
            "Program file is too large."
        );

        f_close(&file);

        return;
    }

    PROGRAM_DEBUG_HEX(
        "Program file buffer",
        program_file_buffer
    );

    UINT bytes_read = 0;

    PROGRAM_DEBUG_MESSAGE(
        "Calling f_read."
    );

    result = f_read(
        &file,
        program_file_buffer,
        (UINT)file_size,
        &bytes_read
    );

    PROGRAM_DEBUG_UINT(
        "f_read result",
        result
    );

    PROGRAM_DEBUG_UINT(
        "Bytes read",
        bytes_read
    );

    PROGRAM_DEBUG_MESSAGE(
        "Calling f_close."
    );

    FRESULT close_result =
        f_close(&file);

    PROGRAM_DEBUG_UINT(
        "f_close result",
        close_result
    );

    if (result != FR_OK)
    {
        print(
            "Could not read program. "
            "FatFs error: "
        );

        print_uint64(
            (uint64_t)result
        );

        println("");

        return;
    }

    if (close_result != FR_OK)
    {
        print(
            "Could not close program. "
            "FatFs error: "
        );

        print_uint64(
            (uint64_t)close_result
        );

        println("");

        return;
    }

    if (bytes_read !=
        (UINT)file_size)
    {
        PROGRAM_DEBUG_MESSAGE(
            "File was only partially read."
        );

        PROGRAM_DEBUG_UINT(
            "Expected bytes",
            file_size
        );

        PROGRAM_DEBUG_UINT(
            "Actual bytes",
            bytes_read
        );

        println(
            "Program file was only "
            "partially read."
        );

        return;
    }

    PROGRAM_DEBUG_MESSAGE(
        "File read completed successfully."
    );

    PROGRAM_DEBUG_HEX(
        "ELF byte 0",
        program_file_buffer[0]
    );

    PROGRAM_DEBUG_HEX(
        "ELF byte 1",
        program_file_buffer[1]
    );

    PROGRAM_DEBUG_HEX(
        "ELF byte 2",
        program_file_buffer[2]
    );

    PROGRAM_DEBUG_HEX(
        "ELF byte 3",
        program_file_buffer[3]
    );

    PROGRAM_DEBUG_MESSAGE(
        "Calling load_elf_binary."
    );

    elf_load_info_t load_info =
        load_elf3_binary(
            program_file_buffer,
            (size_t)file_size
        );

    PROGRAM_DEBUG_UINT(
        "ELF load result",
        load_info.result
    );

    PROGRAM_DEBUG_HEX(
        "ELF entry pointer",
        load_info.entry
    );

    if (load_info.result !=
        ELF_LOAD_OK)
    {
        print(
            "ELF loading failed: "
        );

        println(
            elf3_load_error_string(
                load_info.result
            )
        );

        return;
    }

    if (load_info.entry == NULL)
    {
        PROGRAM_DEBUG_MESSAGE(
            "Loader returned a null "
            "entry pointer."
        );

        println(
            "ELF entry point is null."
        );

        return;
    }

    redux_program_entry_t entry =
        (redux_program_entry_t)
        load_info.entry;

    PROGRAM_DEBUG_HEX(
        "Entry function pointer",
        entry
    );

    PROGRAM_DEBUG_HEX(
        "Redux API address",
        USER_API_ADDRESS
    );

    PROGRAM_DEBUG_UINT(
        "Redux API version",
        REDUX_API_VERSION
    );

    println(
        "Launching program..."
    );

    PROGRAM_DEBUG_MESSAGE(
        "About to call ELF entry."
    );

    int exit_code = 0;

    if (!program_setup_user_runtime())
    {
        println(
            "Could not set up userspace."
        );

        return;
    }

    uint64_t user_rsp =
        USER_STACK_TOP -
        sizeof(uint64_t);

    *(uint64_t *)(uintptr_t)
        user_rsp =
            user_exit_stub;

    exit_code = (int)jump_usermode(
        (uint64_t)(uintptr_t)
        load_info.entry,
        user_rsp,
        USER_API_ADDRESS
    );

    PROGRAM_DEBUG_MESSAGE(
        "ELF entry returned successfully."
    );

    PROGRAM_DEBUG_UINT(
        "Program exit code",
        exit_code
    );

    print(
        "Program exited with code "
    );

    if (exit_code < 0)
    {
        print("-");

        int64_t signed_code =
            (int64_t)exit_code;

        uint64_t positive_code =
            (uint64_t)(
                -signed_code
            );

        print_uint64(
            positive_code
        );
    }
    else
    {
        print_uint64(
            (uint64_t)exit_code
        );
    }

    println(".");
}