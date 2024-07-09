#ifndef MAW_MEDIAFILES_H
#define MAW_MEDIAFILES_H

#include "maw/maw.h"

int maw_mediafiles_alloc(MawConfig *cfg, MawArguments *args,
                         MediaFile mediafiles[MAW_MAX_FILES],
                         ssize_t *mediafiles_count)
    __attribute__((warn_unused_result));

void maw_mediafiles_free(MediaFile mediafiles[MAW_MAX_FILES], ssize_t count);

#endif // MAW_MEDIAFILES_H
