#include "runner.h"
#include "log.h"

#include <unistd.h>

#include <sys/event.h>

static void *maw_runner_thread(void *);
int maw_runner_launch(Metadata metadata[], size_t size, size_t jobs);

////////////////////////////////////////////////////////////////////////////////

// Must be locked before read/write
static int next_job_index = 0;
static pthread_mutex_t lock;

static void *maw_runner_thread(void *arg) {
    int r;
    int idx;
    const ThreadContext *arr = (ThreadContext*)arg;

    while (next_job_index >= 0) {
        r = pthread_mutex_lock(&lock);
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_mutex_lock: %s\n", strerror(r));
            return NULL;
        }
        next_job_index--;
        idx = next_job_index;
        r = pthread_mutex_unlock(&lock);
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_mutex_unlock: %s\n", strerror(r));
            return NULL;
        }
        if (idx < 0) {
            break;
        }

        MAW_LOGF(MAW_INFO, "Thread started: %s\n", arr[idx].metadata.filepath);
        usleep(2000);
        MAW_LOGF(MAW_INFO, "Thread done: %s\n", arr[idx].metadata.filepath);
    }

end:
    return NULL;
}

int maw_runner_launch(Metadata arr[], size_t size, size_t jobs) {
    int r = INTERNAL_ERROR;
    int status;
    pthread_t *threads = NULL;
    ThreadContext *thread_ctxs = NULL;

    next_job_index = size;

    threads = calloc(jobs, sizeof(pthread_t));
    if (threads == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    thread_ctxs = calloc(jobs, sizeof(ThreadContext));
    if (thread_ctxs == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    r = pthread_mutex_init(&lock, NULL);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "pthread_mutex_init: %s\n", strerror(r));
        goto end;
    }

    for (size_t i = 0; i < jobs; i++) {
        r = pthread_create(&threads[i], NULL, maw_runner_thread, (void*)arr);
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_create: %s\n", strerror(r));
            goto end;
        }
    }

    for (size_t i = 0; i < jobs; i++) {
        r = pthread_join(threads[i], NULL);
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_join: %s\n", strerror(r));
            goto end;
        }
    }


    r = 0;
    MAW_LOG(MAW_INFO, "Threads done\n");
end:
    free(threads);
    free(thread_ctxs);

    r = pthread_mutex_destroy(&lock);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "pthread_mutex_destroy: %s\n", strerror(r));
    }
    
    return r;
}
