#include "runner.h"
#include "log.h"

#include <pthread.h>
#include <unistd.h>

#include <sys/event.h>


static void *maw_runner_thread(void *arg) {
    const Metadata *metadata = (Metadata*)arg;
    MAW_LOGF(MAW_INFO, "Thread started: %s", metadata->filepath);
    usleep(2000);
    MAW_LOGF(MAW_INFO, "Thread done: %s", metadata->filepath);
    return NULL;
}

int maw_runner_launch(Metadata arr[], size_t size, size_t jobs) {
    int r = INTERNAL_ERROR;
    pthread_t *threads = NULL;
    struct kevent *events = NULL;
    int kfd = -1;
    int *fds = NULL;
    size_t active_jobs = 0;
    size_t jobs_done = 0;

    kfd = kqueue();
    if (kfd != 0) {
        MAW_PERROR("Failed to setup kqueue");
        goto end;
    }

    fds = calloc(jobs, sizeof(int));
    if (fds == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    threads = calloc(jobs, sizeof(pthread_t));
    if (threads == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    //
    EV_SET(events, kfd, EVFILT_READ, EV_ADD, 0, 0, NULL);

    // for (size_t i = 0; i < size; i++) {
    //     if (active_jobs < jobs) {
    //         pthread_create(&threads[active_jobs], NULL, maw_runner_thread, (void*)(&arr[i]));
    //         active_jobs++;
    //         continue;
    //     }

    //     while (true) {
    //         pthread_join(threads[i], NULL);
    //         jobs_done++;
    //         i = (i + 1) % jobs;
    //     }
    // }

end:
    free(threads);
    free(fds);
    (void)close(kfd);
    return r;
}
