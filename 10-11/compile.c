#include "compile.h"
#include "string_view.h"
#include "error.h"

#include <assert.h>

typedef enum SymbolKind {
    SK_LOCAL,
    SK_ARGUMENT,
    SK_FIELD,
    SK_STATIC,
    SK_COUNT
} SymbolKind;

typedef struct SymbolTable {
    String symbol_names[SK_COUNT][32];
    String symbol_types[SK_COUNT][32];
    int symbol_count[SK_COUNT];
} SymbolTable;

// NOTE: not doing any duplicate symbol checking, could do a name check for each symbol kind
static void add_symbol(SymbolTable* const table, const String name, const String type, const SymbolKind kind) {
    int* const symbol_count = table->symbol_count + kind;
    for (int i = 0; i < *symbol_count; ++i) {
        if (string_equals(name, table->symbol_names[kind][i])) {
            return;
        }
    }
    
    table->symbol_names[kind][*symbol_count] = name;
    table->symbol_types[kind][*symbol_count] = type;
    ++(*symbol_count);
}

static void remove_symbols(SymbolTable* const table, const SymbolKind kind) {
    for (int i = 0; i < table->symbol_count[kind]; ++i) {
        String* const name = &table->symbol_names[kind][i];
        name->data = 0;
        name->count = 0;
        
        String* const type = &table->symbol_types[kind][i];
        type->data = 0;
        type->count = 0;
    }
    
    table->symbol_count[kind] = 0;
}

typedef struct Symbol {
    String name;
    String type;
    SymbolKind kind;
    int index;
} Symbol;

typedef struct GetSymbolResult {
    Symbol symbol;
    bool found;
} GetSymbolResult;

static GetSymbolResult get_symbol(const SymbolTable* const table, const String name) {
    GetSymbolResult result = {};
    for (int kind = 0; kind < SK_COUNT; ++kind) {
        for (int i = 0; i < table->symbol_count[kind]; ++i) {
            if (string_equals(name, table->symbol_names[kind][i])) {
                result.symbol.name = name;
                result.symbol.type = table->symbol_types[kind][i];
                result.symbol.kind = kind;
                result.symbol.index = i;
                result.found = true;
                break;
            }
        }
    }
    
    return result;
}

static SymbolKind get_class_var_symbol_kind_from_token_type(const TokenType token_type) {
    SymbolKind kind = 0;
    switch (token_type) {
        case TT_STATIC: {
            kind = SK_STATIC;
        } break;
        
        case TT_FIELD: {
            kind = SK_FIELD;
        } break;
        
        default: {
            assert(false);
        }
    }
    
    return kind;
}

static void write_c_string(const char* const s, Output* const out) {
#ifdef OUTPUT_XML
    fputs(s, out->xml);
#endif
}

static void write_integer_constant(const String constant, Output* const out) {
#ifdef OUTPUT_XML
    fprintf(out->xml, "<integerConstant> %.*s </integerConstant>", constant.count, constant.data);
#endif
}

static void write_string_constant(const String constant, Output* const out) {
#ifdef OUTPUT_XML
    fprintf(out->xml, "<stringConstant> %.*s </stringConstant>", constant.count, constant.data);
#endif
}

static void write_symbol(const Token symbol_token, Output* const out) {
#ifdef OUTPUT_XML
    switch (symbol_token.type) {
        case TT_OPEN_ANGLE_BRACKET: {
            fputs("<symbol> &lt; </symbol>", out->xml);
        } break;
        
        case TT_CLOSE_ANGLE_BRACKET: {
            fputs("<symbol> &gt; </symbol>", out->xml);
        } break;
        
        case TT_AMPERSAND: {
            fputs("<symbol> &amp; </symbol>", out->xml);
        } break;
        
        default: {
            const String symbol = symbol_token.text;
            fprintf(out->xml, "<symbol> %.*s </symbol>", symbol.count, symbol.data);
        } break;
    }
#endif
}

static void write_identifier(const String identifier, Output* const out) {
#ifdef OUTPUT_XML
    fprintf(out->xml, "<identifier> %.*s </identifier>", identifier.count, identifier.data);
#endif
}

static void write_keyword(const String keyword, Output* const out) {
#ifdef OUTPUT_XML
    fprintf(out->xml, "<keyword> %.*s </keyword>", keyword.count, keyword.data);
#endif
}

