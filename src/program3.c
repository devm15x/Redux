#include "program3.h"

#include "drivers/elf3.h"
#include "drivers/ff.h"
#include "drivers/ascii.h"
#include "drivers/keyboard.h"

#include "redux_api.h"
#include "terminal.h"

#include <stddef.h>
#include <stdint.h>

#define PROGRAM_FILE_BUFFER_SIZE (1024 * 1024)
#define PROGRAM_DEBUG 0

/*
 * Buffer used to read the ELF file from disk.
 *
 * Alignment is useful because this buffer contains ELF structures
 * with 64-bit members.
 */
static uint8_t program_file_buffer[
    PROGRAM_FILE_BUFFER_SIZE
] __attribute__((aligned(16)));

/*
 * The framebuffer is kept inside the kernel.
 *
 * Ring-0 programs only see api->clear(), not Limine directly.
 */
static struct limine_framebuffer *
    g_program_framebuffer = NULL;


/* ============================================================
 * Redux API wrappers
 * ============================================================
 */

static void api_set_cursor(
    uint32_t x,
    uint32_t y
)
{
    terminal_set_cursor(x, y);
}

static void api_clear(void)
{
    if (g_program_framebuffer == NULL)
    {
        return;
    }

    terminal_clear(
        g_program_framebuffer,
        0x00081A33
    );
}

void program_initialize3(
    struct limine_framebuffer *framebuffer
)
{
    g_program_framebuffer =
        framebuffer;
}


/* ============================================================
 * Redux Ring-0 API
 * ============================================================
 */

static const redux_api_t redux_api =
{
    .version = REDUX_API_VERSION,

    .print = print,
    .println = println,
    .putchar = terminal_putchar,
    .print_uint64 = print_uint64,

    .clear = api_clear,

    .keyboard_get_scancode =
        keyboard_get_scancode,

    .scancode_to_ascii =
        scancode_to_ascii,

    .set_cursor =
        api_set_cursor
};


/* ============================================================
 * Debugging
 * ============================================================
 */

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
    debug_print_hex(                   \
        name,                          \
        (uint64_t)(uintptr_t)(value)   \
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


/* ============================================================
 * Program loader
 * ============================================================
 */

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

    /*
     * Validate the path before touching it.
     */
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


    /* ========================================================
     * Open file
     * ========================================================
     */

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


    /* ========================================================
     * Validate file size
     * ========================================================
     */

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
        PROGRAM_DEBUG_MESSAGE(
            "Program file exceeds "
            "buffer size."
        );

        println(
            "Program file is larger "
            "than 1 MiB."
        );

        result =
            f_close(&file);

        PROGRAM_DEBUG_UINT(
            "f_close result",
            result
        );

        return;
    }

    PROGRAM_DEBUG_HEX(
        "Program file buffer",
        program_file_buffer
    );


    /* ========================================================
     * Read file
     * ========================================================
     */

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


    /* ========================================================
     * Close file
     * ========================================================
     */

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


    /* ========================================================
     * ELF debug header
     * ========================================================
     */

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


    /* ========================================================
     * Load ELF
     * ========================================================
     */

    PROGRAM_DEBUG_MESSAGE(
        "Calling load_elf_binary."
    );

    elf_load_info_t load_info =
        load_elf_binary(
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
            elf_load_error_string(
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


    /* ========================================================
     * Convert ELF entry into Redux entry point
     * ========================================================
     */

    redux_program_entry_t entry =
        (redux_program_entry_t)
        load_info.entry;

    PROGRAM_DEBUG_HEX(
        "Entry function pointer",
        entry
    );

    PROGRAM_DEBUG_HEX(
        "Redux API address",
        &redux_api
    );

    PROGRAM_DEBUG_UINT(
        "Redux API version",
        redux_api.version
    );

    PROGRAM_DEBUG_HEX(
        "Redux API print",
        redux_api.print
    );

    PROGRAM_DEBUG_HEX(
        "Redux API println",
        redux_api.println
    );

    PROGRAM_DEBUG_HEX(
        "Redux API putchar",
        redux_api.putchar
    );


    /* ========================================================
     * Execute program
     * ========================================================
     */

    println(
        "Launching program..."
    );

    PROGRAM_DEBUG_MESSAGE(
        "About to call ELF entry."
    );

    int exit_code =
        entry(&redux_api);

    PROGRAM_DEBUG_MESSAGE(
        "ELF entry returned successfully."
    );

    PROGRAM_DEBUG_UINT(
        "Program exit code",
        exit_code
    );


    /* ========================================================
     * Print exit status
     * ========================================================
     */

    print(
        "Program exited with code "
    );

    if (exit_code < 0)
    {
        print("-");

        /*
         * Convert through int64_t so INT_MIN-like
         * values do not overflow as an int.
         */
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
