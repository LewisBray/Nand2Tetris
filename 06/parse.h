#ifndef PARSE_H
#define PARSE_H

#include "string_view.h"
#include "tokenise.h"

#include <stdint.h>

typedef struct AInstruction {
    uint16_t address;
    String symbol;
} AInstruction;

typedef struct CInstruction {
    uint16_t comp;
    uint16_t dest;
    uint16_t jump;
} CInstruction;

typedef enum InstructionType {
    IT_A,
    IT_C
} InstructionType;

typedef struct Instruction {
    InstructionType type;
    union {
        AInstruction a;
        CInstruction c;
    };
} Instruction;

typedef struct Instructions {
    Instruction* data;
    int count;
} Instructions;

typedef struct JumpSymbol {
    String name;
    uint16_t address;
} JumpSymbol;

typedef struct JumpSymbols {
    JumpSymbol* data;
    int count;
} JumpSymbols;

typedef struct ParseResult {
    Instructions instructions;
    JumpSymbols jump_symbols;
} ParseResult;

static ParseResult parse(Tokens tokens);

typedef struct Buffer {
    char* begin;
    char* end;
    char* current;
} Buffer;

static int get_size(const Buffer* buffer);

static Buffer output_instructions_to_buffer(Instructions instructions, JumpSymbols jump_symbols);

#endif
