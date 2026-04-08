#include "parse.h"
#include "error.h"

typedef struct TokenStream {
    const Token* start;
    const Token* end;
    const Token* current;
} TokenStream;

static const Token* consume_token(TokenStream* const token_stream) {
    return token_stream->current++;
}

static void skip_token(TokenStream* const token_stream) {
    token_stream->current++;
}

static TokenType peek_next_type(const TokenStream* const token_stream) {
    return token_stream->current[1].type;
}

static AInstruction parse_a_instruction(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_AT);
    
    skip_token(token_stream);
    AInstruction instruction = {};
    const Token* const token = consume_token(token_stream);
    switch (token->type) {
        case TT_INT: {
            const int address = string_to_int(token->text);
            if (address >= 0x8000) {
                ERROR("Encountered literal address %d which is outside of memory address range", address);
            }
            
            instruction.address = address;
        } break;
        
        case TT_IDENTIFIER: {
            instruction.symbol = token->text;
        } break;
        
        default: {
            ERROR("Encountered invalid token in a-instruction specification");
        } break;
    }
    
    return instruction;
}

static uint16_t parse_not_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_NOT);
    
    uint16_t result = 0;
    skip_token(token_stream);
    const Token* const token = consume_token(token_stream);
    switch (token->type) {
        case TT_IDENTIFIER: {
            if (STRING_EQUALS_C_STRING(token->text, "A")) {
                result = 0b0110001;
            } else if (STRING_EQUALS_C_STRING(token->text, "M")) {
                result = 0b1110001;
            } else if (STRING_EQUALS_C_STRING(token->text, "D")) {
                result = 0b0001101;
            } else {
                ERROR("Operand to ! c-instruction must be A, M or D");
            }
        } break;
        
        case TT_EOF: {
            ERROR("Unexpected end of file during ! c-instruction");
        } break;
        
        default: {
            ERROR("Operand to ! c-instruction must be A, M or D");
        } break;
    }
    
    return result;
}

static uint16_t parse_integer_literal_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_INT);
    
    const Token* const current = consume_token(token_stream);
    if (STRING_EQUALS_C_STRING(current->text, "0")) {
        return 0b0101010;
    } else if (STRING_EQUALS_C_STRING(current->text, "1")) {
        return 0b0111111;
    } else {
        ERROR("c-instructions can only have 0 or 1 as integer literals");
    }
}

static uint16_t parse_identity_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_IDENTIFIER);
    
    const Token* const token = consume_token(token_stream);
    if (STRING_EQUALS_C_STRING(token->text, "A")) {
        return 0b0110000;
    } else if (STRING_EQUALS_C_STRING(token->text, "M")) {
        return 0b1110000;
    } else if (STRING_EQUALS_C_STRING(token->text, "D")) {
        return 0b0001100;
    } else {
        ERROR("Only A, M or D can be used in Identity c-instruction");
    }
}

static uint16_t parse_negation_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_MINUS);
    
    skip_token(token_stream);
    const Token* const token = consume_token(token_stream);
    switch (token->type) {
        case TT_INT: {
            if (STRING_EQUALS_C_STRING(token->text, "1")) {
                return 0b0111010;
            } else {
                ERROR("Only 1 can be used as integer literal in - c-instruction");
            }
        } break;
        
        case TT_IDENTIFIER: {
            if (STRING_EQUALS_C_STRING(token->text, "A")) {
                return 0b0110011;
            } else if (STRING_EQUALS_C_STRING(token->text, "M")) {
                return 0b1110011;
            } else if (STRING_EQUALS_C_STRING(token->text, "D")) {
                return 0b0001111;
            } else {
                ERROR("Operand to - c-instruction must be A, M or D");
            }
        } break;
        
        case TT_EOF: {
            ERROR("Unexpected end of file during c-instruction");
        } break;
        
        default: {
            ERROR("Only 1, A, M or D can be used for - c-instruction");
        } break;
    }
}

