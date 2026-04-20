#include "compile.h"

#include <assert.h>

static void parse_expression(JackFile* file, FILE* out);
static void parse_statements(JackFile* file, FILE* out);

static void write_integer_constant(const String constant, FILE* out) {
    fprintf(out, "<integerConstant> %.*s </integerConstant>", constant.count, constant.data);
}

static void write_string_constant(const String constant, FILE* out) {
    fprintf(out, "<stringConstant> %.*s </stringConstant>", constant.count, constant.data);
}

static void write_symbol(const Token symbol_token, FILE* out) {
    switch (symbol_token.type) {
        case TT_OPEN_ANGLE_BRACKET: {
            fputs("<symbol> &lt; </symbol>", out);
        } break;
        
        case TT_CLOSE_ANGLE_BRACKET: {
            fputs("<symbol> &gt; </symbol>", out);
        } break;
        
        case TT_AMPERSAND: {
            fputs("<symbol> &amp; </symbol>", out);
        } break;
        
        default: {
            const String symbol = symbol_token.text;
            fprintf(out, "<symbol> %.*s </symbol>", symbol.count, symbol.data);
        } break;
    }
}

static void write_identifier(const String identifier, FILE* out) {
    fprintf(out, "<identifier> %.*s </identifier>", identifier.count, identifier.data);
}

static void write_keyword(const String keyword, FILE* out) {
    fprintf(out, "<keyword> %.*s </keyword>", keyword.count, keyword.data);
}

static void write_type(const Token token, FILE* out) {
    if (token.type == TT_IDENTIFIER) {
        write_identifier(token.text, out);
    } else {
        write_keyword(token.text, out);
    }
}

static bool is_class_var_type(const TokenType type) {
    switch (type) {
        case TT_STATIC:
        case TT_FIELD: {
            return true;
        }
        
        default: {
            return false;
        }
    }
}

static bool is_type_type(const TokenType type) {
    switch (type) {
        case TT_INT:
        case TT_CHAR:
        case TT_BOOLEAN:
        case TT_IDENTIFIER: {   // for class name
            return true;
        }
        
        default: {
            return false;
        }
    }
}

static void parse_class_var_declaration(JackFile* const file, FILE* out) {
    assert(is_class_var_type(file->current_token.type));
    
    fputs("<classVarDec>", out);
    write_keyword(consume_token(file).text, out);
    
    const Token type_token = consume_token(file);
    if (!is_type_type(type_token.type)) {
        ERROR("Line: %d. Specify a var type of 'int', 'char', 'boolean' or a class name", type_token.line);
    }
    
    write_type(type_token, out);
    
    const Token var_name_token = consume_token(file);
    if (var_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected identifier for var name", var_name_token.line);
    }
    
    write_identifier(var_name_token.text, out);
    
    while (file->current_token.type == TT_COMMA) {
        write_symbol(consume_token(file), out);
        
        const Token additional_var_name_token = consume_token(file);
        if (additional_var_name_token.type != TT_IDENTIFIER) {
            ERROR("Line: %d. Expected identifier for var name", additional_var_name_token.line);
        }
        
        write_identifier(additional_var_name_token.text, out);
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. class var declaration must end with ';'", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    fputs("</classVarDec>", out);
}

static bool is_subroutine_type(const TokenType type) {
    switch (type) {
        case TT_CONSTRUCTOR:
        case TT_FUNCTION:
        case TT_METHOD: {
            return true;
        }
        
        default: {
            return false;
        }
    }
}

static bool is_return_type_type(const TokenType type) {
    switch (type) {
        case TT_VOID:
        case TT_INT:
        case TT_CHAR:
        case TT_BOOLEAN:
        case TT_IDENTIFIER: {   // for class name
            return true;
        }
        
        default: {
            return false;
        }
    }
}

static void parse_parameter_list(JackFile* const file, FILE* out) {
    fputs("<parameterList>", out);
    
    if (is_type_type(file->current_token.type)) {
        write_type(consume_token(file), out);
        
        const Token var_name_token = consume_token(file);
        if (var_name_token.type != TT_IDENTIFIER) {
            ERROR("Line: %d. Expected identifier for var name", var_name_token.line);
        }
        
        write_identifier(var_name_token.text, out);
        
        while (file->current_token.type == TT_COMMA) {
            write_symbol(consume_token(file), out);
            
            const Token type_token = consume_token(file);
            if (!is_type_type(type_token.type)) {
                ERROR("Line: %d. Expected type following ',' in parameter list", type_token.line);
            }
            
            write_type(type_token, out);
            
            const Token additional_var_name_token = consume_token(file);
            if (additional_var_name_token.type != TT_IDENTIFIER) {
                ERROR("Line: %d. Expected identifier for var name", additional_var_name_token.line);
            }
            
            write_identifier(additional_var_name_token.text, out);
        }
    }
    
    fputs("</parameterList>", out);
}

static bool is_term_type(const TokenType type) {
    switch (type) {
        case TT_INT_CONST:
        case TT_STRING_CONST:
        case TT_TRUE:
        case TT_FALSE:
        case TT_NULL:
        case TT_THIS:
        case TT_IDENTIFIER:
        case TT_OPEN_PAREN:
        case TT_MINUS:
        case TT_TILDE: {
            return true;
        }
        
        default: {
            return false;
        }
    }
}

static void parse_expression_list(JackFile* const file, FILE* out) {
    fputs("<expressionList>", out);
    
    if (is_term_type(file->current_token.type)) {   // expression which begins with term
        parse_expression(file, out);
        
        while (file->current_token.type == TT_COMMA) {
            write_symbol(consume_token(file), out);
            parse_expression(file, out);
        }
    }
    
    fputs("</expressionList>", out);
}

static void parse_procedure_invocation(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_OPEN_PAREN);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. subroutine call must have '(' after subroutine name", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    parse_expression_list(file, out);
    
    const Token close_paren_token = consume_token(file);
    if (close_paren_token.type != TT_CLOSE_PAREN) {
        ERROR("Line: %d. subroutine call must have closing ')'", close_paren_token.line);
    }
    
    write_symbol(close_paren_token, out);
}