static void write_type(const Token token, Output* const out) {
#ifdef OUTPUT_XML
    if (token.type == TT_IDENTIFIER) {
        write_identifier(token.text, out);
    } else {
        write_keyword(token.text, out);
    }
#endif
}

// NOTE: global state that should be set upon every call to 'compile'
// in a more serious project this would be passed in state but this
// is so simple and constrained that this will suffice
static int g_label_number = 1;
static String g_class_name = {};

static void parse_expression(JackFile* file, SymbolTable* symbol_table, Output* const out);
static void parse_statements(JackFile* file, SymbolTable* symbol_table, Output* const out);

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

static void parse_class_var_declaration(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(is_class_var_type(file->current_token.type));
    
    write_c_string("<classVarDec>", out);
    
    const Token var_kind_token = consume_token(file);
    write_keyword(var_kind_token.text, out);
    
    const Token var_type_token = consume_token(file);
    if (!is_type_type(var_type_token.type)) {
        ERROR("Line: %d. Specify a var type of 'int', 'char', 'boolean' or a class name", var_type_token.line);
    }
    
    write_type(var_type_token, out);
    
    const Token var_name_token = consume_token(file);
    if (var_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected identifier for var name", var_name_token.line);
    }
    
    write_identifier(var_name_token.text, out);
    
    const String type = var_type_token.text;
    const SymbolKind kind = get_class_var_symbol_kind_from_token_type(var_kind_token.type);
    add_symbol(symbol_table, var_name_token.text, type, kind);
    
    while (file->current_token.type == TT_COMMA) {
        write_symbol(consume_token(file), out);
        
        const Token additional_var_name_token = consume_token(file);
        if (additional_var_name_token.type != TT_IDENTIFIER) {
            ERROR("Line: %d. Expected identifier for var name", additional_var_name_token.line);
        }
        
        write_identifier(additional_var_name_token.text, out);
        
        add_symbol(symbol_table, additional_var_name_token.text, type, kind);
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. class var declaration must end with ';'", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    write_c_string("</classVarDec>", out);
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

static void parse_parameter_list(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    write_c_string("<parameterList>", out);
    
    if (is_type_type(file->current_token.type)) {
        const Token type_token = consume_token(file);
        write_type(type_token, out);
        
        const Token name_token = consume_token(file);
        if (name_token.type != TT_IDENTIFIER) {
            ERROR("Line: %d. Expected identifier for parameter name", name_token.line);
        }
        
        write_identifier(name_token.text, out);
        
        add_symbol(symbol_table, name_token.text, type_token.text, SK_ARGUMENT);
        
        while (file->current_token.type == TT_COMMA) {
            write_symbol(consume_token(file), out);
            
            const Token additional_type_token = consume_token(file);
            if (!is_type_type(additional_type_token.type)) {
                ERROR("Line: %d. Expected type following ',' in parameter list", additional_type_token.line);
            }
            
            write_type(additional_type_token, out);
            
            const Token additional_name_token = consume_token(file);
            if (additional_name_token.type != TT_IDENTIFIER) {
                ERROR("Line: %d. Expected identifier for parameter name", additional_name_token.line);
            }
            
            write_identifier(additional_name_token.text, out);
            
            add_symbol(symbol_table, additional_name_token.text, additional_type_token.text, SK_ARGUMENT);
        }
    }
    
    write_c_string("</parameterList>", out);
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

static int parse_expression_list(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    write_c_string("<expressionList>", out);
    
    int expression_count = 0;
    if (is_term_type(file->current_token.type)) {   // expression which begins with term
        parse_expression(file, symbol_table, out);
        ++expression_count;
        
        while (file->current_token.type == TT_COMMA) {
            write_symbol(consume_token(file), out);
            parse_expression(file, symbol_table, out);
            ++expression_count;
        }
    }
    
    write_c_string("</expressionList>", out);
    
    return expression_count;
}

static void parse_procedure_invocation(JackFile* const file, SymbolTable* const symbol_table, const String symbol_name, const String subroutine_name, Output* const out) {
    assert(file->current_token.type == TT_OPEN_PAREN);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. subroutine call must have '(' after subroutine name", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    if (symbol_name.count == 0) {
        // internal method call
        fputs("push pointer 0\n", out->vm);
        
        const int argument_count = parse_expression_list(file, symbol_table, out);
        fprintf(out->vm, "call %.*s.%.*s %d\n", g_class_name.count, g_class_name.data, subroutine_name.count, subroutine_name.data, argument_count + 1);
    } else {
        const GetSymbolResult result = get_symbol(symbol_table, symbol_name);
        if (result.found) {
            // external method call
            const Symbol* const symbol = &result.symbol;
            switch (symbol->kind) {
                case SK_LOCAL: {
                    fprintf(out->vm, "push local %d\n", symbol->index);
                } break;
                
                case SK_ARGUMENT: {
                    fprintf(out->vm, "push argument %d\n", symbol->index);
                } break;
                
                case SK_FIELD: {
                    fprintf(out->vm, "push this %d\n", symbol->index);
                } break;
                
                case SK_STATIC: {
                    fprintf(out->vm, "push static %d\n", symbol->index);
                } break;
                
                default: {
                    assert(false);
                } break;
            }
            
            const int argument_count = parse_expression_list(file, symbol_table, out);
            fprintf(out->vm, "call %.*s.%.*s %d\n", symbol->type.count, symbol->type.data, subroutine_name.count, subroutine_name.data, argument_count + 1);
        } else {
            // function call
            const int argument_count = parse_expression_list(file, symbol_table, out);
            fprintf(out->vm, "call %.*s.%.*s %d\n", symbol_name.count, symbol_name.data, subroutine_name.count, subroutine_name.data, argument_count);
        }
    }
    
    const Token close_paren_token = consume_token(file);
    if (close_paren_token.type != TT_CLOSE_PAREN) {
        ERROR("Line: %d. subroutine call must have closing ')'", close_paren_token.line);
    }
    
    write_symbol(close_paren_token, out);
}

static void parse_term(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    write_c_string("<term>", out);
    
    switch (file->current_token.type) {
        case TT_INT_CONST: {
            const Token int_token = consume_token(file);
            write_integer_constant(int_token.text, out);
            fprintf(out->vm, "push constant %.*s\n", int_token.text.count, int_token.text.data);
        } break;
        
        case TT_STRING_CONST: {
            const Token string_token = consume_token(file);
            write_string_constant(string_token.text, out);
            fprintf(out->vm, "push constant %d\n", string_token.text.count);
            fputs("call String.new 1\n", out->vm);
            for (int i = 0; i < string_token.text.count; ++i) {
                const int digit = string_token.text.data[i];
                fprintf(out->vm, "push constant %d\n", digit);
                fputs("call String.appendChar 2\n", out->vm);
            }
        } break;
        
        case TT_TRUE: {
            write_keyword(consume_token(file).text, out);
            fputs("push constant 1\n", out->vm);
            fputs("neg\n", out->vm);
        } break;
        
        case TT_FALSE:
        case TT_NULL: {
            write_keyword(consume_token(file).text, out);
            fputs("push constant 0\n", out->vm);
        } break;
        
        case TT_THIS: {
            write_keyword(consume_token(file).text, out);
            fputs("push pointer 0\n", out->vm);
        } break;
        
        case TT_IDENTIFIER: {
            const Token identifier_token = consume_token(file);
            write_identifier(identifier_token.text, out);
            switch (file->current_token.type) {
                case TT_OPEN_BRACKET: {
                    write_symbol(consume_token(file), out);
                    
                    const GetSymbolResult result = get_symbol(symbol_table, identifier_token.text);
                    if (!result.found) {
                        ERROR("Line: %d. Symbol referenced in term that wasn't declared", identifier_token.line);
                    }
                    
                    const Symbol* const symbol = &result.symbol;
                    switch (symbol->kind) {
                        case SK_LOCAL: {
                            fprintf(out->vm, "push local %d\n", symbol->index);
                        } break;
                        
                        case SK_ARGUMENT: {
                            fprintf(out->vm, "push argument %d\n", symbol->index);
                        } break;
                        
                        case SK_FIELD: {
                            fprintf(out->vm, "push this %d\n", symbol->index);
                        } break;
                        
                        case SK_STATIC: {
                            fprintf(out->vm, "push static %d\n", symbol->index);
                        } break;
                        
                        default: {
                            assert(false);
                        } break;
                    }
                    
                    parse_expression(file, symbol_table, out);
                    
                    const Token close_bracket_token = consume_token(file);
                    if (close_bracket_token.type != TT_CLOSE_BRACKET) {
                        ERROR("Line: %d. '[' must be closed with ']'", close_bracket_token.line);
                    }
                    
                    write_symbol(close_bracket_token, out);
                    
                    fputs("add\n", out->vm);
                    fputs("pop pointer 1\n", out->vm);
                    fputs("push that 0\n", out->vm);
                } break;
                
                case TT_OPEN_BRACE: {
                    const String symbol_name = {};
                    const String subroutine_name = identifier_token.text;
                    parse_procedure_invocation(file, symbol_table, symbol_name, subroutine_name, out);
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
                    
                    const String symbol_name = identifier_token.text;
                    const String subroutine_name = subroutine_name_token.text;
                    parse_procedure_invocation(file, symbol_table, symbol_name, subroutine_name, out);
                } break;
                
                case TT_EOF: {
                    ERROR("Unexpected end of file during expression");
                } break;
                
                default: {
                    // expression is just the identifier
                    const GetSymbolResult result = get_symbol(symbol_table, identifier_token.text);
                    if (!result.found) {
                        ERROR("Line: %d. Symbol referenced in term that wasn't declared", identifier_token.line);
                    }
                    
                    const Symbol* const symbol = &result.symbol;
                    switch (symbol->kind) {
                        case SK_LOCAL: {
                            fprintf(out->vm, "push local %d\n", symbol->index);
                        } break;
                        
                        case SK_ARGUMENT: {
                            fprintf(out->vm, "push argument %d\n", symbol->index);
                        } break;
                        
                        case SK_FIELD: {
                            fprintf(out->vm, "push this %d\n", symbol->index);
                        } break;
                        
                        case SK_STATIC: {
                            fprintf(out->vm, "push static %d\n", symbol->index);
                        } break;
                        
                        default: {
                            assert(false);
                        } break;
                    }
                } break;
            }
        } break;
        
        case TT_OPEN_PAREN: {
            write_symbol(consume_token(file), out);
            parse_expression(file, symbol_table, out);
            
            const Token close_paren_token = consume_token(file);
            if (close_paren_token.type != TT_CLOSE_PAREN) {
                ERROR("Line: %d. '(' must be closed with ')'", close_paren_token.line);
            }
            
            write_symbol(close_paren_token, out);
        } break;
        
        case TT_MINUS: {
            write_symbol(consume_token(file), out);
            parse_term(file, symbol_table, out);
            fputs("neg\n", out->vm);
        } break;
        
        case TT_TILDE: {
            write_symbol(consume_token(file), out);
            parse_term(file, symbol_table, out);
            fputs("not\n", out->vm);
        } break;
        
        default: {
        } break;
    }
    
    write_c_string("</term>", out);
}

static void parse_expression(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    write_c_string("<expression>", out);
    
    parse_term(file, symbol_table, out);
    
    bool parsing = true;
    while (parsing) {
        switch (file->current_token.type) {
            case TT_PLUS: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("add\n", out->vm);
            } break;
            
            case TT_MINUS: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("sub\n", out->vm);
            } break;
            
            case TT_ASTERISK: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("call Math.multiply 2\n", out->vm);
            } break;
            
            case TT_SLASH: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("call Math.divide 2\n", out->vm);
            } break;
            
            case TT_AMPERSAND: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("and\n", out->vm);
            } break;
            
            case TT_PIPE: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("or\n", out->vm);
            } break;
            
            case TT_OPEN_ANGLE_BRACKET: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("lt\n", out->vm);
            } break;
            
            case TT_CLOSE_ANGLE_BRACKET: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("gt\n", out->vm);
            } break;
            
            case TT_EQUALS: {
                write_symbol(consume_token(file), out);
                parse_term(file, symbol_table, out);
                fputs("eq\n", out->vm);
            } break;
            
            default: {
                // nothing further to do, no binary ops
                parsing = false;
            } break;
        }
    }
    
    write_c_string("</expression>", out);
}

