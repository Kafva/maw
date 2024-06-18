#ifndef RUNNER_H
#define RUNNER_H

#include "maw.h"

#include <pthread.h>

struct RunnerContext {
    struct kevent *events;
    pthread_t *threads;
    int kfd;
} typedef RunnerContext;

int maw_runner_launch(Metadata arr[], size_t size, size_t jobs);

#endif // RUNNER_H
