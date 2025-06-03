/* See LICENSE file for copyright and license details. */
#include "util.hpp"

#include "log.hpp"

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct DelayPayload {
    void (*fn)(void *);
    void *inner_pl;
    int delay_for;
};

static void *delay_detached(void *pl) {
    DelayPayload *dpl = (DelayPayload *)pl;
    usleep((unsigned)dpl->delay_for);
    dpl->fn(dpl->inner_pl);
    delete dpl;
    return NULL;
}

void delay(int delay_for, void (*fn)(void *), void *arg) {
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t tid;
    DelayPayload *dpl = new DelayPayload {};
    dpl->fn = fn;
    dpl->inner_pl = arg;
    dpl->delay_for = delay_for;
    pthread_create(&tid, &attr, delay_detached, dpl);
}

int mkdirP(char const *dir_name, int mode) {
    struct stat st {};
    if (stat(dir_name, &st) != -1) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = EEXIST;
        return -1;
    }

    char const *parent = dirname(strdupa(dir_name));
    if (mkdirP(parent, mode)) return -1;
    return mkdir(dir_name, (unsigned)mode);
}

