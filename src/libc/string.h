#ifndef REDUX_STRING_H
#define REDUX_STRING_H

#include <stddef.h>

void *memcpy(void *restrict dest, const void *restrict src, size_t n);
void *memset(void *dest, int value, size_t n);
void *memmove(void *dest, const void *src, size_t n);

size_t strlen(const char *text);
int strcmp(const char *a, const char *b);
int memcmp(const void *a, const void *b, size_t n);
char *strchr(const char *str, int c);

#endif