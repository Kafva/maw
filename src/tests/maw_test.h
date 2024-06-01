#ifndef MAW_TEST_H
#define MAW_TEST_H

#ifdef MAW_TEST

#include <stdbool.h>

struct Testcase {
    char *desc;
    bool (*fn)(void);
};

bool test_maw_update(void);
bool test_set_cover(void);
bool test_dual_audio(void);
bool test_dual_video(void);

#define DEFINE_TESTCASES \
    struct Testcase testcases[] = { \
        { .desc = "Basic test",                 .fn = test_maw_update }, \
        { .desc = "Set cover",                  .fn = test_set_cover },  \
        { .desc = "Dual audio streams",         .fn = test_dual_audio }, \
        { .desc = "Dual video streams",         .fn = test_dual_video }, \
    }

#endif

#endif // MAW_TEST_H