static void parse_let_statement(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_LET);
    
    write_c_string("<letStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token var_name_token = consume_token(file);
    if (var_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected identifier for var name in let statement", var_name_token.line);
    }
    
    write_identifier(var_name_token.text, out);
    
    const GetSymbolResult result = get_symbol(symbol_table, var_name_token.text);
    if (!result.found) {
        ERROR("Line: %d. Symbol referenced in let statement that wasn't declared", var_name_token.line);
    }
    
    const Symbol* const symbol = &result.symbol;
    
    bool is_array_element_assignment = false;
    if (file->current_token.type == TT_OPEN_BRACKET) {
        write_symbol(consume_token(file), out);
        
        switch (symbol->kind) {
            case SK_LOCAL: {
                fprintf(out->vm, "push local %d\n", symbol->index);
            } break;
            
            case SK_ARGUMENT: {
                fprintf(out->vm, "push argument %d\n", symbol->index);
            } break;
            
            case SK_FIELD: {
                fprintf(out->vm, "push this %d\n", symbol->index);
            } break;
            
            case SK_STATIC: {
                fprintf(out->vm, "push static %d\n", symbol->index);
            } break;
            
            default: {
                assert(false);
            } break;
        }
        
        parse_expression(file, symbol_table, out);
        
        const Token close_bracket_token = consume_token(file);
        if (close_bracket_token.type != TT_CLOSE_BRACKET) {
            ERROR("Line: %d. '[' must have matching ']' after enclosed expresion", close_bracket_token.line);
        }
        
        write_symbol(close_bracket_token, out);
        
        // put assignment address on stack
        fputs("add\n", out->vm);
        
        is_array_element_assignment = true;
    }
    
    const Token equals_token = consume_token(file);
    if (equals_token.type != TT_EQUALS) {
        ERROR("Line: %d. Missing '=' in let statement", equals_token.line);
    }
    
    write_symbol(equals_token, out);
    
    parse_expression(file, symbol_table, out);
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. let statement must end with ';'", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    write_c_string("</letStatement>", out);
    
    if (is_array_element_assignment) {
        // expression value is on stack so store in temp to get at assignment address
        fputs("pop temp 0\n", out->vm);
        fputs("pop pointer 1\n", out->vm);
        
        // now put expression value from temp storage into assignment address
        fputs("push temp 0\n", out->vm);
        fputs("pop that 0\n", out->vm);
    } else {
        switch (symbol->kind) {
            case SK_LOCAL: {
                fprintf(out->vm, "pop local %d\n", symbol->index);
            } break;
            
            case SK_ARGUMENT: {
                fprintf(out->vm, "pop argument %d\n", symbol->index);
            } break;
            
            case SK_FIELD: {
                fprintf(out->vm, "pop this %d\n", symbol->index);
            } break;
            
            case SK_STATIC: {
                fprintf(out->vm, "pop static %d\n", symbol->index);
            } break;
            
            default: {
                assert(false);
            } break;
        }
    }
}

