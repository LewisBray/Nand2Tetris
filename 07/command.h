#ifndef COMMAND_H
#define COMMAND_H

#include "string_view.h"

typedef enum CommandType {
    CMD_PUSH,
    CMD_POP,
    CMD_ADD,
    CMD_SUB,
    CMD_NEG,
    CMD_EQ,
    CMD_GT,
    CMD_LT,
    CMD_AND,
    CMD_OR,
    CMD_NOT
} CommandType;

typedef enum Segment {
    SEG_ARGUMENT,
    SEG_LOCAL,
    SEG_STATIC,
    SEG_CONSTANT,
    SEG_THIS,
    SEG_THAT,
    SEG_POINTER,
    SEG_TEMP
} Segment;

typedef struct CommandMemoryAccess {
    Segment segment;
    String index;
} CommandMemoryAccess;

typedef struct Command {
    CommandType type;
    union {
        CommandMemoryAccess push;
        CommandMemoryAccess pop;
    };
} Command;

#endif
