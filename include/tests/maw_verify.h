#ifndef TESTS_MAW_VERIFY_H
#define TESTS_MAW_VERIFY_H

#include <stdbool.h>
#include <libavformat/avformat.h>

#include "maw.h"

#define LHS_EMPTY_OR_EQ(lhs, rhs) \
    (lhs == NULL || strlen(lhs) == 0 || strcmp(rhs, lhs) == 0)

bool maw_verify(const char *, const Metadata *);

#endif
