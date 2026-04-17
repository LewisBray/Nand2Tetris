#ifndef VM_FILE_H
#define VM_FILE_H

#include "command.h"

typedef struct VMFile {
    const char* begin;
    const char* position;
    const char* end;
} VMFile;

typedef enum TokenType {
    TT_EOF,
    TT_INT,
    TT_IDENTIFIER
} TokenType;

typedef struct Token {
    TokenType type;
    String text;
} Token;

static Token get_next_token(VMFile* file);
static Command parse_command(String name, VMFile* file);

#endif