static void parse_term(JackFile* const file, FILE* out) {
    fputs("<term>", out);
    
    switch (file->current_token.type) {
        case TT_INT_CONST: {
            write_integer_constant(consume_token(file).text, out);
        } break;
        
        case TT_STRING_CONST: {
            write_string_constant(consume_token(file).text, out);
        } break;
        
        case TT_TRUE:
        case TT_FALSE:
        case TT_NULL:
        case TT_THIS: {
            write_keyword(consume_token(file).text, out);
        } break;
        
        case TT_IDENTIFIER: {
            write_identifier(consume_token(file).text, out);
            switch (file->current_token.type) {
                case TT_OPEN_BRACKET: {
                    write_symbol(consume_token(file), out);
                    parse_expression(file, out);
                    
                    const Token close_bracket_token = consume_token(file);
                    if (close_bracket_token.type != TT_CLOSE_BRACKET) {
                        ERROR("Line: %d. '[' must be closed with ']'", close_bracket_token.line);
                    }
                    
                    write_symbol(close_bracket_token, out);
                } break;
                
                case TT_OPEN_BRACE: {
                    parse_procedure_invocation(file, out);
                } break;
                
                case TT_DOT: {
                    write_symbol(consume_token(file), out);
                    
                    const Token subroutine_name_token = consume_token(file);
                    if (subroutine_name_token.type != TT_IDENTIFIER) {
                        ERROR("Line: %d. Expected identifier for subroutine name", subroutine_name_token.line);
                    }
                    
                    write_identifier(subroutine_name_token.text, out);
                    
                    if (file->current_token.type != TT_OPEN_PAREN) {
                        ERROR("Line: %d. Method invocation must have '('", file->current_token.line);
                    }
                    
                    parse_procedure_invocation(file, out);
                } break;
                
                case TT_EOF: {
                    ERROR("Unexpected end of file during expression");
                } break;
                
                default: {
                    // expression is just the identifier
                } break;
            }
        } break;
        
        case TT_OPEN_PAREN: {
            write_symbol(consume_token(file), out);
            parse_expression(file, out);
            
            const Token close_paren_token = consume_token(file);
            if (close_paren_token.type != TT_CLOSE_PAREN) {
                ERROR("Line: %d. '(' must be closed with ')'", close_paren_token.line);
            }
            
            write_symbol(close_paren_token, out);
        } break;
        
        case TT_MINUS:
        case TT_TILDE: {
            write_symbol(consume_token(file), out);
            parse_term(file, out);
        } break;
        
        default: {
            // ERROR("Line: %d. Unexpected token type at start of expression", token.line);
        } break;
    }
    fputs("</term>", out);
}

static void parse_expression(JackFile* const file, FILE* out) {
    fputs("<expression>", out);
    
    parse_term(file, out);
    
    bool parsing = true;
    while (parsing) {
        switch (file->current_token.type) {
            case TT_PLUS:
            case TT_MINUS:
            case TT_ASTERISK:
            case TT_SLASH:
            case TT_AMPERSAND:
            case TT_PIPE:
            case TT_OPEN_ANGLE_BRACKET:
            case TT_CLOSE_ANGLE_BRACKET:
            case TT_EQUALS: {
                write_symbol(consume_token(file), out);
                parse_term(file, out);
            } break;
            
            default: {
                // nothing further to do, no binary ops
                parsing = false;
            } break;
        }
    }
    
    fputs("</expression>", out);
}

