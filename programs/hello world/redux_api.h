#ifndef REDUX_API_H
#define REDUX_API_H

#include <stdint.h>

#define REDUX_API_VERSION 1

typedef struct
{
    uint64_t version;

    void (*print)(const char *text);
    void (*println)(const char *text);
    void (*putchar)(char character);
} redux_api_t;

typedef int (*redux_program_entry_t)(const redux_api_t *api);

#endif