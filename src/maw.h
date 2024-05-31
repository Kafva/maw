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

struct Metadata {
    char *title;
    char *album;
    char *artist;
    char *cover_path;
};

int maw_yaml_parse(const char *);

int maw_dump(const char *);
int maw_update(const char *, const struct Metadata *, const enum MetadataPolicy);

#endif // MAW_H
