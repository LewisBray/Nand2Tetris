#ifndef COMPILE_H
#define COMPILE_H

#include "jack_file.h"

#include <stdio.h>

typedef struct Output {
    FILE* vm;
#ifdef OUTPUT_XML
    FILE* xml;
#endif
} Output;

static void compile(JackFile* file, Output* out);

#endif