static uint16_t parse_addition_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_IDENTIFIER);
    assert(peek_next_type(token_stream) == TT_PLUS);
    
    const Token* const lhs = consume_token(token_stream);
    skip_token(token_stream);   // skip '+' token
    const Token* const rhs = consume_token(token_stream);
    if (STRING_EQUALS_C_STRING(lhs->text, "A")) {
        if (rhs->type == TT_INT && STRING_EQUALS_C_STRING(rhs->text, "1")) {
            return 0b0110111;
        } else {
            ERROR("Only 1 can be added to A in c-instruction");
        }
    } else if (STRING_EQUALS_C_STRING(lhs->text, "D")) {
        switch (rhs->type) {
            case TT_INT: {
                if (STRING_EQUALS_C_STRING(rhs->text, "1")) {
                    return 0b0011111;
                } else {
                    ERROR("Only integer literal 1 can be added to D in c-instruction");
                }
            } break;
            
            case TT_IDENTIFIER: {
                if (STRING_EQUALS_C_STRING(rhs->text, "A")) {
                    return 0b0000010;
                } else if (STRING_EQUALS_C_STRING(rhs->text, "M")) {
                    return 0b1000010;
                } else {
                    ERROR("Only register A or M can be added to D in c-instruction");
                }
            } break;
            
            case TT_EOF: {
                ERROR("Unexpected end of file in binary addition c-instruction");
            } break;
            
            default: {
                ERROR("Unexpected right hand operand of addition c-instruction");
            }
        }
    } else if (STRING_EQUALS_C_STRING(lhs->text, "M")) {
        if (rhs->type == TT_INT && STRING_EQUALS_C_STRING(rhs->text, "1")) {
            return 0b1110111;
        } else {
            ERROR("Only 1 can be added to M in c-instruction");
        }
    } else {
        ERROR("Unexpected left hand side of addition c-instruction");
    }
}

static uint16_t parse_subtraction_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_IDENTIFIER);
    assert(peek_next_type(token_stream) == TT_MINUS);
    
    const Token* const lhs = consume_token(token_stream);
    skip_token(token_stream);   // skip '-' token
    const Token* const rhs = consume_token(token_stream);
    if (STRING_EQUALS_C_STRING(lhs->text, "A")) {
        switch (rhs->type) {
            case TT_INT: {
                if (STRING_EQUALS_C_STRING(rhs->text, "1")) {
                    return 0b0110010;
                } else {
                    ERROR("Only integer literal 1 can be subtracted from A in c-instruction");
                }
            } break;
            
            case TT_IDENTIFIER: {
                if (STRING_EQUALS_C_STRING(rhs->text, "D")) {
                    return 0b0000111;
                } else {
                    ERROR("Only register D can be subtracted from A in c-instruction");
                }
            } break;
            
            case TT_EOF: {
                ERROR("Unexpected end of file in binary subtraction c-instruction");
            } break;
            
            default: {
                ERROR("Unexpected right hand operand of subtraction c-instruction");
            }
        }
    } else if (STRING_EQUALS_C_STRING(lhs->text, "D")) {
        switch (rhs->type) {
            case TT_INT: {
                if (STRING_EQUALS_C_STRING(rhs->text, "1")) {
                    return 0b0001110;
                } else {
                    ERROR("Only integer literal 1 can be subtracted from D in c-instruction");
                }
            } break;
            
            case TT_IDENTIFIER: {
                if (STRING_EQUALS_C_STRING(rhs->text, "A")) {
                    return 0b0010011;
                } else if (STRING_EQUALS_C_STRING(rhs->text, "M")) {
                    return 0b1010011;
                } else {
                    ERROR("Only register A or M can be subtracted from D in c-instruction");
                }
            } break;
            
            case TT_EOF: {
                ERROR("Unexpected end of file in binary subtraction c-instruction");
            } break;
            
            default: {
                ERROR("Unexpected right hand operand of subtraction c-instruction");
            }
        }
    } else if (STRING_EQUALS_C_STRING(lhs->text, "M")) {
        switch (rhs->type) {
            case TT_INT: {
                if (STRING_EQUALS_C_STRING(rhs->text, "1")) {
                    return 0b1110010;
                } else {
                    ERROR("Only integer literal 1 can be subtracted from M in c-instruction");
                }
            } break;
            
            case TT_IDENTIFIER: {
                if (STRING_EQUALS_C_STRING(rhs->text, "D")) {
                    return 0b1000111;
                } else {
                    ERROR("Only register D can be subtracted from M in c-instruction");
                }
            } break;
            
            case TT_EOF: {
                ERROR("Unexpected end of file in binary subtraction c-instruction");
            } break;
            
            default: {
                ERROR("Unexpected right hand operand of subtraction c-instruction");
            }
        }
    } else {
        ERROR("Unexpected left hand side of subtraction c-instruction");
    }
}

static uint16_t parse_and_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_IDENTIFIER);
    assert(peek_next_type(token_stream) == TT_AND);
    
    const Token* const lhs = consume_token(token_stream);
    skip_token(token_stream);   // skip '&' token
    const Token* const rhs = consume_token(token_stream);
    if (STRING_EQUALS_C_STRING(lhs->text, "D")) {
        if (STRING_EQUALS_C_STRING(rhs->text, "A")) {
            return 0b0000000;
        } else if (STRING_EQUALS_C_STRING(rhs->text, "M")) {
            return 0b1000000;
        } else {
            ERROR("Only register A or M can be right hand operand in & c-instruction");
        }
    } else {
        ERROR("Only register D can be left hand operand to & c-instruction");
    }
}

