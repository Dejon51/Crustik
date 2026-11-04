#ifndef THREADS_H
#define THREADS_H

#define THREAD_STACK_SIZE (16u * 1024u * 1024u)

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

typedef HANDLE thread_t;

#define THREAD_RET    DWORD WINAPI
#define THREAD_RETURN return 0

static inline int thread_create(thread_t *t, DWORD(WINAPI *fn)(LPVOID), void *arg)
{
    *t = CreateThread(NULL, (SIZE_T)THREAD_STACK_SIZE, fn, arg,
                      STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

    if (!*t) 
        *t = CreateThread(NULL, (SIZE_T)THREAD_STACK_SIZE, fn, arg, 0, NULL);

    return *t ? 0 : -1;
}

static inline void thread_join(thread_t t)
{
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
}

#else

#include <pthread.h>
#include <limits.h>

typedef pthread_t thread_t;

#define THREAD_RET    void *
#define THREAD_RETURN return NULL

static inline int thread_create(thread_t *t, void *(*fn)(void *), void *arg)
{
    pthread_attr_t attr;
    size_t want = THREAD_STACK_SIZE;

#ifdef PTHREAD_STACK_MIN
    if (want < (size_t)PTHREAD_STACK_MIN)
        want = (size_t)PTHREAD_STACK_MIN;
#endif

    if (pthread_attr_init(&attr) != 0)
        return pthread_create(t, NULL, fn, arg);

    if (pthread_attr_setstacksize(&attr, want) != 0)
    {
        pthread_attr_destroy(&attr);
        return pthread_create(t, NULL, fn, arg);
    }

    int rc = pthread_create(t, &attr, fn, arg);
    pthread_attr_destroy(&attr);

    if (rc != 0)
        rc = pthread_create(t, NULL, fn, arg);

    return rc;
}

static inline void thread_join(thread_t t)
{
    pthread_join(t, NULL);
}

#endif

#endif
