#ifndef TESTS_MAW_VERIFY_H
#define TESTS_MAW_VERIFY_H

#include <stdbool.h>
#include "maw.h"

bool maw_verify(const char *, const Metadata *);

bool maw_verify_cover(const AVFormatContext *,
                      const char *,
                      const Metadata *);

#endif
