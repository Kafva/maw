#include "maw/threads.h"
#include "maw/log.h"
#include "maw/update.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void maw_clock_measure(time_t);
static void *maw_threads_worker(void *);

////////////////////////////////////////////////////////////////////////////////

#define WITH_LOCK(r, expr) \
    do { \
        r = pthread_mutex_lock(&lock); \
        if (r != 0) { \
            MAW_LOGF(MAW_ERROR, "pthread_mutex_lock: %s", strerror(r)); \
            goto end; \
        } \
        expr r = pthread_mutex_unlock(&lock); \
        if (r != 0) { \
            MAW_LOGF(MAW_ERROR, "pthread_mutex_unlock: %s", strerror(r)); \
            goto end; \
        } \
    } while (0)

// Must be locked before read/write
static ssize_t next_mediafiles_index;
static pthread_mutex_t lock;

static void maw_clock_measure(time_t start_time) {
    time_t end_time;
    time_t elapsed;
    time_t minutes, seconds;

    end_time = time(NULL);
    elapsed = end_time - start_time;

    minutes = elapsed / 60;
    seconds = elapsed % 60;

    if (minutes == 0 && seconds == 0) {
        MAW_LOG(MAW_INFO, "Done");
    }
    else {
        MAW_LOGF(MAW_INFO, "Done: %02ld:%02ld", minutes, seconds);
    }
}

static void *maw_threads_worker(void *arg) {
    int r;
    int finished_jobs = 0;
    ThreadContext *ctx = (ThreadContext *)arg;
    unsigned long tid = (unsigned long)pthread_self();

    if (ctx->status != THREAD_STARTED) {
        MAW_LOGF(MAW_ERROR, "Thread #%lu not properly started", tid);
        return NULL;
    }

    MAW_LOGF(MAW_DEBUG, "Thread #%lu started", tid);

    while (true) {
        if (next_mediafiles_index < 0) {
            goto end;
        }

        // Take the next mediafiles_index
        WITH_LOCK(r, {
            next_mediafiles_index--;
            ctx->mediafiles_index = next_mediafiles_index;
        });

        if (ctx->mediafiles_index < 0) {
            goto end;
        }

        // Do work on current mediafiles_index
        r = maw_update(
            (const MediaFile *)(&ctx->mediafiles[ctx->mediafiles_index]));

        // On fail, set next_mediafiles_index to -1, cancelling other threads
        if (r != 0) {
            ctx->status = THREAD_FAILED;
            WITH_LOCK(r, { next_mediafiles_index = -1; });
        }
        else {
            finished_jobs++;
        }
    }
end:
    if (ctx->status == THREAD_FAILED) {
        MAW_LOGF(MAW_ERROR, "Thread #%lu: failed [done %d job(s)]", tid,
                 finished_jobs);
    }
    else {
        MAW_LOGF(MAW_INFO, "Thread #%lu: ok [done %d job(s)]", tid,
                 finished_jobs);
    }
    return NULL;
}

// @return non-zero if at least one thread fails
int maw_threads_launch(MediaFile mediafiles[], ssize_t size,
                       size_t thread_count) {
    int status = -1;
    int r = MAW_ERR_INTERNAL;
    pthread_t *threads = NULL;
    ThreadContext *thread_ctxs = NULL;
    time_t start_time;

    start_time = time(NULL);
    next_mediafiles_index = size;

    threads = calloc(thread_count, sizeof(pthread_t));
    if (threads == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    thread_ctxs = calloc(thread_count, sizeof(ThreadContext));
    if (thread_ctxs == NULL) {
        MAW_PERROR("calloc");
        goto end;
    }

    for (size_t i = 0; i < thread_count; i++) {
        thread_ctxs[i].status = THREAD_UNINITIALIZED;
        thread_ctxs[i].mediafiles_index = -1;
        thread_ctxs[i].mediafiles = mediafiles;
    }

    r = pthread_mutex_init(&lock, NULL);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "pthread_mutex_init: %s", strerror(r));
        goto end;
    }

    for (size_t i = 0; i < thread_count; i++) {
        thread_ctxs[i].status = THREAD_STARTED;
        r = pthread_create(&threads[i], NULL, maw_threads_worker,
                           (void *)(&thread_ctxs[i]));
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_create: %s", strerror(r));
            goto end;
        }
    }

    status = 0;
end:
    if (thread_ctxs != NULL) {
        for (size_t i = 0; i < thread_count; i++) {
            if (thread_ctxs[i].status == THREAD_UNINITIALIZED)
                continue;

            // Save 'status' so that we do not return 0 if a thread failed
            r = pthread_join(threads[i], NULL);
            if (r != 0) {
                MAW_LOGF(MAW_ERROR, "pthread_join: %s", strerror(r));
                status = -1;
            }

            if (thread_ctxs[i].status == THREAD_FAILED) {
                status = -1;
            }
        }
    }

    free(thread_ctxs);
    free(threads);
    r = pthread_mutex_destroy(&lock);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "pthread_mutex_destroy: %s", strerror(r));
        status = -1;
    }

    if (status == 0) {
        maw_clock_measure(start_time);
    }
    return status;
}
