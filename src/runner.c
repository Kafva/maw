#include "runner.h"
#include "log.h"

static void *maw_runner_thread(void *);


// Must be locked before read/write
static int next_metadata_index;
static pthread_mutex_t lock;

static void *maw_runner_thread(void *arg) {
    int r;
    ThreadContext *ctx = (ThreadContext*)arg;
    int finished_jobs = 0;
    unsigned long tid = (unsigned long)pthread_self();

    if (ctx->status != THREAD_STARTED) {
        MAW_LOGF(MAW_ERROR, "Thread #%lu not properly started\n", tid);
        return NULL;
    }

    MAW_LOGF(MAW_DEBUG, "Thread #%lu started\n", tid);

    while (true) {
        if (next_metadata_index < 0) {
            break;
        }
        // Take the next metadata_index
        r = pthread_mutex_lock(&lock);
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_mutex_lock: %s\n", strerror(r));
            ctx->status = THREAD_FAILED;
            break;
        }

        next_metadata_index--;
        ctx->metadata_index = next_metadata_index;

        r = pthread_mutex_unlock(&lock);
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_mutex_unlock: %s\n", strerror(r));
            ctx->status = THREAD_FAILED;
            break;
        }
        if (ctx->metadata_index < 0) {
            break;
        }

        // Do work on current metadata_index
        r = maw_update((const Metadata*)(&ctx->metadata[ctx->metadata_index]));

        // On fail, set next_metadata_index to -1, cancelling other threads
        if (r != 0) {
            r = pthread_mutex_lock(&lock);
            if (r != 0) {
                MAW_LOGF(MAW_ERROR, "pthread_mutex_lock: %s\n", strerror(r));
                ctx->status = THREAD_FAILED;
                break;
            }
            next_metadata_index = -1;
            r = pthread_mutex_unlock(&lock);
            if (r != 0) {
                MAW_LOGF(MAW_ERROR, "pthread_mutex_unlock: %s\n", strerror(r));
                ctx->status = THREAD_FAILED;
                break;
            }
            break;
        }
        else {
            finished_jobs++;
        }
    }

    if (ctx->status == THREAD_FAILED) {
        MAW_LOGF(MAW_ERROR, "Thread #%lu: %d job(s) failed\n", tid, 
                                                               finished_jobs);
    }
    else {
        MAW_LOGF(MAW_DEBUG, "Thread #%lu: %d job(s) ok\n", tid, 
                                                           finished_jobs);
    }

    return NULL;
}

int maw_runner_launch(Metadata metadata[], size_t size, size_t thread_count) {
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
        MAW_LOGF(MAW_ERROR, "pthread_mutex_init: %s\n", strerror(r));
        goto end;
    }

    for (size_t i = 0; i < thread_count; i++) {
        thread_ctxs[i].status = THREAD_STARTED;
        r = pthread_create(&threads[i], NULL,
                           maw_runner_thread,
                           (void*)(&thread_ctxs[i]));
        if (r != 0) {
            MAW_LOGF(MAW_ERROR, "pthread_create: %s\n", strerror(r));
            goto end;
        }
    }

end:
    if (thread_ctxs != NULL) {
        for (size_t i = 0; i < thread_count; i++) {
            if (thread_ctxs[i].status == THREAD_UNINITIALIZED)
                continue;

            r = pthread_join(threads[i], NULL);
            if (r != 0)
                MAW_LOGF(MAW_ERROR, "pthread_join: %s\n", strerror(r));

            r = thread_ctxs[i].status;
            if (r == THREAD_FAILED)
                MAW_LOGF(MAW_ERROR, "Thread #%zu failed\n", i);
        }
    }

    free(thread_ctxs);
    free(threads);
    r = pthread_mutex_destroy(&lock);
    if (r != 0) {
        MAW_LOGF(MAW_ERROR, "pthread_mutex_destroy: %s\n", strerror(r));
    }

    return r;
}
