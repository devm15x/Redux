#include "shell.h"
#include "drivers/keyboard.h"
#include "drivers/ascii.h"
#include "terminal.h"
#include "limine.h"

#include <stddef.h>
#include <stdint.h>

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
    }
    else if (strings_equal(argv[0], "ver"))
    {
        println("Redux Kernel v0.0.1");
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
        terminal_clear(shell_framebuffer);
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