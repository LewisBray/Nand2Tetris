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
    CMD_NOT,
    CMD_LABEL,
    CMD_GOTO,
    CMD_IF_GOTO,
    CMD_FUNCTION,
    CMD_CALL,
    CMD_RETURN
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

typedef struct CommandLabel {
    String name;
} CommandLabel;

typedef struct CommandJump {
    String label;
} CommandJump;

typedef struct CommandFunction {
    String name;
    int local_var_count;
} CommandFunction;

typedef struct CommandCall {
    String function;
    int arg_count;
} CommandCall;

typedef struct Command {
    CommandType type;
    union {
        CommandMemoryAccess push;
        CommandMemoryAccess pop;
        CommandLabel label;
        CommandJump go_to;
        CommandJump if_go_to;
        CommandFunction function;
        CommandCall call;
    };
} Command;

#endif