static void parse_let_statement(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_LET);
    
    fputs("<letStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token var_name_token = consume_token(file);
    if (var_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected identifier for var name in let statement", var_name_token.line);
    }
    
    write_identifier(var_name_token.text, out);
    
    if (file->current_token.type == TT_OPEN_BRACKET) {
        write_symbol(consume_token(file), out);
        
        parse_expression(file, out);
        
        const Token close_bracket_token = consume_token(file);
        if (close_bracket_token.type != TT_CLOSE_BRACKET) {
            ERROR("Line: %d. '[' must have matching ']' after enclosed expresion", close_bracket_token.line);
        }
        
        write_symbol(close_bracket_token, out);
    }
    
    const Token equals_token = consume_token(file);
    if (equals_token.type != TT_EQUALS) {
        ERROR("Line: %d. Missing '=' in let statement", equals_token.line);
    }
    
    write_symbol(equals_token, out);
    
    parse_expression(file, out);
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. let statement must end with ';'", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    fputs("</letStatement>", out);
}

static void parse_if_statement(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_IF);
    
    fputs("<ifStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. if keyword must be followed by '('", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    parse_expression(file, out);
    
    const Token close_paren_token = consume_token(file);
    if (close_paren_token.type != TT_CLOSE_PAREN) {
        ERROR("Line: %d. Missing ')' in if statement", close_paren_token.line);
    }
    
    write_symbol(close_paren_token, out);
    
    const Token open_brace_token = consume_token(file);
    if (open_brace_token.type != TT_OPEN_BRACE) {
        ERROR("Line: %d. Missing '{' in if statement before expressions", open_brace_token.line);
    }
    
    write_symbol(open_brace_token, out);
    
    parse_statements(file, out);
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. Missing '}' after statements in if statement", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    if (file->current_token.type == TT_ELSE) {
        write_keyword(consume_token(file).text, out);
        
        const Token else_open_brace_token = consume_token(file);
        if (else_open_brace_token.type != TT_OPEN_BRACE) {
            ERROR("Line: %d. else keyword must be followed by '{'", else_open_brace_token.line);
        }
        
        write_symbol(else_open_brace_token, out);
        
        parse_statements(file, out);
        
        const Token else_close_brace_token = consume_token(file);
        if (else_close_brace_token.type != TT_CLOSE_BRACE) {
            ERROR("Line: %d. Missing '}' after statements in else clause", else_close_brace_token.line);
        }
        
        write_symbol(else_close_brace_token, out);
    }
    
    fputs("</ifStatement>", out);
}

static void parse_while_statement(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_WHILE);
    
    fputs("<whileStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. while keyword must be followed by '('", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    parse_expression(file, out);
    
    const Token close_paren_token = consume_token(file);
    if (close_paren_token.type != TT_CLOSE_PAREN) {
        ERROR("Line: %d. Missing ')' in while statement", close_paren_token.line);
    }
    
    write_symbol(close_paren_token, out);
    
    const Token open_brace_token = consume_token(file);
    if (open_brace_token.type != TT_OPEN_BRACE) {
        ERROR("Line: %d. Missing '{' in while statement before expressions", open_brace_token.line);
    }
    
    write_symbol(open_brace_token, out);
    
    parse_statements(file, out);
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. Missing '}' after statements in while statement", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    fputs("</whileStatement>", out);
}

