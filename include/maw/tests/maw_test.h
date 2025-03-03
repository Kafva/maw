#ifndef TESTS_MAW_TEST_H
#define TESTS_MAW_TEST_H

#include "maw/log.h"

#include <stdbool.h>

struct Testcase {
    char *desc;
    bool (*fn)(const char *);
};

int run_tests(const char *match_testcase);

#define MAW_ASSERT_EQ(expected, actual, msg) \
    do { \
        if (expected != actual) { \
            MAW_LOGF(MAW_ERROR, "%s: expected %d, actual: %d", msg, expected, actual); \
            return false; \
        } \
    } while (0)

#endif // TESTS_MAW_TEST_H
