/* See LICENSE file for copyright and license details. */
#include "util.hpp"

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

std::optional<std::filesystem::path> getLogDir(void) {
    auto logsubdir = "/dwm/log/";

    char const *xdg_cache_home = getenv("XDG_CACHE_HOME");
    if (xdg_cache_home) {
        auto path = std::filesystem::path(xdg_cache_home) / logsubdir;
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec == std::errc {}) return path;
        WARN("Failed to get XDG_CACHE_HOME (%s): %s", path.c_str(), strerror(errno));
    }

    char const *home = getenv("HOME");
    if (home) {
        auto path = std::filesystem::path(home) / ".cache" / logsubdir;
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec == std::errc {}) return path;
        WARN("Failed to get $HOME/.cache directory (%s): %s", path.c_str(), strerror(errno));
    }
    return {};
}

char const *datetime(void) {
    static char buf[26];

    time_t timer = time(NULL);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&timer));
    return buf;
}
