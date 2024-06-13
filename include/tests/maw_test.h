#ifndef TESTS_MAW_TEST_H
#define TESTS_MAW_TEST_H

#ifdef MAW_TEST

#include <stdbool.h>
#include "log.h"

struct Testcase {
    char *desc;
    bool (*fn)(const char *);
};

bool test_keep_all(const char *);
bool test_clear_non_core_fields(const char *);
bool test_clear_cover(const char *);
bool test_crop_cover(const char *);
bool test_add_cover(const char *);
bool test_replace_cover(const char *);
bool test_bad_covers(const char *);
bool test_no_audio(const char *);
bool test_dual_audio(const char *);
bool test_dual_video(const char *);


#define DEFINE_TESTCASES \
    struct Testcase testcases[] = { \
        { .desc = "Default behaviour should keep everything as is",   .fn = test_keep_all }, \
        { .desc = "Clear non core fields",                            .fn = test_clear_non_core_fields }, \
        { .desc = "Clear cover",                                      .fn = test_clear_cover },  \
        { .desc = "Add cover",                                        .fn = test_add_cover },  \
        { .desc = "Replace cover",                                    .fn = test_replace_cover }, \
        { .desc = "Bad covers",                                       .fn = test_bad_covers }, \
        { .desc = "No audio streams",                                 .fn = test_no_audio   }, \
        { .desc = "Dual audio streams",                               .fn = test_dual_audio }, \
        { .desc = "Dual video streams",                               .fn = test_dual_video }, \
        { .desc = "Crop cover",                                       .fn = test_crop_cover }, \
    }
#endif


#define MAW_ASSERT_EQ(lhs, rhs, msg) do {\
   if (lhs != rhs) { \
        MAW_LOGF(MAW_ERROR, "%s: got %d, expected %d\n", msg, lhs, rhs); \
        return false; \
   } \
} while (0)

#endif // TESTS_MAW_TEST_H
