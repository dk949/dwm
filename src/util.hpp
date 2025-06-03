/* See LICENSE file for copyright and license details. */
#ifndef DWM_UTIL_HPP
#define DWM_UTIL_HPP


#include <filesystem>
#include <optional>
#define MAX(A, B)        ((A) > (B) ? (A) : (B))
#define MIN(A, B)        ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))


#ifndef NDEBUG
#    define DEBUG_PRINTF(...)                             \
        do {                                              \
            fprintf(log_file, "[DWM DBG]: " __VA_ARGS__); \
            fputc('\n', log_file);                        \
            fflush(log_file);                             \
        } while (0)
#    define DEBUG_PRINT(VAR, FMT) fprintf(log_file, "[DWM DBG]: " #VAR " = " #FMT, VAR)
#    define DEBUG_PRINT_ARR(ARR, SZ, FMT)                                    \
        do {                                                                 \
            fputs("[DWM DBG]: " #ARR ":", log_file);                         \
            for (size_t i = 0; i < SZ; i++) {                                \
                fprintf(log_file, "\t" #ARR "[%lu] = " FMT "\n", i, ARR[i]); \
            }                                                                \
        } while (0)
#    define IF_DEBUG if (1)
#    ifdef DWM_TRACE_EVENTS
#        define IF_EVENT_TRACE if (1)
#    else
#        define IF_EVENT_TRACE if (0)
#    endif
#else  // NDEBUG
#    define DEBUG_PRINTF(...)             (void)0
#    define DEBUG_PRINT(VAR, FMT)         (void)0
#    define DEBUG_PRINT_ARR(ARR, SZ, FMT) (void)0
#    define IF_DEBUG                      if (0)
#    define IF_EVENT_TRACE                if (0)
#endif  // NDEBUG

#define LOG(...)                                                  \
    do {                                                          \
        if (!log_file) {                                          \
            fprintf(stderr, "LOG FILE CLOSED USING STDERR!!!\n"); \
            log_file = stderr;                                    \
        }                                                         \
        fprintf(log_file, "%s", datetime());                      \
        fprintf(log_file, "[DWM]: " __VA_ARGS__);                 \
        fputc('\n', log_file);                                    \
        fflush(log_file);                                         \
    } while (0)

#define WARN(...)                                                 \
    do {                                                          \
        if (!log_file) {                                          \
            fprintf(stderr, "LOG FILE CLOSED USING STDERR!!!\n"); \
            log_file = stderr;                                    \
        }                                                         \
        fprintf(log_file, "%s", datetime());                      \
        fprintf(log_file, "[DWM WARNING]: " __VA_ARGS__);         \
        fputc('\n', log_file);                                    \
        fflush(log_file);                                         \
    } while (0)


#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
void die(char const *fmt, ...);
void delay(int delay_for, void (*fn)(void *), void *arg);
char const *datetime(void);


extern FILE *log_file;
/**
 * Get dwm log directory
 *
 * Caller owns returned string
 *
 * If an error occurs, `NULL` is returned
 */
std::optional<std::filesystem::path> getLogDir(void);


/**
 * Like mkdir -p
 *
 * If directory exists does nothing and returns 0
 *
 * If directory does not exist tries to create all directories leading up to it.
 *
 * If any errors occur, returns -1 and sets `errno` appropriatly.
 *
 * If part of the intermediate path is not a directory, sets errno to EEXISTS
 */
int mkdirP(char const *dir_name, int mode);

#endif  // DWM_UTIL_HPP
