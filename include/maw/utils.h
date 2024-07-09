#ifndef MAW_UTILS_H
#define MAW_UTILS_H

#include "maw/maw.h"

#include <stdbool.h>
#include <unistd.h>
#ifdef __linux__
#include <stdint.h>
#endif

size_t readfile(const char *filepath, char *out, size_t outsize)
    __attribute__((warn_unused_result));
int movefile(const char *src, const char *dst)
    __attribute__((warn_unused_result));
bool on_same_device(const char *path1, const char *path2);
bool isfile(const char *path);
bool isdir(const char *path);
uint32_t hash(const char *data);

#endif // MAW_UTILS_H
