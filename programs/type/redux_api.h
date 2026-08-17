#ifndef REDUX_API_H
#define REDUX_API_H

#include <stdint.h>
#include <stddef.h>

#define REDUX_API_VERSION 2

typedef struct
{
    uint64_t version;

    void (*print)(const char *text);
    void (*println)(const char *text);
    void (*putchar)(char character);
    void (*print_uint64)(uint64_t value);
    void (*set_cursor)(uint32_t x, uint32_t y);
    void (*clear)(void);

    uint8_t (*keyboard_get_scancode)(void);
    char (*scancode_to_ascii)(uint8_t scancode);

    int (*file_read_text)(
        const char *path,
        char *buffer,
        uint64_t buffer_size,
        uint64_t *bytes_read
    );

    int (*file_write_text)(
        const char *path,
        const char *text,
        uint64_t text_length
    );

} redux_api_t;

typedef int (*redux_program_entry_t)(
    const redux_api_t *api
);


#endif