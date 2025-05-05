/* See LICENSE file for copyright and license details. */
#include "util.h"

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

void *ecalloc(size_t nmemb, size_t size) {
    void *p;

    if (!(p = calloc(nmemb, size))) {
        die("calloc:");
    }
    return p;
}

void die(char const *fmt, ...) {
    FILE *file = log_file ? log_file : stderr;
    va_list ap;

    va_start(ap, fmt);
    fputs("[DWM ERROR]: ", file);
    vfprintf(file, fmt, ap);
    va_end(ap);

    if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
        fputc(' ', file);
        fputs(strerror(errno), file);
    } else {
        fputc('\n', file);
    }
    if (!log_file) fputs("NOTE: logfile unavailable", file);
    fflush(file);

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

char *buildStringV(char const *first, va_list args) {
    if (first == NULL) return NULL;
    va_list length_count;
    va_copy(length_count, args);
    size_t total_length = 0;
    for (char const *it = first; it != NULL; it = va_arg(length_count, char const *))
        total_length += strlen(it);
    va_end(length_count);

    char *out = malloc(total_length + 1);
    out[0] = 0;
    for (char const *it = first; it != NULL; it = va_arg(args, char const *))
        strcat(out, it);

    return out;
}

char *buildString(char const *first, ...) {
    va_list args;
    va_start(args, first);
    char *out = buildStringV(first, args);
    va_end(args);
    return out;
}

char *buildStringDealloc(char *first, ...) {
    va_list args;
    va_start(args, first);
    char *out = buildStringDeallocV(first, args);
    va_end(args);
    return out;
}

char *buildStringDeallocV(char *first, va_list args) {
    char *out = buildStringV(first, args);
    free(first);
    return out;
}

int mkdirP(char const *dir_name, int mode) {
    struct stat st = {0};
    if (stat(dir_name, &st) != -1) {
        if (S_ISDIR(st.st_mode)) return 0;
        errno = EEXIST;
        return -1;
    }

    char const *parent = dirname(strdupa(dir_name));
    if (mkdirP(parent, mode)) return -1;
    return mkdir(dir_name, mode);
}

char *getLogDir(void) {
    char const *logsubdir = "/dwm/log/";

    char const *xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home) {
        char *path = buildString(xdg_cache_home, logsubdir, NULL);
        if (!mkdirP(path, 0700)) return path;
        WARN("Failed to get XDG_CACHE_HOME (%s): %s", path, strerror(errno));
    }

    char const *home = getenv("HOME");
    if (home) {
        char *path = buildString(home, "/.cache", logsubdir, NULL);
        if (!mkdirP(path, 0700)) return path;
        WARN("Failed to get $HOME/.cache directory (%s): %s", path, strerror(errno));
    }
    return NULL;
}

char const *datetime(void) {
    static char buf[26];

    time_t timer = time(NULL);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&timer));
    return buf;
}
