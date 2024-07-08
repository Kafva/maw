#ifndef TESTS_MAW_TEST_H
#define TESTS_MAW_TEST_H

#include "maw/log.h"

#include <stdbool.h>

struct Testcase {
    char *desc;
    bool (*fn)(const char *);
};

int run_tests(const char *match_testcase);

#define MAW_ASSERT_EQ(lhs, rhs, msg) \
    do { \
        if (lhs != rhs) { \
            MAW_LOGF(MAW_ERROR, "%s: got %d, expected %d", msg, lhs, rhs); \
            return false; \
        } \
    } while (0)

#endif // TESTS_MAW_TEST_H
