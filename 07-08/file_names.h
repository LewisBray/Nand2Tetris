#ifndef FILE_NAMES_H
#define FILE_NAMES_H

#define FILE_NAME_BUFFER_SIZE 4096

typedef struct FileNames {
    char (*data)[FILE_NAME_BUFFER_SIZE];
    int count;
} FileNames;

FileNames get_file_names_with_extension(const char* dir, const char* ext);

#endif
