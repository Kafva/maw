#ifndef MAW_UPDATE_H
#define MAW_UPDATE_H

#include "maw/maw.h"

int maw_update_load(MawConfig *cfg, MawArguments *args,
                    MediaFile mediafiles[MAW_MAX_FILES],
                    ssize_t *mediafiles_count)
    __attribute__((warn_unused_result));
void maw_update_free(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count);
int maw_update(const MediaFile *mediafile) __attribute__((warn_unused_result));

#endif // MAW_UPDATE_H
