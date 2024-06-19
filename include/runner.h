#ifndef RUNNER_H
#define RUNNER_H

#include "maw.h"

#include <pthread.h>

struct ThreadContext {
    const Metadata metadata;
    int r;
    
} typedef ThreadContext;

int maw_runner_launch(Metadata arr[], size_t size, size_t jobs);

#endif // RUNNER_H