static uint16_t parse_or_comp(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_IDENTIFIER);
    assert(peek_next_type(token_stream) == TT_OR);
    
    const Token* const lhs = consume_token(token_stream);
    skip_token(token_stream);   // skip '|' token
    const Token* const rhs = consume_token(token_stream);
    if (STRING_EQUALS_C_STRING(lhs->text, "D")) {
        if (STRING_EQUALS_C_STRING(rhs->text, "A")) {
            return 0b0010101;
        } else if (STRING_EQUALS_C_STRING(rhs->text, "M")) {
            return 0b1010101;
        } else {
            ERROR("Only register A or M can be right hand operand in | c-instruction");
        }
    } else {
        ERROR("Only register D can be left hand operand to | c-instruction");
    }
}

static uint16_t parse_destination(TokenStream* const token_stream) {
    assert(peek_next_type(token_stream) == TT_EQUALS);
    
    const Token* const token = consume_token(token_stream);
    skip_token(token_stream);   // skip '=' token
    if (STRING_EQUALS_C_STRING(token->text, "M")) {
        return 0b001;
    } else if (STRING_EQUALS_C_STRING(token->text, "D")) {
        return 0b010;
    } else if (STRING_EQUALS_C_STRING(token->text, "MD")) {
        return 0b011;
    } else if (STRING_EQUALS_C_STRING(token->text, "A")) {
        return 0b100;
    } else if (STRING_EQUALS_C_STRING(token->text, "AM")) {
        return 0b101;
    } else if (STRING_EQUALS_C_STRING(token->text, "AD")) {
        return 0b110;
    } else if (STRING_EQUALS_C_STRING(token->text, "AMD")) {
        return 0b111;
    } else {
        ERROR("Destination of c-instruction can only be M, D, MD, A, AM, AD or AMD");
    }
}

static uint16_t parse_jump(TokenStream* const token_stream) {
    assert(token_stream->current->type == TT_SEMI_COLON);
    
    skip_token(token_stream);
    const Token* const token = consume_token(token_stream);
    if (token->type == TT_IDENTIFIER) {
        if (STRING_EQUALS_C_STRING(token->text, "JGT")) {
            return 0b001;
        } else if (STRING_EQUALS_C_STRING(token->text, "JEQ")) {
            return 0b010;
        } else if (STRING_EQUALS_C_STRING(token->text, "JGE")) {
            return 0b011;
        } else if (STRING_EQUALS_C_STRING(token->text, "JLT")) {
            return 0b100;
        } else if (STRING_EQUALS_C_STRING(token->text, "JNE")) {
            return 0b101;
        } else if (STRING_EQUALS_C_STRING(token->text, "JLE")) {
            return 0b110;
        } else if (STRING_EQUALS_C_STRING(token->text, "JMP")) {
            return 0b111;
        } else {
            ERROR("Only JGT, JEQ, JGE, JLT, JNE, JLE and JMP are the allowed jump types");
        }
    } else {
        ERROR("Expected identifier for jump specification");
    }
}

