#include "asm_file.h"

#include <stdio.h>

static bool buffer_has_content(const ASMFile* const file) {
    return file->buffer_index > 0;
}

static void flush_buffer(ASMFile* const file) {
    fwrite(file->buffer, 1, file->buffer_index, file->handle);
    file->buffer_index = 0;
}

// i've basically made my own (possibly worse) fprintf here, oh well, i
// wrote a load of code with this paradigm and it works so keeping it,
// but i suspect C can be used more elegantly here
#define TO_STRING(c_str) construct_string(c_str, sizeof(c_str) - 1)

static void write_char(ASMFile* const file, const char c) {
    file->buffer[file->buffer_index++] = c;
    if (file->buffer_index == file->buffer_capacity) {
        flush_buffer(file);
    }
}

static void write_string(ASMFile* const file, const String string) {
    for (int i = 0; i < string.count; ++i) {
        write_char(file, string.data[i]);
    }
}

static void write_c_string(ASMFile* const file, const char* string) {
    while (*string != '\0') {
        write_char(file, *string++);
    }
}

static void write_int(ASMFile* const file, const int i) {
    char buffer[16] = {};
    snprintf(buffer, sizeof(buffer), "%d", i);
    write_c_string(file, buffer);
}

static void write_a_instruction(ASMFile* const file, const String at) {
    write_char(file, '@');
    write_string(file, at);
    write_char(file, '\n');
}

static void write_static_var_a_instruction(ASMFile* const file, const String index, const String vm_file_name) {
    write_char(file, '@');
    write_string(file, file->name);
    write_char(file, '.');
    write_string(file, vm_file_name);
    write_char(file, '.');
    write_string(file, index);
    write_char(file, '\n');
}

static void write_store_value_in_d(ASMFile* const file, const String at) {
    write_a_instruction(file, at);
    write_c_string(file, "D=M\n");
}

static void write_store_constant_in_d(ASMFile* const file, const String value) {
    write_a_instruction(file, value);
    write_c_string(file, "D=A\n");
}

static void write_push_d_to_stack(ASMFile* const file)
{
    write_c_string(file, "@SP\n");
    write_c_string(file, "M=M+1\n");    // inc SP now so we don't have to @SP again...
    write_c_string(file, "A=M-1\n");    // ...later, so make sure we -1 here to compensate
    write_c_string(file, "M=D\n");
}

static void write_pop_stack_to_d(ASMFile* const file)
{
    write_c_string(file, "@SP\n");
    write_c_string(file, "AM=M-1\n");   // dec SP and set A reg up at same time
    write_c_string(file, "D=M\n");
}

static void write_push_from_segment(ASMFile* const file, const String segment_name, const String index) {
    // get address of segment and put in D
    write_store_value_in_d(file, segment_name);
    
    // increment pointer in D by index
    write_a_instruction(file, index);
    write_c_string(file, "A=D+A\n");
    write_c_string(file, "D=M\n");  // D holds what is at segment + index
    
    write_push_d_to_stack(file);
}

#define WRITE_PUSH_FROM_SEGMENT(file, segment_name, index) write_push_from_segment(file, TO_STRING(segment_name), index)

static void write_push_from_address(ASMFile* const file, const String address, const String index) {
    // get address put in D
    write_store_constant_in_d(file, address);
    
    // increment pointer in D by index
    write_a_instruction(file, index);
    write_c_string(file, "A=D+A\n");
    write_c_string(file, "D=M\n");  // D holds what is at temp + index
    
    write_push_d_to_stack(file);
}

#define WRITE_PUSH_FROM_ADDRESS(file, address, index) write_push_from_address(file, TO_STRING(address), index)

static void write_pop_to_segment(ASMFile* const file, const String segment_name, const String index) {
    // get address of segment and put in D
    write_store_value_in_d(file, segment_name);
    
    // increment pointer in D by index
    write_a_instruction(file, index);
    write_c_string(file, "D=D+A\n");
    
    // store value in R13
    write_c_string(file, "@R13\n");
    write_c_string(file, "M=D\n");
    
    // pop stack and store in D
    write_pop_stack_to_d(file);
    
    // load address to write to from R13
    write_c_string(file, "@R13\n");
    write_c_string(file, "A=M\n");
    
    // write popped value
    write_c_string(file, "M=D\n");
}

