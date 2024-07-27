#ifndef MAW_UTILS_H
#define MAW_UTILS_H

#include "maw/maw.h"

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

size_t readfile(const char *filepath, char **out)
    __attribute__((warn_unused_result));
int movefile(const char *src, const char *dst)
    __attribute__((warn_unused_result));
bool isfile(const char *path);
bool on_same_device(const char *path1, const char *path2);
uint32_t hash(const char *data);
int basename_no_ext(const char *filepath, char *out, size_t outsize)
    __attribute__((warn_unused_result));
const char *extname(const char *s);

#endif // MAW_UTILS_H
