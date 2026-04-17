#ifndef ASM_FILE_H
#define ASM_FILE_H

#include "command.h"

#include <stdio.h>

#define ASM_FILE_BUFFER_CAPACITY 4096

typedef struct ASMFile {
    String name;
    int jump_label_counter;
    char* buffer;
    int buffer_capacity;
    int buffer_index;
    FILE* handle;
} ASMFile;

static bool buffer_has_content(const ASMFile* file);
static void flush_buffer(ASMFile* file);

static void write_bootstrap_asm(ASMFile* file);
static void write_asm(ASMFile* file, const Command* command, String vm_file_name);

#endif
