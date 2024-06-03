#ifndef MAW_H
#define MAW_H

#include <stdbool.h>

// Decides what to keep from the input file when no custom values are provided.
enum MetadataPolicy {
    KEEP_CORE_FIELDS    = 0x1,
    KEEP_ALL_FIELDS     = 0x1 << 1,
    KEEP_COVER          = 0x1 << 2,
    CROP_COVER          = 0x1 << 3,
};

#define POLICY_NEEDS_ORIGINAL_COVER(policy) (policy & (KEEP_COVER | CROP_COVER))

struct Metadata {
    char *title;
    char *album;
    char *artist;
    char *cover_path;
};

int maw_update(const char *, const struct Metadata *, const int);

#ifdef MAW_TEST
#include <libavformat/avformat.h>

#define LHS_EMPTY_OR_EQ(lhs, rhs) \
    (lhs == NULL || strlen(lhs) == 0 || strcmp(rhs, lhs) == 0)

bool maw_verify(const char *, const struct Metadata *, const int);
bool maw_verify_cover(const AVFormatContext *,
                      const char *,
                      const struct Metadata *,
                      const int);
#endif

#endif // MAW_H
