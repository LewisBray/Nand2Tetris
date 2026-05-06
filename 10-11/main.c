// define this if you want the xml output in addition to the vm output.
// this isn't a brilliant way of doing this, in a more serious project
// the compilation should maybe output some intermediate representation
// of functions/objects/etc.. for more serious type + error checking
// and then the output back-end could be chosen but since the problem
// is relatively constrained, this will do for finishing this project
// #define OUTPUT_XML

#define _CRT_SECURE_NO_WARNINGS

#include "string_view.h"
#include "file_names.h"
#include "jack_file.h"
#include "compile.h"
#include "error.h"

#include "string_view.c"
#include "jack_file.c"
#include "compile.c"

#include <stdio.h>

static String remove_file_extension(const char* const file) {
    String result = construct_string_from_c_string(file);
    const int dot_index = reverse_find_index_of(result, '.');
    if (dot_index != -1) {
        result.count = dot_index;
    }
    
    return result;
}

static const char JACK_FILE_EXTENSION[] = "jack";

int main(const int argc, const char* const argv[]) {
    if (argc != 2) {
        ERROR("Supply the .%s file or a directory as the only argument", JACK_FILE_EXTENSION);
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
    
    FileNames jack_file_names = {};
    if (dot_index == -1) {
        char dir_buffer[4096] = {};
        snprintf(dir_buffer, sizeof(dir_buffer), "%.*s", dir.count, dir.data);
        jack_file_names = get_file_names_with_extension(dir_buffer, JACK_FILE_EXTENSION);
    } else if (dot_index + 5 == arg1.count
        && arg1.data[arg1.count - 4] == JACK_FILE_EXTENSION[0]
        && arg1.data[arg1.count - 3] == JACK_FILE_EXTENSION[1]
        && arg1.data[arg1.count - 2] == JACK_FILE_EXTENSION[2]
        && arg1.data[arg1.count - 1] == JACK_FILE_EXTENSION[3]) {
        jack_file_names.data = malloc(FILE_NAME_BUFFER_SIZE);
        jack_file_names.count = 1;
        snprintf(jack_file_names.data[0], FILE_NAME_BUFFER_SIZE, "%.*s.%s", name.count, name.data, JACK_FILE_EXTENSION);
    }
    
    if (jack_file_names.count == 0) {
        ERROR("Must specify filename with .%s extension or directory that contains some", JACK_FILE_EXTENSION);
    }
    
    for (int jack_file_index = 0; jack_file_index < jack_file_names.count; ++jack_file_index) {
        char jack_file_name[4096] = {};
        snprintf(jack_file_name, sizeof(jack_file_name), "%.*s/%s", dir.count, dir.data, jack_file_names.data[jack_file_index]);
        JackFile jack_file = open_jack_file(jack_file_name);
        
        Output out = {};
        
        const String name = remove_file_extension(jack_file_names.data[jack_file_index]);
        
        char vm_file_name[4096] = {};
        snprintf(vm_file_name, sizeof(vm_file_name), "%.*s/%.*s.%s", dir.count, dir.data, name.count, name.data, "vm");
        out.vm = fopen(vm_file_name, "wb");
        
#ifdef OUTPUT_XML
        char xml_file_name[4096] = {};
        snprintf(xml_file_name, sizeof(xml_file_name), "%.*s/%.*s.%s", dir.count, dir.data, name.count, name.data, "xml");
        out.xml = fopen(xml_file_name, "wb");
#endif
        
        compile(&jack_file, &out);
    }
    
    return 0;
}