static void parse_do_statement(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_DO);
    
    fputs("<doStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token identifier_token = consume_token(file); // could be function or class/var name
    if (identifier_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Either class, variable or procedure name must follow do keyword", identifier_token.line);
    }
    
    write_identifier(identifier_token.text, out);
    
    switch (file->current_token.type) {
        case TT_OPEN_PAREN: {
            parse_procedure_invocation(file, out);
        } break;
        
        case TT_DOT: {
            write_symbol(consume_token(file), out);
            
            const Token subroutine_name_token = consume_token(file);
            if (subroutine_name_token.type != TT_IDENTIFIER) {
                ERROR("Line: %d. Expected identifier for subroutine name", subroutine_name_token.line);
            }
            
            write_identifier(subroutine_name_token.text, out);
            
            if (file->current_token.type != TT_OPEN_PAREN) {
                ERROR("Line: %d. Method invocation must have '('", file->current_token.line);
            }
            
            parse_procedure_invocation(file, out);
        } break;
        
        default: {
            ERROR("Line: %d. Expected procedure invocation in do statement", file->current_token.line);
        } break;
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. Missing ';' at end of do statement", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    fputs("</doStatement>", out);
}

static void parse_return_statement(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_RETURN);
    
    fputs("<returnStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    if (is_term_type(file->current_token.type)) {
        parse_expression(file, out);
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. Missing ';' at end of return statement", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    fputs("</returnStatement>", out);
}

static void parse_statements(JackFile* const file, FILE* out) {
    fputs("<statements>", out);
    
    bool parsing = true;
    while (parsing) {
        switch (file->current_token.type) {
            case TT_LET: {
                parse_let_statement(file, out);
            } break;
            
            case TT_IF: {
                parse_if_statement(file, out);
            } break;
            
            case TT_WHILE: {
                parse_while_statement(file, out);
            } break;
            
            case TT_DO: {
                parse_do_statement(file, out);
            } break;
            
            case TT_RETURN: {
                parse_return_statement(file, out);
            } break;
            
            default: {
                parsing = false;
            } break;
        }
    }
    
    fputs("</statements>", out);
}

static void parse_var_declaration(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_VAR);
    
    fputs("<varDec>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token type_token = consume_token(file);
    if (!is_type_type(type_token.type)) {
        ERROR("Line: %d. Missing type specifier for var declaration", type_token.line);
    }
    
    write_type(type_token, out);
    
    const Token var_name_token = consume_token(file);
    if (var_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Missing identifier for var name", var_name_token.line);
    }
    
    write_identifier(var_name_token.text, out);
    
    while (file->current_token.type == TT_COMMA) {
        write_symbol(consume_token(file), out);
        
        const Token additional_var_name_token = consume_token(file);
        if (additional_var_name_token.type != TT_IDENTIFIER) {
            ERROR("Line: %d. Missing identifier for var name", additional_var_name_token.line);
        }
        
        write_identifier(additional_var_name_token.text, out);
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. var declaration must end with ';'", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    fputs("</varDec>", out);
}

static void parse_subroutine_body(JackFile* const file, FILE* out) {
    const Token open_brace_token = consume_token(file);
    if (open_brace_token.type != TT_OPEN_BRACE) {
        ERROR("Line: %d. subroutine body must start with '{'", open_brace_token.line);
    }
    
    fputs("<subroutineBody>", out);
    write_symbol(open_brace_token, out);
    
    while (file->current_token.type == TT_VAR) {
        parse_var_declaration(file, out);
    }
    
    parse_statements(file, out);
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. subroutine body must end with '}'", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    fputs("</subroutineBody>", out);
}

static void parse_subroutine_declaration(JackFile* const file, FILE* out) {
    assert(is_subroutine_type(file->current_token.type));
    
    fputs("<subroutineDec>", out);
    write_keyword(consume_token(file).text, out);
    
    const Token return_type_token = consume_token(file);
    if (!is_return_type_type(return_type_token.type)) {
        ERROR("Line: %d. Invalid subroutine return type", return_type_token.line);
    }
    
    write_type(return_type_token, out);
    
    const Token subroutine_name_token = consume_token(file);
    if (subroutine_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected identifier for subroutine name", subroutine_name_token.line);
    }
    
    write_identifier(subroutine_name_token.text, out);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. subroutine name must be followed by '('", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    parse_parameter_list(file, out);
    
    const Token close_paren_token = consume_token(file);
    if (close_paren_token.type != TT_CLOSE_PAREN) {
        ERROR("Line: %d. Expected ')' in subroutine declaration", close_paren_token.line);
    }
    
    write_symbol(close_paren_token, out);
    
    parse_subroutine_body(file, out);
    
    fputs("</subroutineDec>", out);
}

static void parse_class(JackFile* const file, FILE* out) {
    assert(file->current_token.type == TT_CLASS);
    
    fputs("<class>", out);
    write_keyword(consume_token(file).text, out);
    
    const Token class_name_token = consume_token(file);
    if (class_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected class name after 'class' keyword", class_name_token.line);
    }
    
    write_identifier(class_name_token.text, out);
    
    const Token open_brace_token = consume_token(file);
    if (open_brace_token.type != TT_OPEN_BRACE) {
        ERROR("Line: %d. class name must be followed by '{'", open_brace_token.line);
    }
    
    write_symbol(open_brace_token, out);
    
    while (is_class_var_type(file->current_token.type)) {
        parse_class_var_declaration(file, out);
    }
    
    while (is_subroutine_type(file->current_token.type)) {
        parse_subroutine_declaration(file, out);
    }
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. class declaration must be closed with '}'", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    fputs("</class>", out);
}

static void compile(JackFile* const file, FILE* out) {
    if (file->current_token.type == TT_CLASS) {
        parse_class(file, out);
    } else {
        ERROR("Line: %d. Unexpected token type", file->current_token.line);
    }
}