#ifndef UTIL_H
#define UTIL_H

#include <unistd.h>

size_t readfile(const char *filepath, char *out, size_t outsize);

#endif // UTIL_H

