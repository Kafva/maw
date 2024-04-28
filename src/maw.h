#ifndef AV_H 
#define AV_H

#include <Foundation/Foundation.h>

struct Metadata {
    char* album;
    char* artist;
    char* cover_path;
    bool clear_metadata;
};

int maw_yaml_parse(void);
int maw_dump(char*);
int maw_update(char*, struct Metadata*);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define log(FORMAT, ...) \
    fprintf(stderr, "%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])
#pragma clang diagnostic pop
    
#endif

// vi: ft=objc
