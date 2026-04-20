#ifndef STRING_VIEW_H
#define STRING_VIEW_H

typedef struct String {
    const char* data;
    int count;
} String;

static String construct_string(const char* data, int count);
static String construct_string_from_c_string(const char* s);
static int reverse_find_index_of(String string, char c);
static bool string_equals(String a, String b);
static int string_to_int(String s);

#define STRING_EQUALS(str, c_str) string_equals((str), construct_string((c_str), sizeof(c_str) - 1))

#endif
