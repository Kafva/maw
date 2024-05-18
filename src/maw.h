#ifndef MAW_H
#define MAW_H

#include <stdbool.h>

struct Metadata {
    char *title;
    char *album;
    char *artist;
    char *cover_path;
    bool clear_metadata;
};

int maw_yaml_parse(const char *);

void maw_log(int, const char *, int, const char *, ...)
             __attribute__((format (printf, 4, 5)));
int maw_init(int);
int maw_dump(const char *);
int maw_update(const char *, const struct Metadata *);


#define MAW_LOG(level, fmt, ...) \
    maw_log(level, __FILE_NAME__, __LINE__, fmt, ##__VA_ARGS__)

#endif // MAW_H
