#include "job.h"
#include "log.h"

static void *maw_job_thread(void *);

////////////////////////////////////////////////////////////////////////////////

#define WITH_LOCK(r, expr) do { \
    r = pthread_mutex_lock(&lock); \
    if (r != 0) { \
        MAW_LOGF(MAW_ERROR, "pthread_mutex_lock: %s", strerror(r)); \
        goto end; \
    } \
    expr \
    r = pthread_mutex_unlock(&lock); \
    if (r != 0) { \
        MAW_LOGF(MAW_ERROR, "pthread_mutex_unlock: %s", strerror(r)); \
        goto end; \
    } \
} while (0) \

// Must be locked before read/write
static int next_metadata_index;
static pthread_mutex_t lock;

static void *maw_job_thread(void *arg) {
    int r;
    int finished_jobs = 0;
    ThreadContext *ctx = (ThreadContext*)arg;
    unsigned long tid = (unsigned long)pthread_self();

    if (ctx->status != THREAD_STARTED) {
        MAW_LOGF(MAW_ERROR, "Thread #%lu not properly started", tid);
        return NULL;
    }

    MAW_LOGF(MAW_DEBUG, "Thread #%lu started", tid);

    while (true) {
        if (next_metadata_index < 0) {
            goto end;
        }

        // Take the next metadata_index
        WITH_LOCK(r, {
            next_metadata_index--;
            ctx->metadata_index = next_metadata_index;
        });

        if (ctx->metadata_index < 0) {
            goto end;
        }

        // Do work on current metadata_index
        r = maw_update((const Metadata*)(&ctx->metadata[ctx->metadata_index]));

        // On fail, set next_metadata_index to -1, cancelling other threads
        if (r != 0) {
            ctx->status = THREAD_FAILED;
            WITH_LOCK(r, {
                next_metadata_index = -1;
            });
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
        MAW_LOGF(MAW_DEBUG, "Thread #%lu: ok [done %d job(s)]", tid,
                                                           finished_jobs);
    }
    return NULL;
}

// @return non-zero if at least one thread fails
int maw_job_launch(Metadata metadata[], size_t size, size_t thread_count) {
    int status = -1;
    int r = INTERNAL_ERROR;
    pthread_t *threads = NULL;
    ThreadContext *thread_ctxs = NULL;

    next_metadata_index = size;

    threads = calloc(thread_count, sizeof(pthread_t));
    if (threads == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    thread_ctxs = calloc(thread_count, sizeof(ThreadContext));
    if (thread_ctxs == NULL) {
        MAW_LOG(MAW_ERROR, "Out of memory");
        goto end;
    }

    for (size_t i = 0; i < thread_count; i++) {
        thread_ctxs[i].status = THREAD_UNINITIALIZED;
        thread_ctxs[i].metadata_index = -1;
        thread_ctxs[i].metadata = metadata;
    }

    r = pthread_mutex_init(&lock, NULL);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "pthread_mutex_init: %s", strerror(r));
        goto end;
    }

    for (size_t i = 0; i < thread_count; i++) {
        thread_ctxs[i].status = THREAD_STARTED;
        r = pthread_create(&threads[i], NULL,
                           maw_job_thread,
                           (void*)(&thread_ctxs[i]));
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

    return status;
}