static void parse_if_statement(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_IF);
    
    write_c_string("<ifStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. if keyword must be followed by '('", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    parse_expression(file, symbol_table, out);
    
    const int if_label_number = g_label_number++;
    
    fputs("not\n", out->vm);
    fprintf(out->vm, "if-goto L%d\n", if_label_number);
    
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
    
    parse_statements(file, symbol_table, out);
    
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
        
        const int else_label_number = g_label_number++;
        
        fprintf(out->vm, "goto L%d\n", else_label_number);
        fprintf(out->vm, "label L%d\n", if_label_number);
        
        parse_statements(file, symbol_table, out);
        
        const Token else_close_brace_token = consume_token(file);
        if (else_close_brace_token.type != TT_CLOSE_BRACE) {
            ERROR("Line: %d. Missing '}' after statements in else clause", else_close_brace_token.line);
        }
        
        write_symbol(else_close_brace_token, out);
        
        fprintf(out->vm, "label L%d\n", else_label_number);
    }
    else
    {
        fprintf(out->vm, "label L%d\n", if_label_number);
    }
    
    write_c_string("</ifStatement>", out);
}

static void parse_while_statement(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_WHILE);
    
    write_c_string("<whileStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. while keyword must be followed by '('", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    const int start_label_number = g_label_number++;
    const int end_label_number = g_label_number++;
    
    fprintf(out->vm, "label L%d\n", start_label_number);
    
    parse_expression(file, symbol_table, out);
    
    fputs("not\n", out->vm);
    fprintf(out->vm, "if-goto L%d\n", end_label_number);
    
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
    
    parse_statements(file, symbol_table, out);
    
    fprintf(out->vm, "goto L%d\n", start_label_number);
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. Missing '}' after statements in while statement", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    fprintf(out->vm, "label L%d\n", end_label_number);
    
    write_c_string("</whileStatement>", out);
}

