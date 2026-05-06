#include "jack_file.h"

#include <ctype.h>

static bool is_valid_identifier_char(const char c) {
    return isalpha(c) || isdigit(c) || (c == '_');
}

static Token get_next_token(JackFile* const file) {
    while (file->position != file->end) {
        bool skipped_whitespace = false;
        while (file->position != file->end && isspace(*file->position)) {
            if (*file->position == '\n') {
                ++file->current_line;
            }
            
            skipped_whitespace = true;
            ++file->position;
        }
        
        bool skipped_comment = false;
        if (file->position != file->end && *file->position == '/') {
            if ((file->position + 1) != file->end && file->position[1] == '/') {
                ++file->position;
                while (file->position != file->end && *file->position != '\n') {
                    ++file->position;
                }
                
                skipped_comment = true;
            } else if ((file->position + 1) != file->end && file->position[1] == '*') {
                ++file->position;
                while (file->position != file->end) {
                    if (*file->position == '*' && (file->position + 1) != file->end && file->position[1] == '/') {
                        file->position += 2;
                        break;
                    }
                    
                    ++file->position;
                }
                
                skipped_comment = true;
            }
        }
        
        if (!skipped_whitespace && !skipped_comment) {
            break;
        }
    }
    
    if (file->position == file->end) {
        static const Token eof_token = {.type = TT_EOF};
        return eof_token;
    }
    
    Token result = {};
    result.line = file->current_line;
    switch (*file->position) {
        case '{': {
            result.type = TT_OPEN_BRACE;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '}': {
            result.type = TT_CLOSE_BRACE;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '(': {
            result.type = TT_OPEN_PAREN;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case ')': {
            result.type = TT_CLOSE_PAREN;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '[': {
            result.type = TT_OPEN_BRACKET;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case ']': {
            result.type = TT_CLOSE_BRACKET;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '.': {
            result.type = TT_DOT;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case ',': {
            result.type = TT_COMMA;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case ';': {
            result.type = TT_SEMI_COLON;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '+': {
            result.type = TT_PLUS;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '-': {
            result.type = TT_MINUS;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '*': {
            result.type = TT_ASTERISK;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '/': {
            result.type = TT_SLASH;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '&': {
            result.type = TT_AMPERSAND;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '|': {
            result.type = TT_PIPE;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '<': {
            result.type = TT_OPEN_ANGLE_BRACKET;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '>': {
            result.type = TT_CLOSE_ANGLE_BRACKET;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '=': {
            result.type = TT_EQUALS;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '~': {
            result.type = TT_TILDE;
            result.text = construct_string(file->position, 1);
            ++file->position;
        } break;
        
        case '"': {
            result.type = TT_STRING_CONST;
            result.text.data = ++file->position;
            result.text.count = 0;
            bool encountered_end_of_string = false;
            while (file->position != file->end) {
                if (*file->position == '\n') {
                    ERROR("Line: %d. Encountered new-line in string constant", file->current_line);
                }
                
                if (*file->position == '"') {
                    encountered_end_of_string = true;
                    break;
                }
                
                ++result.text.count;
                ++file->position;
            }
            
            ++file->position;
            
            if (!encountered_end_of_string) {
                ERROR("Unexpected end of file in string constant");
            }
        } break;
        
        default: {
            if (isdigit(*file->position)) {
                result.type = TT_INT_CONST;
                result.text.data = file->position++;
                result.text.count = 1;
                while (file->position != file->end && isdigit(*file->position)) {
                    ++result.text.count;
                    ++file->position;
                }
            } else if (is_valid_identifier_char(*file->position)) {
                result.text.data = file->position++;
                result.text.count = 1;
                while (file->position != file->end && is_valid_identifier_char(*file->position)) {
                    ++result.text.count;
                    ++file->position;
                }
                
                if (STRING_EQUALS(result.text, "class")) {
                    result.type = TT_CLASS;
                } else if (STRING_EQUALS(result.text, "constructor")) {
                    result.type = TT_CONSTRUCTOR;
                } else if (STRING_EQUALS(result.text, "function")) {
                    result.type = TT_FUNCTION;
                } else if (STRING_EQUALS(result.text, "method")) {
                    result.type = TT_METHOD;
                } else if (STRING_EQUALS(result.text, "field")) {
                    result.type = TT_FIELD;
                } else if (STRING_EQUALS(result.text, "static")) {
                    result.type = TT_STATIC;
                } else if (STRING_EQUALS(result.text, "var")) {
                    result.type = TT_VAR;
                } else if (STRING_EQUALS(result.text, "int")) {
                    result.type = TT_INT;
                } else if (STRING_EQUALS(result.text, "char")) {
                    result.type = TT_CHAR;
                } else if (STRING_EQUALS(result.text, "boolean")) {
                    result.type = TT_BOOLEAN;
                } else if (STRING_EQUALS(result.text, "void")) {
                    result.type = TT_VOID;
                } else if (STRING_EQUALS(result.text, "true")) {
                    result.type = TT_TRUE;
                } else if (STRING_EQUALS(result.text, "false")) {
                    result.type = TT_FALSE;
                } else if (STRING_EQUALS(result.text, "null")) {
                    result.type = TT_NULL;
                } else if (STRING_EQUALS(result.text, "this")) {
                    result.type = TT_THIS;
                } else if (STRING_EQUALS(result.text, "let")) {
                    result.type = TT_LET;
                } else if (STRING_EQUALS(result.text, "do")) {
                    result.type = TT_DO;
                } else if (STRING_EQUALS(result.text, "if")) {
                    result.type = TT_IF;
                } else if (STRING_EQUALS(result.text, "else")) {
                    result.type = TT_ELSE;
                } else if (STRING_EQUALS(result.text, "while")) {
                    result.type = TT_WHILE;
                } else if (STRING_EQUALS(result.text, "return")) {
                    result.type = TT_RETURN;
                } else {
                    result.type = TT_IDENTIFIER;
                }
            } else {
                ERROR("Line: %d. Encountered unexpected token type", file->current_line);
            }
        } break;
    }
    
    return result;
}

static JackFile open_jack_file(const char* const file_name) {
    JackFile result = {};
    
    FILE* file = fopen(file_name, "rb");
    fseek(file, 0, SEEK_END);
    const int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* const file_buffer = malloc(file_size);
    const int bytes_read = fread(file_buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        ERROR("Failed to read entire file: %s", file_name);
    }
    
    result.begin = file_buffer;
    result.position = file_buffer;
    result.end = file_buffer + file_size;
    result.current_line = 1;
    result.current_token = get_next_token(&result);
    
    return result;
}

static Token consume_token(JackFile* const file) {
    const Token result = file->current_token;
    file->current_token = get_next_token(file);
    return result;
}
