/* See LICENSE file for copyright and license details. */
#ifndef UTIL_H
#define UTIL_H

#define MAX(A, B)        ((A) > (B) ? (A) : (B))
#define MIN(A, B)        ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))


#ifndef NDEBUG
#    define DEBUG_PRINTF(...)                  \
        do {                                   \
            printf("[DWM DBG]: " __VA_ARGS__); \
            fputc('\n', stdout);               \
            fflush(stdout);                    \
        } while (0)
#    define DEBUG_PRINT(VAR, FMT) printf("[DWM DBG]: " #VAR " = " #FMT, VAR)
#    define DEBUG_PRINT_ARR(ARR, SZ, FMT)                         \
        do {                                                      \
            puts("[DWM DBG]: " #ARR ":");                         \
            for (size_t i = 0; i < SZ; i++) {                     \
                printf("\t" #ARR "[%lu] = " FMT "\n", i, ARR[i]); \
            }                                                     \
        } while (0)
#    define IF_DEBUG if (1)
#else  // NDEBUG
#    define DEBUG_PRINTF(...)             (void)0
#    define DEBUG_PRINT(VAR, FMT)         (void)0
#    define DEBUG_PRINT_ARR(ARR, SZ, FMT) (void)0
#    define IF_DEBUG                      if (0)
#endif  // NDEBUG


#define WARN(...)                                       \
    do {                                                \
        fprintf(stderr, "[DWM WARNING]: " __VA_ARGS__); \
        fputc('\n', stderr);                            \
    } while (0)


#include <stddef.h>
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
void die(char const *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
void delay(int delay_for, void (*fn)(void *), void *arg);

#endif  // UTIL_H
