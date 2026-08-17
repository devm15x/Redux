#ifndef REDUX_PROGRAM_H
#define REDUX_PROGRAM_H

#include "limine.h"

void program_run(const char *path);

void program_initialize(
    struct limine_framebuffer *framebuffer
);

#endif