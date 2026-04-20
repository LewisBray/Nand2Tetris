#ifndef JACK_FILE_H
#define JACK_FILE_H

#include "string_view.h"

typedef enum TokenType {
    TT_EOF,
    TT_CLASS,
    TT_CONSTRUCTOR,
    TT_FUNCTION,
    TT_METHOD,
    TT_FIELD,
    TT_STATIC,
    TT_VAR,
    TT_INT,
    TT_CHAR,
    TT_BOOLEAN,
    TT_VOID,
    TT_TRUE,
    TT_FALSE,
    TT_NULL,
    TT_THIS,
    TT_LET,
    TT_DO,
    TT_IF,
    TT_ELSE,
    TT_WHILE,
    TT_RETURN,
    TT_OPEN_BRACE,
    TT_CLOSE_BRACE,
    TT_OPEN_PAREN,
    TT_CLOSE_PAREN,
    TT_OPEN_BRACKET,
    TT_CLOSE_BRACKET,
    TT_DOT,
    TT_COMMA,
    TT_SEMI_COLON,
    TT_PLUS,
    TT_MINUS,
    TT_ASTERISK,
    TT_SLASH,
    TT_AMPERSAND,
    TT_PIPE,
    TT_OPEN_ANGLE_BRACKET,
    TT_CLOSE_ANGLE_BRACKET,
    TT_EQUALS,
    TT_TILDE,
    TT_INT_CONST,
    TT_STRING_CONST,
    TT_IDENTIFIER
} TokenType;

typedef struct Token {
    TokenType type;
    int line;
    String text;
} Token;

typedef struct JackFile {
    const char* begin;
    const char* position;
    const char* end;
    int current_line;
    Token current_token;
} JackFile;

static JackFile open_jack_file(const char* file_name);
static Token consume_token(JackFile* file);

#endif
