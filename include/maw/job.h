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
    const Metadata *metadata;
    int metadata_index;
    ThreadStatus status;
} typedef ThreadContext;

int maw_job_launch(Metadata metadata[], size_t size, size_t jobs)
    __attribute__((warn_unused_result));

#endif // JOB_H
