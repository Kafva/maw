#ifndef CFG_H
#define CFG_H

#include <yaml.h>

int maw_cfg_parse(const char *filepath) __attribute__((warn_unused_result));
int maw_cfg_init(const char *filepath, yaml_parser_t *parser)
                 __attribute__((warn_unused_result));
void maw_cfg_deinit(yaml_parser_t *parser);

#endif // CFG_H
