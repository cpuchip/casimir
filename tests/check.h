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

/* Whether text holds a bare chain of powers, a^b^c at one depth of parentheses (decision 52: a chain is
   printed (a^b)^c or a^(b^c)). A ^ after another ^ at the same depth, with no lower operator between. */
static inline int bare_chain(const char *text)
{
    int carets[64] = {0}, d = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '(' && d < 63) carets[++d] = 0;
        else if (*p == ')' && d > 0) d--;
        else if (*p == '^' && carets[d]++) return 1;
        else if (*p == '+' || *p == '-' || *p == '=' || *p == ',') carets[d] = 0;
        else if ((unsigned char)*p == 0xC3 && ((unsigned char)p[1] == 0x97 || (unsigned char)p[1] == 0xB7)) carets[d] = 0;
    }
    return 0;
}

static inline int check_done(void)
{
    printf("checks: %d failed: %d\n", check_n, check_failed);
    return check_failed ? 1 : 0;
}

#endif