static ParseResult parse(const Tokens tokens) {
    Instructions instructions = {};
    instructions.data = malloc(sizeof(Instruction) * tokens.count); // can't have more instructions than tokens
    instructions.count = 0;
    
    JumpSymbols jump_symbols = {};
    jump_symbols.data = malloc(sizeof(JumpSymbol) * tokens.count);  // can't have more jumps than tokens
    jump_symbols.count = 0;
    
    TokenStream token_stream = {};
    token_stream.start = tokens.data;
    token_stream.end = tokens.data + tokens.count;
    token_stream.current = tokens.data;
    
    bool parsing = true;
    while (parsing) {
        switch (token_stream.current->type) {
            // a-instruction
            case TT_AT: {
                Instruction instruction = {};
                instruction.type = IT_A;
                instruction.a = parse_a_instruction(&token_stream);
                instructions.data[instructions.count++] = instruction;
            } break;
            
            // c-instruction
            case TT_NOT: {
                Instruction instruction = {};
                instruction.type = IT_C;
                instruction.c.comp = parse_not_comp(&token_stream);
                if (token_stream.current->type == TT_SEMI_COLON) {
                    instruction.c.jump = parse_jump(&token_stream);
                }
                
                instructions.data[instructions.count++] = instruction;
            } break;
            
            case TT_INT: {
                Instruction instruction = {};
                instruction.type = IT_C;
                instruction.c.comp = parse_integer_literal_comp(&token_stream);
                if (token_stream.current->type == TT_SEMI_COLON) {
                    instruction.c.jump = parse_jump(&token_stream);
                }
                
                instructions.data[instructions.count++] = instruction;
            } break;
            
            case TT_MINUS: {
                Instruction instruction = {};
                instruction.type = IT_C;
                instruction.c.comp = parse_negation_comp(&token_stream);
                if (token_stream.current->type == TT_SEMI_COLON) {
                    instruction.c.jump = parse_jump(&token_stream);
                }
                
                instructions.data[instructions.count++] = instruction;
            } break;
            
            case TT_IDENTIFIER: {
                switch (peek_next_type(&token_stream)) {
                    case TT_EQUALS: {
                        Instruction instruction = {};
                        instruction.type = IT_C;
                        instruction.c.dest = parse_destination(&token_stream);
                        switch (token_stream.current->type) {
                            case TT_NOT: {
                                instruction.c.comp = parse_not_comp(&token_stream);
                                if (token_stream.current->type == TT_SEMI_COLON) {
                                    instruction.c.jump = parse_jump(&token_stream);
                                }
                            } break;
                            
                            case TT_IDENTIFIER: {
                                // could be a register or start of binary expression
                                switch (peek_next_type(&token_stream)) {
                                    case TT_PLUS: {
                                        instruction.c.comp = parse_addition_comp(&token_stream);
                                        if (token_stream.current->type == TT_SEMI_COLON) {
                                            instruction.c.jump = parse_jump(&token_stream);
                                        }
                                    } break;
                                    
                                    case TT_MINUS: {
                                        instruction.c.comp = parse_subtraction_comp(&token_stream);
                                        if (token_stream.current->type == TT_SEMI_COLON) {
                                            instruction.c.jump = parse_jump(&token_stream);
                                        }
                                    } break;
                                    
                                    case TT_AND: {
                                        instruction.c.comp = parse_and_comp(&token_stream);
                                        if (token_stream.current->type == TT_SEMI_COLON) {
                                            instruction.c.jump = parse_jump(&token_stream);
                                        }
                                    } break;
                                    
                                    case TT_OR: {
                                        instruction.c.comp = parse_or_comp(&token_stream);
                                        if (token_stream.current->type == TT_SEMI_COLON) {
                                            instruction.c.jump = parse_jump(&token_stream);
                                        }
                                    } break;
                                    
                                    case TT_EOF: {
                                        parsing = false;
                                    } // fallthrough
                                    
                                    default: {
                                        // assume just the identifier on its own
                                        instruction.c.comp = parse_identity_comp(&token_stream);
                                    } break;
                                }
                            } break;
                            
                            case TT_INT: {
                                instruction.c.comp = parse_integer_literal_comp(&token_stream);
                                if (token_stream.current->type == TT_SEMI_COLON) {
                                    instruction.c.jump = parse_jump(&token_stream);
                                }
                            } break;
                            
                            case TT_MINUS: {
                                instruction.c.comp = parse_negation_comp(&token_stream);
                                if (token_stream.current->type == TT_SEMI_COLON) {
                                    instruction.c.jump = parse_jump(&token_stream);
                                }
                            } break;
                            
                            default: {
                            } break;
                        }
                        
                        instructions.data[instructions.count++] = instruction;
                    } break;
                    
                    case TT_SEMI_COLON: {
                        Instruction instruction = {};
                        instruction.type = IT_C;
                        instruction.c.comp = parse_identity_comp(&token_stream);
                        instruction.c.jump = parse_jump(&token_stream);
                        
                        instructions.data[instructions.count++] = instruction;
                    } break;
                    
                    case TT_EOF: {
                        parsing = false;
                    } // fallthrough
                    
                    default: {
                        // assume just the identifier on its own
                        Instruction instruction = {};
                        instruction.type = IT_C;
                        instruction.c.comp = parse_identity_comp(&token_stream);
                        
                        instructions.data[instructions.count++] = instruction;
                    } break;
                }
            } break;
            
            // label symbol
            case TT_LPAREN: {
                skip_token(&token_stream);
                const Token* const jump_symbol_token = consume_token(&token_stream);
                if (jump_symbol_token->type != TT_IDENTIFIER) {
                    ERROR("Expected identifier for jump symbol declaration");
                }
                
                if (token_stream.current->type != TT_RPAREN) {
                    ERROR("Jump symbol declarations must be closed with )");
                }
                
                skip_token(&token_stream);
                JumpSymbol jump_symbol = {};
                jump_symbol.name = jump_symbol_token->text;
                jump_symbol.address = instructions.count;
                jump_symbols.data[jump_symbols.count++] = jump_symbol;
            } break;
            
            case TT_EOF: {
                parsing = false;
            } break;
            
            default: {
            } break;
        }
    }
    
    ParseResult result = {};
    result.instructions = instructions;
    result.jump_symbols = jump_symbols;
    
    return result;
}

