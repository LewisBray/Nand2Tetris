#define _CRT_SECURE_NO_WARNINGS

#include "string_view.h"
#include "tokenise.h"
#include "error.h"
#include "parse.h"

#include "string_view.c"
#include "tokenise.c"
#include "parse.c"

#include <stdio.h>

static const char ASM_FILE_EXTENSION[] = "asm";
static const char HACK_FILE_EXTENSION[] = "hack";

int main(const int argc, const char* const argv[]) {
    if (argc != 2) {
        ERROR("Supply the .%s file as the only argument.", ASM_FILE_EXTENSION);
    }
    
    const String filename = construct_string_from_c_string(argv[1]);
    
    bool has_asm_extension = false;
    const int dot_index = reverse_find_index_of(filename, '.');
    if (dot_index != -1) {
        String extension = {};
        extension.data = filename.data + dot_index + 1;
        extension.count = filename.count - dot_index - 1;
        has_asm_extension = STRING_EQUALS_C_STRING(extension, ASM_FILE_EXTENSION);
    }
    
    if (!has_asm_extension) {
        ERROR("Filename must have .%s extension", ASM_FILE_EXTENSION);
    }
    
    FILE* asm_file = fopen(argv[1], "rb");
    fseek(asm_file, 0, SEEK_END);
    const int asm_file_size = ftell(asm_file);
    char* const asm_file_buffer = malloc(asm_file_size);
    
    fseek(asm_file, 0, SEEK_SET);
    const int bytes_read = fread(asm_file_buffer, 1, asm_file_size, asm_file);
    if (bytes_read != asm_file_size) {
        ERROR("Failed to read entire .%s file", ASM_FILE_EXTENSION);
    }
    
    const Tokens tokens = tokenise(asm_file_buffer, asm_file_size);
    
    const ParseResult parse_result = parse(tokens);
    const Instructions instructions = parse_result.instructions;
    const JumpSymbols jump_symbols = parse_result.jump_symbols;
    
    const Buffer hack_file_buffer = output_instructions_to_buffer(instructions, jump_symbols);
    
    char hack_file_name[1024] = {}; // probably large enough
    snprintf(hack_file_name, sizeof(hack_file_name), "%.*s.%s", dot_index, filename.data, HACK_FILE_EXTENSION);
    hack_file_name[sizeof(hack_file_name) - 1] = '\0';
    
    FILE* hack_file = fopen(hack_file_name, "wb");
    const int hack_file_size = get_size(&hack_file_buffer);
    fwrite(hack_file_buffer.begin, 1, hack_file_size, hack_file);
    
    return 0;
}
