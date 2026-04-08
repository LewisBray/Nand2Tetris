#ifndef ERROR_H
#define ERROR_H

#include <stdlib.h>
#include <stdio.h>

#define ERROR(str, ...) printf(str, ##__VA_ARGS__); puts(""); exit(1)

#endif