#define WRITE_POP_TO_SEGMENT(file, segment_name, index) write_pop_to_segment(file, TO_STRING(segment_name), index)

static void write_pop_to_address(ASMFile* const file, const String address, const String index) {
    // get address of segment and put in D
    write_store_constant_in_d(file, address);
    
    // increment pointer in D by index
    write_a_instruction(file, index);
    write_c_string(file, "D=D+A\n");
    
    // store value in R13
    write_c_string(file, "@R13\n");
    write_c_string(file, "M=D\n");
    
    // pop stack and store in D
    write_pop_stack_to_d(file);
    
    // load address to write to from R13
    write_c_string(file, "@R13\n");
    write_c_string(file, "A=M\n");
    
    // write popped value
    write_c_string(file, "M=D\n");
}

#define WRITE_POP_TO_ADDRESS(file, address, index) write_pop_to_address(file, TO_STRING(address), index)

static void write_label(ASMFile* const file, const String label) {
    write_string(file, file->name);
    write_char(file, '.');
    write_string(file, label);
}

static void write_bootstrap_asm(ASMFile* const file) {
    // set SP to 256
    write_c_string(file, "@256\n");
    write_c_string(file, "D=A\n");
    write_c_string(file, "@SP\n");
    write_c_string(file, "M=D\n");
    
    // write asm to call Sys.init
    static const char SYS_INIT_FN_NAME[] = "Sys.init";
    
    Command call_sys_init_cmd = {};
    call_sys_init_cmd.type = CMD_CALL;
    call_sys_init_cmd.call.function.data = SYS_INIT_FN_NAME;
    call_sys_init_cmd.call.function.count = sizeof(SYS_INIT_FN_NAME) - 1;
    call_sys_init_cmd.call.arg_count = 0;
    
    String empty = {};
    empty.data = "";
    empty.count = 0;
    
    write_asm(file, &call_sys_init_cmd, empty);
}

