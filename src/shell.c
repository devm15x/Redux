#include "shell.h"
#include "drivers/keyboard.h"
#include "drivers/ascii.h"
#include "terminal.h"
#include "limine.h"
#include "drivers/ff.h"
#include <stddef.h>
#include <stdint.h>
#include "drivers/elf.h"
#include "drivers/elf3.h"
#include "program0.h"
#include "program3.h"

#define SHELL_BUFFER_SIZE 128
#define SHELL_MAX_ARGS 16

static char command_buffer[SHELL_BUFFER_SIZE];
static size_t command_length;

static struct limine_framebuffer *shell_framebuffer;

static int strings_equal(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        if (*a != *b)
        {
            return 0;
        }

        a++;
        b++;
    }

    return *a == *b;
}

static void redux_path_to_fatfs(char *path)
{
    for (size_t i = 0; path[i] != '\0'; i++)
    {
        if (path[i] == '%')
            path[i] = '/';
    }
}

static void fatfs_path_to_redux(char *path)
{
    for (size_t i = 0; path[i] != '\0'; i++)
    {
        if (path[i] == '/')
            path[i] = '%';
    }
}

static void normalize_path(const char *input, char *output, size_t output_size)
{
    size_t i = 0;

    while (input[i] != '\0' && i < output_size - 1)
    {
        output[i] = input[i] == '%' ? '/' : input[i];
        i++;
    }

    output[i] = '\0';
}

static void display_redux_path(const char *path)
{
    print("[0]:");

    if (path[0] == '0' && path[1] == ':')
        path += 2;

    if (*path == '/')
        path++;

    if (*path == '\0')
        return;

    print(" ");

    while (*path != '\0')
    {
        if (*path == '/')
            print(" % ");
        else
            terminal_putchar(*path);

        path++;
    }
}

static int shell_parse_arguments(char *input, char *argv[], int max_args)
{
    int argc = 0;
    char *current = input;

    while (*current != '\0')
    {
        while (*current == ' ')
        {
            current++;
        }

        if (*current == '\0')
        {
            break;
        }

        if (argc >= max_args)
        {
            break;
        }

        argv[argc++] = current;

        while (*current != '\0' && *current != ' ')
        {
            current++;
        }

        if (*current == ' ')
        {
            *current = '\0';
            current++;
        }
    }

    return argc;
}

static void shell_prompt(void)
{
    char cwd[256];

    if (f_getcwd(cwd, sizeof(cwd)) == FR_OK)
    {
        display_redux_path(cwd);
    }
    else
    {
        print("[?]:");
    }

    print("> ");
}


static void shell_create_test_file(void)
{
    FIL file;
    UINT written;

    FRESULT result = f_open(
        &file,
        "0:/HELLO.TXT",
        FA_CREATE_ALWAYS | FA_WRITE
    );

    print("f_open result: ");
    print_uint64((uint64_t)result);
    println("");

    if (result != FR_OK)
    {
        return;
    }

    const char *text = "Hello from Redux!\n";

    result = f_write(
        &file,
        text,
        18,
        &written
    );

    print("f_write result: ");
    print_uint64((uint64_t)result);
    println("");

    print("Bytes written: ");
    print_uint64((uint64_t)written);
    println("");

    result = f_close(&file);

    print("f_close result: ");
    print_uint64((uint64_t)result);
    println("");
}

