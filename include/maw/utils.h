#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>

size_t readfile(const char *filepath, char *out, size_t outsize)
       __attribute__((warn_unused_result));

#endif // UTIL_H

