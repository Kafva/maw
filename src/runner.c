#include "runner.h"
#include "log.h"

#include <unistd.h>

#include <sys/event.h>

static void *maw_runner_thread(void *);
int maw_runner_launch(Metadata [], size_t, size_t);

////////////////////////////////////////////////////////////////////////////////

static void *maw_runner_thread(void *arg) {
    const Metadata *metadata = (Metadata*)arg;
    //MAW_LOGF(MAW_INFO, "Thread started: %s\n", metadata->filepath);
    usleep(2000);
    //MAW_LOGF(MAW_INFO, "Thread done: %s\n", metadata->filepath);
    return NULL;
}

int maw_runner_launch(Metadata arr[], size_t size, size_t jobs) {
    int r = INTERNAL_ERROR;
    pthread_t *threads = NULL;
    size_t active_jobs = 0;
    size_t jobs_done = 0;
    RunnerContext *runner = NULL;

    threads = calloc(jobs, sizeof(pthread_t));
    if (threads == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    for (size_t i = 0; i < size; i++) {
        //if (active_jobs < jobs) {
            r = pthread_create(&threads[active_jobs], NULL,
                               maw_runner_thread,
                               (void*)(&arr[i]));
            if (r != 0) {
                MAW_LOGF(MAW_ERROR, "pthread_create: %s\n", strerror(r));
                goto end;
            }

            // active_jobs++;
            //continue;
        //}

        //while (true) {
            r = pthread_join(threads[active_jobs], NULL);
            if (r != 0) {
                MAW_LOGF(MAW_ERROR, "pthread_join: %s\n", strerror(r));
                goto end;
            }

            // active_jobs--;
            // jobs_done++;
        //}
    }


    r = 0;
    MAW_LOG(MAW_INFO, "Threads done\n");
end:
    free(threads);
    return r;
}