static int get_size(const Buffer* const buffer) {
    return buffer->current - buffer->begin;
}

static void write_byte(Buffer* buffer, const char c) {
    assert(buffer->current != buffer->end);
    *buffer->current++ = c;
}

static uint16_t get_word(const CInstruction c_instruction) {
    const uint16_t comp = c_instruction.comp;
    const uint16_t dest = c_instruction.dest;
    const uint16_t jump = c_instruction.jump;
    return (0b111 << 13) | (comp << 6) | (dest << 3) | jump;
}

static void write_instruction_word(Buffer* buffer, const uint16_t instruction) {
    for (int i = 15; i >= 0; --i) {
        const char c = ((1 << i) & instruction) ? '1' : '0';
        write_byte(buffer, c);
    }
    
    write_byte(buffer, '\n');
}

static void write_c_instruction(Buffer* buffer, const CInstruction c_instruction) {
    const uint16_t word = get_word(c_instruction);
    write_instruction_word(buffer, word);
}

static Buffer output_instructions_to_buffer(const Instructions instructions, const JumpSymbols jump_symbols) {
    const int buffer_size = 17 * instructions.count;    // each instruction is 16 0/1s and a new-line
    Buffer buffer = {};
    buffer.begin = malloc(buffer_size);
    buffer.current = buffer.begin;
    buffer.end = buffer.begin + buffer_size;
    
    static String variable_symbols[0x6000] = {};    // can't have more variables than address space
    static int variable_symbol_count = 0;
    
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R0");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R1");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R2");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R3");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R4");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R5");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R6");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R7");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R8");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R9");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R10");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R11");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R12");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R13");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R14");
    variable_symbols[variable_symbol_count++] = construct_string_from_c_string("R15");
    
    for (int instruction_index = 0; instruction_index < instructions.count; ++instruction_index) {
        const Instruction* const instruction = instructions.data + instruction_index;
        switch (instruction->type) {
            case IT_A: {
                int address = 0x0000;
                if (instruction->a.symbol.count > 0) {
                    // need to resolve symbol
                    const String symbol_name = instruction->a.symbol;
                    int jump_symbol_index = -1;
                    for (int j = 0; j < jump_symbols.count; ++j) {
                        const JumpSymbol* const jump_symbol = jump_symbols.data + j;
                        if (string_equals(symbol_name, jump_symbol->name)) {
                            jump_symbol_index = j;
                            break;
                        }
                    }
                    
                    if (jump_symbol_index != -1) {
                        address = jump_symbols.data[jump_symbol_index].address;
                        
                        if (address >= 0x8000) {
                            ERROR("Encountered jump symbol address %d which is outside of memory address range", address);
                        }
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "SP")) {
                        address = 0x0000;
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "LCL")) {
                        address = 0x0001;
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "ARG")) {
                        address = 0x0002;
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "THIS")) {
                        address = 0x0003;
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "THAT")) {
                        address = 0x0004;
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "SCREEN")) {
                        address = 0x4000;
                    } else if (STRING_EQUALS_C_STRING(symbol_name, "KBD")) {
                        address = 0x6000;
                    } else {
                        bool found = false;
                        for (int i = 0; i < variable_symbol_count; ++i) {
                            if (string_equals(symbol_name, variable_symbols[i])) {
                                found = true;
                                address = i;
                                break;
                            }
                        }
                        
                        if (!found) {
                            address = variable_symbol_count;
                            variable_symbols[variable_symbol_count++] = symbol_name;
                        }
                        
                        if (address > 0x6000) {
                            ERROR("Encountered variable address %d which is outside of memory address range", address);
                        }
                    }
                } else {
                    address = instruction->a.address;
                }
                                
                // // we know high bit is 0 from above check so writing address as instruction works here
                write_instruction_word(&buffer, address);
            } break;
            
            case IT_C: {
                const uint16_t word = get_word(instruction->c);
                write_instruction_word(&buffer, word);
            } break;
            
            default: {
                ERROR("Unexpected instruction type encountered");
            } break;
        }
    }
    
    if (instructions.count > 0) {
        assert(buffer.current != buffer.begin);
        --buffer.current;   // trim extra new-line at the end
    }
    
    return buffer;
}
