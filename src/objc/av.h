#ifndef AV_H 
#define AV_H

#include <Foundation/Foundation.h>

int av_yaml_parse(void);
int av_dump(char*);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define log(FORMAT, ...) \
    fprintf(stderr, "%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String])

#pragma clang diagnostic pop
    
#endif

// vi: ft=objc
