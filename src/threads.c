#include "maw/threads.h"
#include "maw/log.h"
#include "maw/update.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static void maw_clock_measure(time_t);
static void *maw_threads_worker(void *);

////////////////////////////////////////////////////////////////////////////////

static void maw_clock_measure(time_t start_time) {
    time_t end_time;
    time_t elapsed;
    time_t minutes, seconds;

    end_time = time(NULL);
    elapsed = end_time - start_time;

    minutes = elapsed / 60;
    seconds = elapsed % 60;

    if (minutes > 0 || seconds > 0) {
        MAW_LOGF(MAW_INFO, "Done: %02ld:%02ld", minutes, seconds);
    }
}

static void *maw_threads_worker(void *arg) {
    int r;
    ThreadContext *ctx = (ThreadContext *)arg;
    unsigned long tid = (unsigned long)pthread_self();
    size_t i;
    size_t noop_done = 0;
    size_t done = 0;

    MAW_LOGF(MAW_DEBUG, "Thread #%lu started: [%zu,%zu]", tid, ctx->index_start,
             ctx->index_end);

    for (i = ctx->index_start; i < ctx->index_end; i++) {
        r = maw_update(&ctx->mediafiles[i], ctx->dry_run);
        if (r == RESULT_OK) {
            done++;
        }
        else if (r == RESULT_NOOP) {
            noop_done++;
        }
        else {
            goto end;
        }
    }

    ctx->exit_ok = true;
end:
    if (!ctx->exit_ok) {
        MAW_LOGF(MAW_ERROR, "Thread #%lu: failed [%zu change(s)] [%zu noop(s)]",
                 tid, done, noop_done);
    }
    else {
        MAW_LOGF(MAW_INFO, "Thread #%lu: ok [%zu change(s)] [%zu noop(s)]", tid,
                 done, noop_done);
    }
    return NULL;
}

// Return non-zero if at least one thread fails
int maw_threads_launch(MediaFile mediafiles[], size_t size, size_t thread_count,
                       bool dry_run) {
    int status = -1;
    int r = RESULT_ERR_INTERNAL;
    pthread_t *threads = NULL;
    ThreadContext *thread_ctxs = NULL;
    time_t start_time;
    size_t increment;
    size_t leftover;

    start_time = time(NULL);

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

    increment = size / thread_count;
    leftover = size % thread_count;

    MAW_LOGF(MAW_INFO, "Launching %zu threads: %zu job item(s)", thread_count,
             size);
    for (size_t i = 0; i < thread_count; i++) {
        thread_ctxs[i].spawned = false;
        thread_ctxs[i].exit_ok = false;
        thread_ctxs[i].mediafiles = mediafiles;
        thread_ctxs[i].dry_run = dry_run;
        thread_ctxs[i].index_start = increment * i;
        thread_ctxs[i].index_end = i == thread_count - 1
                                       ? increment * (i + 1) + leftover
                                       : increment * (i + 1);

        r = pthread_create(&threads[i], NULL, maw_threads_worker,
                           (void *)(&thread_ctxs[i]));
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_create: %s", strerror(r));
            goto end;
        }
        thread_ctxs[i].spawned = true;
    }

    status = 0;
end:
    if (thread_ctxs != NULL) {
        for (size_t i = 0; i < thread_count; i++) {
            if (!thread_ctxs[i].spawned)
                continue;

            // Save 'status' so that we do not return 0 if a thread failed
            r = pthread_join(threads[i], NULL);
            if (r != 0) {
                MAW_LOGF(MAW_ERROR, "pthread_join: %s", strerror(r));
                status = -1;
            }

            if (!thread_ctxs[i].exit_ok) {
                status = -1;
            }
        }
    }

    free(thread_ctxs);
    free(threads);

    if (status == 0) {
        maw_clock_measure(start_time);
    }
    return status;
}
