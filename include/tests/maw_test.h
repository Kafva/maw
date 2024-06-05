#ifndef MAW_TEST_H
#define MAW_TEST_H

#ifdef MAW_TEST

#include <stdbool.h>
#include "log.h"

struct Testcase {
    char *desc;
    bool (*fn)(const char *);
};

bool test_keep_cover_policy(const char *);
bool test_crop_cover_policy(const char *);
bool test_keep_all_fields_policy(const char *);
bool test_keep_core_fields_policy(const char *);
bool test_add_cover(const char *);
bool test_replace_cover(const char *);
bool test_bad_covers(const char *);
bool test_no_audio(const char *);
bool test_dual_audio(const char *);
bool test_dual_video(const char *);

#define DEFINE_TESTCASES \
    struct Testcase testcases[] = { \
        { .desc = "Keep cover",                 .fn = test_keep_cover_policy }, \
        { .desc = "Crop cover",                 .fn = test_crop_cover_policy }, \
        { .desc = "Keep all fields",            .fn = test_keep_all_fields_policy }, \
        { .desc = "Keep core fields",           .fn = test_keep_core_fields_policy }, \
        { .desc = "Add cover",                  .fn = test_add_cover },  \
        { .desc = "Replace cover",              .fn = test_replace_cover }, \
        { .desc = "Bad covers",                 .fn = test_bad_covers }, \
        { .desc = "No audio streams",           .fn = test_no_audio   }, \
        { .desc = "Dual audio streams",         .fn = test_dual_audio }, \
        { .desc = "Dual video streams",         .fn = test_dual_video }, \
    }
#endif


#define MAW_ASSERT_EQ(lhs, rhs, msg) do {\
   if (lhs != rhs) { \
        MAW_LOGF(MAW_ERROR, "%s: got %d, expected %d\n", msg, lhs, rhs); \
        return false; \
   } \
} while (0)

#endif // MAW_TEST_H
