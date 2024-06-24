#ifndef CFG_H
#define CFG_H

#include <yaml.h>

int maw_cfg_parse(const char *filepath) __attribute__((warn_unused_result));

#endif // CFG_H
