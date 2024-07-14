#ifndef MAW_THREADS_H
#define MAW_THREADS_H

#include "maw/maw.h"

#include <pthread.h>

struct ThreadContext {
    const MediaFile *mediafiles;
    size_t index_start;
    size_t index_end;
    bool dry_run;
    bool exit_ok;
    bool spawned;
} typedef ThreadContext;

int maw_threads_launch(MediaFile mediafiles[], size_t size, size_t thread_count,
                       bool dry_run) __attribute__((warn_unused_result));

#endif // MAW_THREADS_H