static void parse_do_statement(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_DO);
    
    write_c_string("<doStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token identifier_token = consume_token(file); // could be function or class/var name
    if (identifier_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Either class, variable or procedure name must follow do keyword", identifier_token.line);
    }
    
    write_identifier(identifier_token.text, out);
    
    switch (file->current_token.type) {
        case TT_OPEN_PAREN: {
            const String symbol_name = {};
            const String subroutine_name = identifier_token.text;
            parse_procedure_invocation(file, symbol_table, symbol_name, subroutine_name, out);
            fputs("pop temp 0\n", out->vm);
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
            
            const String symbol_name = identifier_token.text;
            const String subroutine_name = subroutine_name_token.text;
            parse_procedure_invocation(file, symbol_table, symbol_name, subroutine_name, out);
            fputs("pop temp 0\n", out->vm);
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
    
    write_c_string("</doStatement>", out);
}

static void parse_return_statement(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_RETURN);
    
    write_c_string("<returnStatement>", out);
    
    write_keyword(consume_token(file).text, out);
    
    if (is_term_type(file->current_token.type)) {
        parse_expression(file, symbol_table, out);
    } else {
        // assume void return type
        fputs("push constant 0\n", out->vm);
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. Missing ';' at end of return statement", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    fputs("return\n", out->vm);
    
    write_c_string("</returnStatement>", out);
}

static void parse_statements(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    write_c_string("<statements>", out);
    
    bool parsing = true;
    while (parsing) {
        switch (file->current_token.type) {
            case TT_LET: {
                parse_let_statement(file, symbol_table, out);
            } break;
            
            case TT_IF: {
                parse_if_statement(file, symbol_table, out);
            } break;
            
            case TT_WHILE: {
                parse_while_statement(file, symbol_table, out);
            } break;
            
            case TT_DO: {
                parse_do_statement(file, symbol_table, out);
            } break;
            
            case TT_RETURN: {
                parse_return_statement(file, symbol_table, out);
            } break;
            
            default: {
                parsing = false;
            } break;
        }
    }
    
    write_c_string("</statements>", out);
}

static int parse_var_declaration(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_VAR);
    
    int var_count = 0;
    write_c_string("<varDec>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token var_type_token = consume_token(file);
    if (!is_type_type(var_type_token.type)) {
        ERROR("Line: %d. Missing type specifier for var declaration", var_type_token.line);
    }
    
    write_type(var_type_token, out);
    
    const Token var_name_token = consume_token(file);
    if (var_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Missing identifier for var name", var_name_token.line);
    }
    
    write_identifier(var_name_token.text, out);
    
    const String type = var_type_token.text;
    add_symbol(symbol_table, var_name_token.text, type, SK_LOCAL);
    ++var_count;
    
    while (file->current_token.type == TT_COMMA) {
        write_symbol(consume_token(file), out);
        
        const Token additional_var_name_token = consume_token(file);
        if (additional_var_name_token.type != TT_IDENTIFIER) {
            ERROR("Line: %d. Missing identifier for var name", additional_var_name_token.line);
        }
        
        write_identifier(additional_var_name_token.text, out);
        
        add_symbol(symbol_table, additional_var_name_token.text, type, SK_LOCAL);
        ++var_count;
    }
    
    const Token semi_colon_token = consume_token(file);
    if (semi_colon_token.type != TT_SEMI_COLON) {
        ERROR("Line: %d. var declaration must end with ';'", semi_colon_token.line);
    }
    
    write_symbol(semi_colon_token, out);
    
    write_c_string("</varDec>", out);
    
    return var_count;
}

static void parse_subroutine_declaration(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(is_subroutine_type(file->current_token.type));
    
    write_c_string("<subroutineDec>", out);
    
    const Token subroutine_type_token = consume_token(file);
    write_keyword(subroutine_type_token.text, out);
    
    if (subroutine_type_token.type == TT_METHOD) {
        // add implicit this pointer to symbol table, this pushes arg offsets by 1
        static const char THIS_BUFFER[] = "this";
        String this = {};
        this.data = THIS_BUFFER;
        this.count = sizeof(THIS_BUFFER) - 1;
        add_symbol(symbol_table, this, g_class_name, SK_ARGUMENT);
    }
    
    const Token return_type_token = consume_token(file);
    if (!is_return_type_type(return_type_token.type)) {
        ERROR("Line: %d. Invalid subroutine return type", return_type_token.line);
    }
    
    write_type(return_type_token, out);
    
    const Token subroutine_name_token = consume_token(file);
    if (subroutine_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected identifier for subroutine name", subroutine_name_token.line);
    }
    
    const String subroutine_name = subroutine_name_token.text;
    write_identifier(subroutine_name, out);
    
    const Token open_paren_token = consume_token(file);
    if (open_paren_token.type != TT_OPEN_PAREN) {
        ERROR("Line: %d. subroutine name must be followed by '('", open_paren_token.line);
    }
    
    write_symbol(open_paren_token, out);
    
    parse_parameter_list(file, symbol_table, out);
    
    const Token close_paren_token = consume_token(file);
    if (close_paren_token.type != TT_CLOSE_PAREN) {
        ERROR("Line: %d. Expected ')' in subroutine declaration", close_paren_token.line);
    }
    
    write_symbol(close_paren_token, out);
    
    // parse subroutine body, inlined for convenience of state access in this routine
    const Token open_brace_token = consume_token(file);
    if (open_brace_token.type != TT_OPEN_BRACE) {
        ERROR("Line: %d. subroutine body must start with '{'", open_brace_token.line);
    }
    
    write_c_string("<subroutineBody>", out);
    write_symbol(open_brace_token, out);
    
    int local_var_count = 0;
    while (file->current_token.type == TT_VAR) {
        local_var_count += parse_var_declaration(file, symbol_table, out);
    }
    
    fprintf(out->vm, "function %.*s.%.*s %d\n", g_class_name.count, g_class_name.data, subroutine_name.count, subroutine_name.data, local_var_count);
    switch (subroutine_type_token.type) {
        case TT_CONSTRUCTOR: {
            // allocate space and set 'this' to point to it
            const int object_size = symbol_table->symbol_count[SK_FIELD];
            fprintf(out->vm, "push constant %d\n", object_size);
            fputs("call Memory.alloc 1\n", out->vm);
            fputs("pop pointer 0\n", out->vm);
        } break;
        
        case TT_METHOD: {
            // implicit first arg is 'this' so set this before carrying on
            fputs("push argument 0\n", out->vm);
            fputs("pop pointer 0\n", out->vm);
        } break;
        
        default: {
        } break;
    }
    
    parse_statements(file, symbol_table, out);
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. subroutine body must end with '}'", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    write_c_string("</subroutineBody>", out);
    
    write_c_string("</subroutineDec>", out);
    
    remove_symbols(symbol_table, SK_LOCAL);
    remove_symbols(symbol_table, SK_ARGUMENT);
}

static void parse_class(JackFile* const file, SymbolTable* const symbol_table, Output* const out) {
    assert(file->current_token.type == TT_CLASS);
    
    write_c_string("<class>", out);
    
    write_keyword(consume_token(file).text, out);
    
    const Token class_name_token = consume_token(file);
    if (class_name_token.type != TT_IDENTIFIER) {
        ERROR("Line: %d. Expected class name after 'class' keyword", class_name_token.line);
    }
    
    g_class_name = class_name_token.text;
    write_identifier(g_class_name, out);
    
    const Token open_brace_token = consume_token(file);
    if (open_brace_token.type != TT_OPEN_BRACE) {
        ERROR("Line: %d. class name must be followed by '{'", open_brace_token.line);
    }
    
    write_symbol(open_brace_token, out);
    
    while (is_class_var_type(file->current_token.type)) {
        parse_class_var_declaration(file, symbol_table, out);
    }
    
    while (is_subroutine_type(file->current_token.type)) {
        parse_subroutine_declaration(file, symbol_table, out);
    }
    
    const Token close_brace_token = consume_token(file);
    if (close_brace_token.type != TT_CLOSE_BRACE) {
        ERROR("Line: %d. class declaration must be closed with '}'", close_brace_token.line);
    }
    
    write_symbol(close_brace_token, out);
    
    write_c_string("</class>", out);
}

static void compile(JackFile* const file, Output* const out) {
    SymbolTable symbol_table = {};
    g_label_number = 1;
    g_class_name.data = 0;
    g_class_name.count = 0;
    if (file->current_token.type == TT_CLASS) {
        parse_class(file, &symbol_table, out);
    } else {
        ERROR("Line: %d. Unexpected token type", file->current_token.line);
    }
}