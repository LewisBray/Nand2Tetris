#include "vm_file.h"
#include "error.h"

#include <ctype.h>

static bool is_valid_identifier_char(const char c) {
    return isalnum(c) || (c == '_') || (c == '.') || (c == '$') || (c == ':');
}

static bool is_valid_identifier(const String s) {
    for (int i = 0; i < s.count; ++i) {
        if (!is_valid_identifier_char(s.data[i])) {
            return false;
        }
    }
    
    return true;
}

static Token get_next_token(VMFile* const file) {
    while (file->position != file->end) {
        bool skipped_whitespace = false;
        while (file->position != file->end && isspace(*file->position)) {
            skipped_whitespace = true;
            ++file->position;
        }
        
        bool skipped_comment = false;
        if (file->position != file->end && *file->position == '/' && (file->position + 1) != file->end && file->position[1] == '/') {
            while (file->position != file->end && *file->position != '\n') {
                skipped_comment = true;
                ++file->position;
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
    result.text.data = file->position;
    if (isalpha(*result.text.data)) {
        result.type = TT_IDENTIFIER;
        while (file->position != file->end && !isspace(*file->position)) {
            ++result.text.count;
            ++file->position;
        }
    } else if (isdigit(*result.text.data)) {
        result.type = TT_INT;
        while (file->position != file->end && !isspace(*file->position)) {
            if (!isdigit(*file->position)) {
                ERROR("Integers must contain only digits");
            }
            
            ++result.text.count;
            ++file->position;
        }
    } else {
        ERROR("Encountered token that isn't alphanumeric or integer");
    }
    
    return result;
}

static Segment parse_segment(const String text) {
    if (STRING_EQUALS_C_STRING(text, "argument")) {
        return SEG_ARGUMENT;
    } else if (STRING_EQUALS_C_STRING(text, "local")) {
        return SEG_LOCAL;
    } else if (STRING_EQUALS_C_STRING(text, "static")) {
        return SEG_STATIC;
    } else if (STRING_EQUALS_C_STRING(text, "constant")) {
        return SEG_CONSTANT;
    } else if (STRING_EQUALS_C_STRING(text, "this")) {
        return SEG_THIS;
    } else if (STRING_EQUALS_C_STRING(text, "that")) {
        return SEG_THAT;
    } else if (STRING_EQUALS_C_STRING(text, "pointer")) {
        return SEG_POINTER;
    } else if (STRING_EQUALS_C_STRING(text, "temp")) {
        return SEG_TEMP;
    } else {
        ERROR("Unexpected memory segment name");
    }
}

static Command parse_command(const String name, VMFile* const file) {
    Command command = {};
    if (STRING_EQUALS_C_STRING(name, "push")) {
        command.type = CMD_PUSH;
        const Token segment_token = get_next_token(file);
        if (segment_token.type != TT_IDENTIFIER) {
            ERROR("Expected alpha identifier for push segment");
        }
        
        command.push.segment = parse_segment(segment_token.text);
        
        const Token index_token = get_next_token(file);
        if (index_token.type != TT_INT) {
            ERROR("Expected int for push segment index");
        }
        
        command.push.index = index_token.text;
    } else if (STRING_EQUALS_C_STRING(name, "pop")) {
        command.type = CMD_POP;
        const Token segment_token = get_next_token(file);
        if (segment_token.type != TT_IDENTIFIER) {
            ERROR("Expected alpha identifier for pop segment");
        }
        
        command.pop.segment = parse_segment(segment_token.text);
        
        const Token index_token = get_next_token(file);
        if (index_token.type != TT_INT) {
            ERROR("Expected int for pop segment index");
        }
        
        command.pop.index = index_token.text;
    } else if (STRING_EQUALS_C_STRING(name, "add")) {
        command.type = CMD_ADD;
    } else if (STRING_EQUALS_C_STRING(name, "sub")) {
        command.type = CMD_SUB;
    } else if (STRING_EQUALS_C_STRING(name, "neg")) {
        command.type = CMD_NEG;
    } else if (STRING_EQUALS_C_STRING(name, "eq")) {
        command.type = CMD_EQ;
    } else if (STRING_EQUALS_C_STRING(name, "gt")) {
        command.type = CMD_GT;
    } else if (STRING_EQUALS_C_STRING(name, "lt")) {
        command.type = CMD_LT;
    } else if (STRING_EQUALS_C_STRING(name, "and")) {
        command.type = CMD_AND;
    } else if (STRING_EQUALS_C_STRING(name, "or")) {
        command.type = CMD_OR;
    } else if (STRING_EQUALS_C_STRING(name, "not")) {
        command.type = CMD_NOT;
    } else if (STRING_EQUALS_C_STRING(name, "label")) {
        command.type = CMD_LABEL;
        const Token name_token = get_next_token(file);
        if (name_token.type != TT_IDENTIFIER) {
            ERROR("Expected identifier for loop label");
        }
        
        if (!is_valid_identifier(name_token.text)) {
            ERROR("Invalid character encountered in loop label");
        }
        
        command.label.name = name_token.text;
    } else if (STRING_EQUALS_C_STRING(name, "goto")) {
        command.type = CMD_GOTO;
        const Token label_token = get_next_token(file);
        if (label_token.type != TT_IDENTIFIER) {
            ERROR("Expected identifier for goto label");
        }
        
        if (!is_valid_identifier(label_token.text)) {
            ERROR("Invalid character encountered in goto label");
        }
        
        command.go_to.label = label_token.text;
    } else if (STRING_EQUALS_C_STRING(name, "if-goto")) {
        command.type = CMD_IF_GOTO;
        const Token label_token = get_next_token(file);
        if (label_token.type != TT_IDENTIFIER) {
            ERROR("Expected identifier for if-goto label");
        }
        
        if (!is_valid_identifier(label_token.text)) {
            ERROR("Invalid character encountered in if-goto label");
        }
        
        command.if_go_to.label = label_token.text;
    } else if (STRING_EQUALS_C_STRING(name, "function")) {
        command.type = CMD_FUNCTION;
        const Token name_token = get_next_token(file);
        if (name_token.type != TT_IDENTIFIER) {
            ERROR("Expected identifier for function name");
        }
        
        if (!is_valid_identifier(name_token.text)) {
            ERROR("Invalid character encountered in function name");
        }
        
        command.function.name = name_token.text;
        
        const Token local_var_count_token = get_next_token(file);
        if (local_var_count_token.type != TT_INT) {
            ERROR("Expected int for function local var count");
        }
        
        command.function.local_var_count = string_to_int(local_var_count_token.text);
    } else if (STRING_EQUALS_C_STRING(name, "call")) {
        command.type = CMD_CALL;
        const Token function_token = get_next_token(file);
        if (function_token.type != TT_IDENTIFIER) {
            ERROR("Expected identifier for function name in call command");
        }
        
        command.call.function = function_token.text;
        
        const Token arg_count_token = get_next_token(file);
        if (arg_count_token.type != TT_INT) {
            ERROR("Expected int for function arg count in call command");
        }
        
        command.call.arg_count = string_to_int(arg_count_token.text);
    } else if (STRING_EQUALS_C_STRING(name, "return")) {
        command.type = CMD_RETURN;
    } else {
        ERROR("Unsupported command type");
    }
    
    return command;
}
