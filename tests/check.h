/* check: the assertions every casim test makes. A test ends with `return check_done();`, which
 * prints the line scripts/run-tests.sh reads; a test that made no checks is refused there. */
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int check_n, check_failed;

#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        check_n++;                                                              \
        if (!(cond)) {                                                          \
            check_failed++;                                                     \
            fprintf(stderr, "%s:%d: FAIL %s: ", __FILE__, __LINE__, #cond);     \
            fprintf(stderr, __VA_ARGS__);                                       \
            fputc('\n', stderr);                                                \
        }                                                                       \
    } while (0)

static inline int check_done(void)
{
    printf("checks: %d failed: %d\n", check_n, check_failed);
    return check_failed ? 1 : 0;
}

#endif
