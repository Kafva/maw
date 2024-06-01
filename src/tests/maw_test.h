#ifndef MAW_TEST_H
#define MAW_TEST_H

#ifdef MAW_TEST

#include <stdbool.h>

struct Testcase {
    char *desc;
    bool (*fn)(void);
};

bool test_maw_update(void);
bool test_add_cover(void);
bool test_replace_cover(void);
bool test_dual_audio(void);
bool test_dual_video(void);

#define DEFINE_TESTCASES \
    struct Testcase testcases[] = { \
        { .desc = "Basic test",                 .fn = test_maw_update }, \
        { .desc = "Add cover",                  .fn = test_add_cover },  \
        { .desc = "Replace cover",              .fn = test_replace_cover },  \
        { .desc = "Dual audio streams",         .fn = test_dual_audio }, \
        { .desc = "Dual video streams",         .fn = test_dual_video }, \
    }

#endif

#endif // MAW_TEST_H
