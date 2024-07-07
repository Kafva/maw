#ifndef JOB_H
#define JOB_H

#include "maw/maw.h"

#include <pthread.h>

enum ThreadStatus {
    THREAD_UNINITIALIZED,
    THREAD_STARTED,
    THREAD_FAILED,
} typedef ThreadStatus;

struct ThreadContext {
    ThreadStatus status;
    const MediaFile *mediafiles;
    ssize_t mediafiles_index;
} typedef ThreadContext;

int maw_job_launch(MediaFile mediafiles[], ssize_t size, size_t jobs)
    __attribute__((warn_unused_result));

#endif // JOB_H
