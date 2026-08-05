#include "program.h"

#include "drivers/elf.h"
#include "drivers/ff.h"
#include "redux_api.h"
#include "terminal.h"

#include <stddef.h>
#include <stdint.h>

#define PROGRAM_FILE_BUFFER_SIZE (1024 * 1024)
#define PROGRAM_DEBUG 0

static uint8_t program_file_buffer[PROGRAM_FILE_BUFFER_SIZE];

static const redux_api_t redux_api = {
    .version = REDUX_API_VERSION,
    .print = print,
    .println = println,
    .putchar = terminal_putchar
};

#if PROGRAM_DEBUG

static void debug_print(const char *text)
{
    print("[PROGRAM DEBUG] ");
    println(text);
}

static void debug_print_uint(const char *name, uint64_t value)
{
    print("[PROGRAM DEBUG] ");
    print(name);
    print(": ");
    print_uint64(value);
    println("");
}

static void debug_print_hex(const char *name, uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    char output[19];

    output[0] = '0';
    output[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        uint64_t shift = (uint64_t)(15 - i) * 4;

        output[i + 2] =
            digits[(value >> shift) & 0x0F];
    }

    output[18] = '\0';

    print("[PROGRAM DEBUG] ");
    print(name);
    print(": ");
    println(output);
}

#define PROGRAM_DEBUG_MESSAGE(text) \
    debug_print(text)

#define PROGRAM_DEBUG_UINT(name, value) \
    debug_print_uint(name, (uint64_t)(value))

#define PROGRAM_DEBUG_HEX(name, value) \
    debug_print_hex(name, (uint64_t)(uintptr_t)(value))

#else

#define PROGRAM_DEBUG_MESSAGE(text) \
    do { (void)(text); } while (0)

#define PROGRAM_DEBUG_UINT(name, value) \
    do { (void)(name); (void)(value); } while (0)

#define PROGRAM_DEBUG_HEX(name, value) \
    do { (void)(name); (void)(value); } while (0)

#endif

void program_run(const char *path)
{
    PROGRAM_DEBUG_MESSAGE("program_run entered.");

    PROGRAM_DEBUG_HEX("Path pointer", path);

    if (path == 0)
    {
        PROGRAM_DEBUG_MESSAGE("Path pointer is null.");
        println("Usage: run <file>");
        return;
    }

    if (path[0] == '\0')
    {
        PROGRAM_DEBUG_MESSAGE("Path string is empty.");
        println("Usage: run <file>");
        return;
    }

    print("[PROGRAM DEBUG] Path: ");
#if PROGRAM_DEBUG
    println(path);
#endif

    FIL file;
    FRESULT result;

    PROGRAM_DEBUG_MESSAGE("Calling f_open.");

    result = f_open(
        &file,
        path,
        FA_READ
    );

    PROGRAM_DEBUG_UINT("f_open result", result);

    if (result != FR_OK)
    {
        print("Could not open program. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
        return;
    }

    PROGRAM_DEBUG_MESSAGE("Program file opened.");

    FSIZE_t file_size = f_size(&file);

    PROGRAM_DEBUG_UINT("Program file size", file_size);

    if (file_size == 0)
    {
        PROGRAM_DEBUG_MESSAGE("Program file is empty.");
        println("Program file is empty.");

        result = f_close(&file);
        PROGRAM_DEBUG_UINT("f_close result", result);

        return;
    }

    if (file_size > PROGRAM_FILE_BUFFER_SIZE)
    {
        PROGRAM_DEBUG_MESSAGE(
            "Program file exceeds buffer size."
        );

        println("Program file is larger than 1 MiB.");

        result = f_close(&file);
        PROGRAM_DEBUG_UINT("f_close result", result);

        return;
    }

    PROGRAM_DEBUG_HEX(
        "Program file buffer",
        program_file_buffer
    );

    UINT bytes_read = 0;

    PROGRAM_DEBUG_MESSAGE("Calling f_read.");

    result = f_read(
        &file,
        program_file_buffer,
        (UINT)file_size,
        &bytes_read
    );

    PROGRAM_DEBUG_UINT("f_read result", result);
    PROGRAM_DEBUG_UINT("Bytes read", bytes_read);

    PROGRAM_DEBUG_MESSAGE("Calling f_close.");

    FRESULT close_result = f_close(&file);

    PROGRAM_DEBUG_UINT(
        "f_close result",
        close_result
    );

    if (result != FR_OK)
    {
        print("Could not read program. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
        return;
    }

    if (close_result != FR_OK)
    {
        print("Could not close program. FatFs error: ");
        print_uint64((uint64_t)close_result);
        println("");
        return;
    }

    if (bytes_read != (UINT)file_size)
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

        println("Program file was only partially read.");
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

    elf_load_info_t load_info = load_elf_binary(
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

    if (load_info.result != ELF_LOAD_OK)
    {
        print("ELF loading failed: ");
        println(
            elf_load_error_string(
                load_info.result
            )
        );

        return;
    }

    if (load_info.entry == 0)
    {
        PROGRAM_DEBUG_MESSAGE(
            "Loader returned a null entry pointer."
        );

        println("ELF entry point is null.");
        return;
    }

    redux_program_entry_t entry =
        (redux_program_entry_t)load_info.entry;

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

    println("Launching program...");

    PROGRAM_DEBUG_MESSAGE(
        "About to call ELF entry."
    );

    int exit_code = entry(&redux_api);

    PROGRAM_DEBUG_MESSAGE(
        "ELF entry returned successfully."
    );

    PROGRAM_DEBUG_UINT(
        "Program exit code",
        exit_code
    );

    print("Program exited with code ");

    if (exit_code < 0)
    {
        print("-");

        uint64_t positive_code =
            (uint64_t)(-(int64_t)exit_code);

        print_uint64(positive_code);
    }
    else
    {
        print_uint64((uint64_t)exit_code);
    }

    println(".");
}