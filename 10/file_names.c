#include "file_names.h"

#include <stdio.h>

#include <Windows.h>

FileNames get_file_names_with_extension(const char* const dir, const char* const ext) {
    FileNames result = {};
    
    char file_wildcard[4096] = {};
    snprintf(file_wildcard, sizeof(file_wildcard), "%s/*.%s", dir, ext);
    
    WIN32_FIND_DATAA find_data = {};
    HANDLE find_handle = FindFirstFileA(file_wildcard, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            ++result.count;
        } while (FindNextFile(find_handle, &find_data) != FALSE);
        
        FindClose(find_handle);
        
        find_handle = FindFirstFileA(file_wildcard, &find_data);
        if (find_handle != INVALID_HANDLE_VALUE) {
            result.data = malloc(result.count * FILE_NAME_BUFFER_SIZE);
            int index = 0;
            do {
                memset(result.data + index, 0, FILE_NAME_BUFFER_SIZE);
                for (int i = 0; i < min(sizeof(find_data.cFileName), FILE_NAME_BUFFER_SIZE); ++i) {
                    result.data[index][i] = find_data.cFileName[i];
                }
                
                result.data[index][FILE_NAME_BUFFER_SIZE - 1] = '\0';
                ++index;
            } while (FindNextFile(find_handle, &find_data) != FALSE);
            
            FindClose(find_handle);
        }
    }
    
    return result;
}