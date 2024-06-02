#ifndef MAW_TEST_H
#define MAW_TEST_H

#ifdef MAW_TEST

#include <stdbool.h>

struct Testcase {
    char *desc;
    bool (*fn)(void);
};

bool test_keep_cover(void);
bool test_add_cover(void);
bool test_replace_cover(void);
bool test_bad_covers(void);
bool test_dual_audio(void);
bool test_dual_video(void);

#define DEFINE_TESTCASES \
    struct Testcase testcases[] = { \
        { .desc = "Keep cover",                 .fn = test_keep_cover }, \
        { .desc = "Add cover",                  .fn = test_add_cover },  \
        { .desc = "Replace cover",              .fn = test_replace_cover },  \
        { .desc = "Bad covers",                 .fn = test_bad_covers }, \
        { .desc = "Dual audio streams",         .fn = test_dual_audio }, \
        { .desc = "Dual video streams",         .fn = test_dual_video }, \
    }

#endif

#endif // MAW_TEST_H