static void write_asm(ASMFile* const file, const Command* const command, const String vm_file_name) {
    switch (command->type) {
        case CMD_PUSH: {
            switch (command->push.segment) {
                case SEG_ARGUMENT: {
                    WRITE_PUSH_FROM_SEGMENT(file, "ARG", command->push.index);
                } break;
                
                case SEG_LOCAL: {
                    WRITE_PUSH_FROM_SEGMENT(file, "LCL", command->push.index);
                } break;
                
                case SEG_STATIC: {
                    write_static_var_a_instruction(file, command->push.index, vm_file_name);
                    write_c_string(file, "D=M\n");
                    write_push_d_to_stack(file);
                } break;
                
                case SEG_CONSTANT: {
                    write_store_constant_in_d(file, command->push.index);
                    write_push_d_to_stack(file);
                } break;
                
                case SEG_THIS: {
                    WRITE_PUSH_FROM_SEGMENT(file, "THIS", command->push.index);
                } break;
                
                case SEG_THAT: {
                    WRITE_PUSH_FROM_SEGMENT(file, "THAT", command->push.index);
                } break;
                
                case SEG_POINTER: {
                    WRITE_PUSH_FROM_ADDRESS(file, "3", command->push.index); // pointer starts from RAM[3]
                } break;
                
                case SEG_TEMP: {
                    WRITE_PUSH_FROM_ADDRESS(file, "5", command->push.index); // temp starts from RAM[5]
                } break;
                
                default: {
                    ERROR("Unexpected segment specifier in push command");
                } break;
            }
        } break;
        
        case CMD_POP: {
            switch (command->pop.segment) {
                case SEG_ARGUMENT: {
                    WRITE_POP_TO_SEGMENT(file, "ARG", command->pop.index);
                } break;
                
                case SEG_LOCAL: {
                    WRITE_POP_TO_SEGMENT(file, "LCL", command->pop.index);
                } break;
                
                case SEG_STATIC: {
                    write_pop_stack_to_d(file);
                    write_static_var_a_instruction(file, command->pop.index, vm_file_name);
                    write_c_string(file, "M=D\n");
                } break;
                
                case SEG_CONSTANT: {
                    ERROR("Cannot pop to constant segment");
                } break;
                
                case SEG_THIS: {
                    WRITE_POP_TO_SEGMENT(file, "THIS", command->pop.index);
                } break;
                
                case SEG_THAT: {
                    WRITE_POP_TO_SEGMENT(file, "THAT", command->pop.index);
                } break;
                
                case SEG_POINTER: {
                    WRITE_POP_TO_ADDRESS(file, "3", command->pop.index); // pointer starts from RAM[3]
                } break;
                
                case SEG_TEMP: {
                    WRITE_POP_TO_ADDRESS(file, "5", command->pop.index); // temp starts from RAM[5]
                } break;
                
                default: {
                    ERROR("Unexpected segment specifier in pop command");
                } break;
            }
        } break;
        
        case CMD_ADD: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "M=D+M\n");
        } break;
        
        case CMD_SUB: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "M=M-D\n");
        } break;
        
        case CMD_NEG: {
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");
            write_c_string(file, "M=-M\n");
        } break;
        
        case CMD_EQ: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "D=M-D\n");
            
            write_c_string(file, "M=0\n");       // if they are not equal, we are done (probably common case)
            write_c_string(file, "@L.");
            write_int(file, file->jump_label_counter);
            write_char(file, '\n');
            write_c_string(file, "D;JNE\n");
            
            write_c_string(file, "@SP\n");       // otherwise write -1 to stack
            write_c_string(file, "A=M-1\n");
            write_c_string(file, "M=-1\n");
            
            write_c_string(file, "(L.");
            write_int(file, file->jump_label_counter);
            write_c_string(file, ")\n");
            
            ++file->jump_label_counter;
        } break;
        
        case CMD_GT: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "D=M-D\n");
            
            write_c_string(file, "M=0\n");       // if not greater-than, we are done
            write_c_string(file, "@L.");
            write_int(file, file->jump_label_counter);
            write_char(file, '\n');
            write_c_string(file, "D;JLE\n");
            
            write_c_string(file, "@SP\n");       // otherwise write -1 to stack
            write_c_string(file, "A=M-1\n");
            write_c_string(file, "M=-1\n");
            
            write_c_string(file, "(L.");
            write_int(file, file->jump_label_counter);
            write_c_string(file, ")\n");
            
            ++file->jump_label_counter;
        } break;
        
        case CMD_LT: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "D=M-D\n");
            
            write_c_string(file, "M=0\n");       // if not less-than, we are done
            write_c_string(file, "@L.");
            write_int(file, file->jump_label_counter);
            write_char(file, '\n');
            write_c_string(file, "D;JGE\n");
            
            write_c_string(file, "@SP\n");       // otherwise write -1 to stack
            write_c_string(file, "A=M-1\n");
            write_c_string(file, "M=-1\n");
            
            write_c_string(file, "(L.");
            write_int(file, file->jump_label_counter);
            write_c_string(file, ")\n");
            
            ++file->jump_label_counter;
        } break;
        
        case CMD_AND: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "M=D&M\n");
        } break;
        
        case CMD_OR: {
            write_pop_stack_to_d(file);
            
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");     // leave SP alone so it's ready for result to be pushed for free
            write_c_string(file, "M=D|M\n");
        } break;
        
        case CMD_NOT: {
            write_c_string(file, "@SP\n");
            write_c_string(file, "A=M-1\n");
            write_c_string(file, "M=!M\n");
        } break;
        
        case CMD_LABEL: {
            write_char(file, '(');
            write_label(file, command->label.name);
            write_c_string(file, ")\n");
        } break;
        
        case CMD_GOTO: {
            write_char(file, '@');
            write_label(file, command->go_to.label);
            write_char(file, '\n');
            write_c_string(file, "0;JMP\n");
        } break;
        
        case CMD_IF_GOTO: {
            write_pop_stack_to_d(file);
            write_char(file, '@');
            write_label(file, command->if_go_to.label);
            write_char(file, '\n');
            write_c_string(file, "D;JNE\n");
        } break;
        
        case CMD_FUNCTION: {
            write_char(file, '(');
            write_label(file, command->function.name);
            write_c_string(file, ")\n");
            
            if (command->function.local_var_count > 0) {
                write_c_string(file, "@LCL\n");
                write_c_string(file, "D=M\n");
                for (int i = 0; i < command->function.local_var_count; ++i) {
                    write_char(file, '@');
                    write_int(file, i);
                    write_char(file, '\n');
                    
                    write_c_string(file, "A=D+A\n");
                    write_c_string(file, "M=0\n");
                }
            }
        } break;
        
        case CMD_CALL: {
            // use (later-declared) return label to push return address to SP
            write_char(file, '@');
            write_c_string(file, "return_");
            write_label(file, command->call.function);
            write_char(file, '.');
            write_int(file, file->jump_label_counter);
            write_char(file, '\n');
            
            write_c_string(file, "D=A\n");
            write_push_d_to_stack(file);
            
            // push LCL
            write_c_string(file, "@LCL\n");
            write_c_string(file, "D=M\n");
            write_push_d_to_stack(file);
            
            // push ARG
            write_c_string(file, "@ARG\n");
            write_c_string(file, "D=M\n");
            write_push_d_to_stack(file);
            
            // push THIS
            write_c_string(file, "@THIS\n");
            write_c_string(file, "D=M\n");
            write_push_d_to_stack(file);
            
            // push THAT
            write_c_string(file, "@THAT\n");
            write_c_string(file, "D=M\n");
            write_push_d_to_stack(file);
            
            // LCL = SP
            write_c_string(file, "@SP\n");
            write_c_string(file, "D=M\n");
            
            write_c_string(file, "@LCL\n");
            write_c_string(file, "M=D\n");
            
            // ARG = SP - n - 5, D still holds SP
            write_c_string(file, "@5\n");
            write_c_string(file, "D=D-A\n");
            
            write_char(file, '@');
            write_int(file, command->call.arg_count);
            write_char(file, '\n');
            write_c_string(file, "D=D-A\n");
            
            write_c_string(file, "@ARG\n");
            write_c_string(file, "M=D\n");
            
            // goto function
            write_char(file, '@');
            write_label(file, command->function.name);
            write_char(file, '\n');
            
            write_c_string(file, "0;JMP\n");
            
            // write return label
            write_char(file, '(');
            write_c_string(file, "return_");
            write_label(file, command->call.function);
            write_char(file, '.');
            write_int(file, file->jump_label_counter);
            write_c_string(file, ")\n");
            
            ++file->jump_label_counter;
        } break;
        
        case CMD_RETURN: {
            // store LCL in R13
            write_c_string(file, "@LCL\n");
            write_c_string(file, "D=M\n");
            write_c_string(file, "@R13\n");
            write_c_string(file, "M=D\n");
            
            // store return address in R14
            write_c_string(file, "@5\n");
            write_c_string(file, "A=D-A\n");
            write_c_string(file, "D=M\n");
            write_c_string(file, "@R14\n");
            write_c_string(file, "M=D\n");
            
            // pop stack to *ARG, this is the return value of the function
            write_pop_stack_to_d(file);
            write_c_string(file, "@ARG\n");
            write_c_string(file, "A=M\n");
            write_c_string(file, "M=D\n");
            
            // restore SP
            write_c_string(file, "@ARG\n");
            write_c_string(file, "D=M+1\n");
            write_c_string(file, "@SP\n");
            write_c_string(file, "M=D\n");
                        
            // restore THAT
            write_c_string(file, "@R13\n");
            write_c_string(file, "AM=M-1\n");
            
            write_c_string(file, "D=M\n");
            write_c_string(file, "@THAT\n");
            write_c_string(file, "M=D\n");
            
            // restore THIS
            write_c_string(file, "@R13\n");
            write_c_string(file, "AM=M-1\n");
            
            write_c_string(file, "D=M\n");
            write_c_string(file, "@THIS\n");
            write_c_string(file, "M=D\n");
            
            // restore ARG
            write_c_string(file, "@R13\n");
            write_c_string(file, "AM=M-1\n");
            
            write_c_string(file, "D=M\n");
            write_c_string(file, "@ARG\n");
            write_c_string(file, "M=D\n");
            
            // restore LCL
            write_c_string(file, "@R13\n");
            write_c_string(file, "AM=M-1\n");
            
            write_c_string(file, "D=M\n");
            write_c_string(file, "@LCL\n");
            write_c_string(file, "M=D\n");
            
            // goto return address
            write_c_string(file, "@R14\n");
            write_c_string(file, "A=M\n");
            write_c_string(file, "0;JMP\n");
        } break;
        
        default: {
            ERROR("Encountered unexpected command type");
        } break;
    }
}
