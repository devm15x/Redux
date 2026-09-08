#ifndef REDUX_API_H
#define REDUX_API_H

#include <stdint.h>
#include <stddef.h>

#define REDUX_API_VERSION 3

typedef struct
{
    uint64_t version;

    void (*print)(const char *text);
    void (*println)(const char *text);
    void (*putchar)(char character);
    void (*clear)(void);
    void (*put_pixel)(uint32_t x, uint32_t y, uint32_t color);

} redux_api_t;

typedef int (*redux_program_entry_t)(
    const redux_api_t *api
);


#endif