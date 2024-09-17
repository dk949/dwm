/* See LICENSE file for copyright and license details. */
#include "util.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void *ecalloc(size_t nmemb, size_t size) {
    void *p;

    if (!(p = calloc(nmemb, size))) {
        die("calloc:");
    }
    return p;
}

void die(char const *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    fputs("[DWM ERROR]", stderr);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
        fputc(' ', stderr);
        perror(NULL);
    } else {
        fputc('\n', stderr);
    }
    fflush(stderr);

    exit(1);
}

typedef struct DelayPayload {
    void (*fn)(void *);
    void *inner_pl;
    int delay_for;
} DelayPayload;

static void *delay_detached(void *pl) {
    DelayPayload *dpl = (DelayPayload *)pl;
    usleep(dpl->delay_for);
    dpl->fn(dpl->inner_pl);
    free(dpl);
    return NULL;
}

void delay(int delay_for, void (*fn)(void *), void *arg) {
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    DelayPayload *dpl = calloc(1, sizeof(DelayPayload));
    dpl->fn = fn;
    dpl->inner_pl = arg;
    dpl->delay_for = delay_for;
    pthread_create(&tid, &attr, delay_detached, dpl);
}
