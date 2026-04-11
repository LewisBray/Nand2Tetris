#define _CRT_SECURE_NO_WARNINGS

#include "string_view.h"
#include "file_names.h"
#include "asm_file.h"
#include "vm_file.h"
#include "error.h"

#include "string_view.c"
#include "asm_file.c"
#include "vm_file.c"

#include <stdlib.h>
#include <stdio.h>

static const char VM_FILE_EXTENSION[] = "vm";
static const char ASM_FILE_EXTENSION[] = "asm";

int main(const int argc, const char* const argv[]) {
    if (argc != 2) {
        ERROR("Supply the .%s file or a directory as the only argument", VM_FILE_EXTENSION);
    }
    
    const String arg1 = construct_string_from_c_string(argv[1]);
    String dir = {};
    String name = {};
    const int dot_index = reverse_find_index_of(arg1, '.');
    int slash_index = reverse_find_index_of(arg1, '/');
    if (slash_index == -1) {
        slash_index = reverse_find_index_of(arg1, '\\');
    }
    
    if (dot_index != -1 && slash_index != -1) {
        if (dot_index <= slash_index) {
            ERROR("Invalid input");
        }
        
        dir.data = arg1.data;
        dir.count = slash_index;
        
        name.data = arg1.data + slash_index + 1;
        name.count = dot_index - slash_index - 1;
    } else if (dot_index != -1 && slash_index == -1) {
        dir.data = "";
        dir.count = 0;
        
        name.data = arg1.data;
        name.count = dot_index;
    } else if (dot_index == -1 && slash_index != -1) {
        dir.data = arg1.data;
        dir.count = slash_index;
        
        name.data = arg1.data + slash_index + 1;
        name.count = arg1.count - slash_index - 1;
    } else {
        dir = arg1;
        name = arg1;
    }
    
    char asm_file_name[4096] = {};  // probably large enough
    FileNames vm_file_names = {};
    if (dot_index == -1) {
        snprintf(asm_file_name, sizeof(asm_file_name), "%.*s/%.*s.%s", dir.count, dir.data, name.count, name.data, ASM_FILE_EXTENSION);
        
        char dir_buffer[4096] = {};
        snprintf(dir_buffer, sizeof(dir_buffer), "%.*s", dir.count, dir.data);
        vm_file_names = get_file_names_with_extension(dir_buffer, VM_FILE_EXTENSION);
    } else if (dot_index == arg1.count - 3
        && arg1.data[arg1.count - 2] == VM_FILE_EXTENSION[0]
        && arg1.data[arg1.count - 1] == VM_FILE_EXTENSION[1]) {
        snprintf(asm_file_name, sizeof(asm_file_name), "%.*s.%s", dot_index, arg1.data, ASM_FILE_EXTENSION);
        
        vm_file_names.data = malloc(FILE_NAME_BUFFER_SIZE);
        vm_file_names.count = 1;
        snprintf(vm_file_names.data[0], FILE_NAME_BUFFER_SIZE, "%.*s.%s", name.count, name.data, VM_FILE_EXTENSION);
    }
    
    if (vm_file_names.count == 0) {
        ERROR("Must specify filename with .%s extension or directory that contains some", VM_FILE_EXTENSION);
    }
    
    ASMFile asm_file = {};
    asm_file.var_base_name = name;
    asm_file.buffer = malloc(ASM_FILE_BUFFER_CAPACITY);
    asm_file.buffer_capacity = ASM_FILE_BUFFER_CAPACITY;
    asm_file.buffer_index = 0;
    asm_file.handle = fopen(asm_file_name, "wb");
    
    for (int vm_file_index = 0; vm_file_index < vm_file_names.count; ++vm_file_index) {
        char vm_file_name[4096] = {};
        snprintf(vm_file_name, sizeof(vm_file_name), "%.*s/%s", dir.count, dir.data, vm_file_names.data[vm_file_index]);
        
        FILE* vm_file_handle = fopen(vm_file_name, "rb");
        fseek(vm_file_handle, 0, SEEK_END);
        const int vm_file_size = ftell(vm_file_handle);
        fseek(vm_file_handle, 0, SEEK_SET);
        
        char* const vm_file_buffer = malloc(vm_file_size);
        const int bytes_read = fread(vm_file_buffer, 1, vm_file_size, vm_file_handle);
        if (bytes_read != vm_file_size) {
            ERROR("Failed to read entire file: %s", vm_file_name);
        }
            
        VMFile vm_file = {};
        vm_file.begin = vm_file_buffer;
        vm_file.position = vm_file_buffer;
        vm_file.end = vm_file_buffer + vm_file_size;
        
        bool parsing = true;
        while (parsing) {
            const Token token = get_next_token(&vm_file);
            switch (token.type) {
                case TT_IDENTIFIER: {
                    const Command command = parse_command(token.text, &vm_file);
                    write_asm(&asm_file, &command);
                } break;
                
                case TT_EOF: {
                    parsing = false;
                } break;
                
                default: {
                    ERROR("Unexpected token type encountered");
                } break;
            }
        }
    }
    
    if (buffer_has_content(&asm_file)) {
        flush_buffer(&asm_file);
    }
    
    return 0;
}
