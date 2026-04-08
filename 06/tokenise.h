#ifndef TOKENISE_H
#define TOKENISE_H

#include "string_view.h"

typedef enum TokenType {
    TT_EOF,
    TT_AT,
    TT_NOT,
    TT_EQUALS,
    TT_PLUS,
    TT_MINUS,
    TT_AND,
    TT_OR,
    TT_SEMI_COLON,
    TT_LPAREN,
    TT_RPAREN,
    TT_INT,
    TT_IDENTIFIER
} TokenType;

typedef struct Token {
    TokenType type;
    String text;
    int line;
} Token;

typedef struct Tokens {
    Token* data;
    int count;
} Tokens;

static Tokens tokenise(const char* asm_file, int asm_file_size);

#endif
