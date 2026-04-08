#include "tokenise.h"

#include <assert.h>
#include <ctype.h>

static int is_valid_identifier_char(const char c) {
    return isalnum(c) || (c == '_') || (c == '.') || (c == '$') || (c == ':');
}

static Tokens tokenise(const char* const asm_file, const int asm_file_size) {
    const int max_token_count = asm_file_size + 1;  // can't be more tokens than bytes in the file, +1 for EOF token
    Tokens tokens = {};
    tokens.data = malloc(sizeof(Token) * max_token_count);
    tokens.count = 0;

    int byte_index = 0;
    int line_count = 0;
    while (byte_index < asm_file_size) {
        // skip any whitespace
        while (byte_index < asm_file_size && isspace(asm_file[byte_index])) {
            if (asm_file[byte_index] == '\n') {
                ++line_count;
            }
            
            ++byte_index;
        }
        
        if (byte_index >= asm_file_size) {
            break;
        }
        
        const char byte = asm_file[byte_index];
        if (byte == '/' && byte_index + 1 < asm_file_size && asm_file[byte_index + 1] == '/') {
            // no token to parse, skip until end of line and try again
            while (byte_index < asm_file_size && asm_file[byte_index] != '\n') {
                ++byte_index;
            }
            
            continue;
        }
        
        Token token = {};
        switch (byte) {
            case '@': {
                token.type = TT_AT;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '!': {
                token.type = TT_NOT;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '=': {
                token.type = TT_EQUALS;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '+': {
                token.type = TT_PLUS;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '-': {
                token.type = TT_MINUS;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '&': {
                token.type = TT_AND;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '|': {
                token.type = TT_OR;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case ';': {
                token.type = TT_SEMI_COLON;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case '(': {
                token.type = TT_LPAREN;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            case ')': {
                token.type = TT_RPAREN;
                token.text = construct_string(asm_file + byte_index, 1);
                ++byte_index;
            } break;
            
            default: {
                if (isdigit(byte)) {
                    token.type = TT_INT;
                    token.text.data = asm_file + byte_index;
                    while (byte_index < asm_file_size && isdigit(asm_file[byte_index])) {
                        ++token.text.count;
                        ++byte_index;
                    }
                } else if (isalpha(byte)) {
                    token.type = TT_IDENTIFIER;
                    token.text.data = asm_file + byte_index;
                    while (byte_index < asm_file_size && is_valid_identifier_char(asm_file[byte_index])) {
                        ++token.text.count;
                        ++byte_index;
                    }
                }
            } break;
        }
        
        token.line = line_count;
        tokens.data[tokens.count++] = token;
    }
    
    assert(byte_index == asm_file_size);
    assert(tokens.count + 1 < max_token_count);
    
    Token eof_token = {};
    eof_token.type = TT_EOF;
    tokens.data[tokens.count++] = eof_token;
    
    return tokens;
}
