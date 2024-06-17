#ifndef RUNNER_H
#define RUNNER_H

#include "maw.h"

#include <pthread.h>

struct RunnerContext {
    struct kevent *events;
    pthread_t *threads;
    int kfd;

} typedef RunnerContext;

int maw_runner_launch(Metadata[], size_t,  size_t);

#endif // RUNNER_H
