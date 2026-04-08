#include "string_view.h"

#include <string.h>

static String construct_string(const char* const data, const int count) {
    String result = {};
    result.data = data;
    result.count = count;
    
    return result;
}

static String construct_string_from_c_string(const char* const s) {
    return construct_string(s, strlen(s));
}

static int reverse_find_index_of(const String string, const char c) {
    if (string.count == 0) {
        return -1;
    }
    
    for (int i = string.count - 1; i >= 0; --i) {
        if (string.data[i] == c) {
            return i;
        }
    }
    
    return -1;
}

static bool string_equals(const String a, const String b) {
    if (a.count != b.count) {
        return false;
    }
    
    for (int i = 0; i < a.count; ++i) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    
    return true;
}

static int string_to_int(const String s) {
    int result = 0;
    for (int i = 0; i < s.count; ++i) {
        const int digit = s.data[i] - '0';
        result = 10 * result + digit;
    }
    
    return result;
}
