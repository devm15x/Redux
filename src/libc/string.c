#include "string.h"
#include <stdint.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *restrict destination = dest;
    const uint8_t *restrict source = src;

    for (size_t i = 0; i < n; i++)
    {
        destination[i] = source[i];
    }

    return dest;
}

void *memset(void *dest, int value, size_t n)
{
    uint8_t *bytes = dest;

    for (size_t i = 0; i < n; i++)
    {
        bytes[i] = (uint8_t)value;
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *destination = dest;
    const uint8_t *source = src;

    if (destination < source)
    {
        for (size_t i = 0; i < n; i++)
        {
            destination[i] = source[i];
        }
    }
    else if (destination > source)
    {
        for (size_t i = n; i > 0; i--)
        {
            destination[i - 1] = source[i - 1];
        }
    }

    return dest;
}

size_t strlen(const char *text)
{
    size_t length = 0;

    while (text[length] != '\0')
    {
        length++;
    }

    return length;
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *left = a;
    const uint8_t *right = b;

    for (size_t i = 0; i < n; i++)
    {
        if (left[i] != right[i])
        {
            return left[i] - right[i];
        }
    }

    return 0;
}

char *strchr(const char *str, int c)
{
    while (*str != '\0')
    {
        if (*str == (char)c)
        {
            return (char *)str;
        }

        str++;
    }

    if (c == '\0')
    {
        return (char *)str;
    }

    return NULL;
}