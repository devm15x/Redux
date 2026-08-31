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

static void shell_dir(const char *path)
{
    DIR directory;
    FILINFO file_info;

    FRESULT result = f_opendir(&directory, path);

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
        {
            break;
        }

        if (file_info.fattrib & AM_DIR)
        {
            print("<DIR> ");
        }
        else
        {
            print("      ");
        }

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
        println("Redux Kernel v0.1.0 Milestone 1");
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
            program_run(argv[1]);
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
            program_run3(argv[1]);
        }
    }
    else if (strings_equal(argv[0], "dir")){
        if (argc >= 2)
        {   
            shell_dir(argv[1]);
        }
        
        else
        {
            shell_dir("0:/");
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