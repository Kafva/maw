#ifndef RUNNER_H
#define RUNNER_H

#include "maw.h"

#include <pthread.h>

enum ThreadStatus {
    UNINITIALIZED,
    STARTED,
    FAILED,
} typedef ThreadStatus;

struct ThreadContext {
    const Metadata *metadata;
    int metadata_index;
    ThreadStatus status;
} typedef ThreadContext;

int maw_runner_launch(Metadata metadata[], size_t size, size_t jobs);

#endif // RUNNER_H
