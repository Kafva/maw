#ifndef MAW_PLAYLISTS_H
#define MAW_PLAYLISTS_H

#include "maw/maw.h"

int maw_playlists_path(MawConfig *cfg, const char *name, char *out, size_t size)
    __attribute__((warn_unused_result));
int maw_playlists_gen(MawConfig *cfg) __attribute__((warn_unused_result));

#endif // MAW_PLAYLISTS_H
