#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <unistd.h>

size_t readfile(const char *filepath, char *out, size_t outsize)
       __attribute__((warn_unused_result));
int movefile(const char *src, const char *dst)
       __attribute__((warn_unused_result));
bool on_same_device(const char *path1, const char *path2)
       __attribute__((warn_unused_result));
bool isfile(const char *path)
       __attribute__((warn_unused_result));
bool isdir(const char *path)
       __attribute__((warn_unused_result));

#endif // UTIL_H

