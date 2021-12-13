/* See LICENSE file for copyright and license details. */
#ifndef UTIL_H
#define UTIL_H

#define MAX(A, B)             ((A) > (B) ? (A) : (B))
#define MIN(A, B)             ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B)      ((A) <= (X) && (X) <= (B))
#define DEBUG_PRINT(VAR, FMT) printf("[DBG]: " #VAR " = " #FMT "\n", VAR)
#define DEBUG_PRINT_ARR(ARR, SZ, FMT)                     \
    puts("[DBG]: " #ARR ":");                             \
    for (size_t i = 0; i < SZ; i++) {                     \
        printf("\t" #ARR "[%lu] = " FMT "\n", i, ARR[i]); \
    }

#include <stddef.h>
void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);

#endif  // UTIL_H