static void shell_mkfolder(const char *path)
{
    char normalized[256];
    normalize_path(path, normalized, sizeof(normalized));

    FRESULT result = f_mkdir(normalized);

    if (result != FR_OK)
    {
        print("Could not create folder. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
    }
}

static void shell_mkfile(const char *path)
{
    char normalized[256];
    normalize_path(path, normalized, sizeof(normalized));

    FIL file;

    FRESULT result = f_open(
        &file,
        normalized,
        FA_CREATE_NEW | FA_WRITE
    );

    if (result != FR_OK)
    {
        print("Could not create file. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
        return;
    }

    f_close(&file);
}
static void shell_cd(const char *path)
{
    char normalized[256];
    normalize_path(path, normalized, sizeof(normalized));
    FRESULT result = f_chdir(normalized);

    if (result != FR_OK)
    {
        print("Could not change directory. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
    }
}
static void shell_rm(const char *path)
{
    char normalized[256];
    normalize_path(path, normalized, sizeof(normalized));

    FRESULT result = f_unlink(normalized);

    if (result != FR_OK)
    {
        print("Could not remove file or folder. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
    }
}

static void shell_dir(const char *path)
{
    DIR directory;
    FILINFO file_info;
    FRESULT result;

    char cwd[256];
    char normalized[256];

    const char *target;

    if (path == NULL || path[0] == '\0')
    {
        result = f_getcwd(cwd, sizeof(cwd));

        if (result != FR_OK)
        {
            print("Could not get current directory. FatFs error: ");
            print_uint64((uint64_t)result);
            println("");
            return;
        }

        target = cwd;
    }
    else
    {
        normalize_path(path, normalized, sizeof(normalized));
        target = normalized;
    }

    print("Directory of ");
    display_redux_path(target);
    println("");
    result = f_opendir(&directory, target);

    if (result != FR_OK)
    {
        print("Could not open directory. FatFs error: ");
        print_uint64((uint64_t)result);
        println("");
        return;
    }

    while (1)
    {
        result = f_readdir(&directory, &file_info);

        if (result != FR_OK)
        {
            print("Could not read directory. FatFs error: ");
            print_uint64((uint64_t)result);
            println("");
            break;
        }

        if (file_info.fname[0] == '\0')
            break;

        if (file_info.fattrib & AM_DIR)
            print("<DIR> ");
        else
            print("      ");

        println(file_info.fname);
    }

    f_closedir(&directory);
}

static void shell_execute(void)
{
    char *argv[SHELL_MAX_ARGS];

    int argc = shell_parse_arguments(
        command_buffer,
        argv,
        SHELL_MAX_ARGS
    );

    if (argc == 0)
    {
        return;
    }

    if (strings_equal(argv[0], "help"))
    {
        println("Commands:");
        println("help");
        println("ver");
        println("echo <text>");
        println("clear");
        println("cls");
        println("dir <dir>");
        println("run <file>");
    }
    else if (strings_equal(argv[0], "ver"))
    {
        println("Redux Kernel v0.1.0 Milestone 2");
    }
    else if (strings_equal(argv[0], "echo"))
    {
        for (int i = 1; i < argc; i++)
        {
            print(argv[i]);

            if (i < argc - 1)
            {
                print(" ");
            }
        }

        println("");
    }
    else if (strings_equal(argv[0], "clear") ||
             strings_equal(argv[0], "cls"))
    {
        terminal_clear(
    shell_framebuffer,
    terminal_get_background()
);
    }
    else if (strings_equal(argv[0], "touchtest")) {
        shell_create_test_file();
    }
    else if (strings_equal(argv[0], "run0"))
    {
        if (argc < 2)
        {
            println("Usage: run <file>");
        }
        else
        {
            char normalized[256];
            normalize_path(argv[1], normalized, sizeof(normalized));
            program_run(normalized);
        }
    }
    else if (strings_equal(argv[0], "run"))
    {
        if (argc < 2)
        {
            println("Usage: run <file>");
        }
        else
        {
            char normalized[256];
            normalize_path(argv[1], normalized, sizeof(normalized));
            program_run3(normalized);
        }
    }
    else if (strings_equal(argv[0], "mkfolder"))
    {
        if (argc < 2)
        {
            println("Usage: mkfolder <folder>");
        }
        else
        {
            shell_mkfolder(argv[1]);
        }
    }
    else if (strings_equal(argv[0], "mkfile"))
    {
        if (argc < 2)
        {
            println("Usage: mkfile <file>");
        }
        else
        {
            shell_mkfile(argv[1]);
        }
    }
    else if (strings_equal(argv[0], "rm"))
    {
        if (argc < 2)
        {
            println("Usage: rm <file\folder>");
        }
        else
        {
            shell_rm(argv[1]);
        }
    }
    else if (strings_equal(argv[0], "cd"))
    {
        if (argc < 2)
        {
            println("Usage: cd <file\folder>");
        }
        else
        {
            shell_cd(argv[1]);
        }
    }
    else if (strings_equal(argv[0], "dir")){
        if (argc >= 2)
        {   
            shell_dir(argv[1]);
        }
        
        else
        {
           shell_dir(NULL);
        }
    }
    else
    {
        print("Unknown command: ");
        println(argv[0]);
    }
}

void shell_init(struct limine_framebuffer *framebuffer)
{
    shell_framebuffer = framebuffer;

    command_length = 0;
    command_buffer[0] = '\0';

    shell_prompt();
}




void shell_update(void)
{
    terminal_cursor_update();

    uint8_t scancode = keyboard_get_scancode();

    if (scancode == 0)
    {
        return;
    }

    char c = scancode_to_ascii(scancode);

    if (c == 0)
    {
        return;
    }

    if (c == '\n')
    {
        terminal_putchar('\n');

        command_buffer[command_length] = '\0';
        shell_execute();

        command_length = 0;
        command_buffer[0] = '\0';

        shell_prompt();
        return;
    }

    if (c == '\b')
    {
        if (command_length > 0)
        {
            command_length--;
            command_buffer[command_length] = '\0';
            terminal_putchar('\b');
        }

        return;
    }

    if (command_length < SHELL_BUFFER_SIZE - 1)
    {
        command_buffer[command_length++] = c;
        command_buffer[command_length] = '\0';

        terminal_putchar(c);
    }
}