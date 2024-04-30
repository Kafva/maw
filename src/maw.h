#ifndef MAW_H 
#define MAW_H

#include <stdbool.h>

struct Metadata {
    char* title;
    char* album;
    char* artist;
    char* cover_path;
    bool clear_metadata;
};

int maw_yaml_parse(const char*);

int maw_init(int);
int maw_dump(const char*);
int maw_update(const char*, const struct Metadata*);

    
#endif // MAW_H
