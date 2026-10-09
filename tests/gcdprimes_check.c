/* CAS 010's gcd on small primes (11 to 29): one translation unit with src/casim.c, so its prime list can be swapped in.
 * The real 31-bit primes never need these paths (the first prime is lucky and its candidate fits), so each is reached
 * here by construction:
 *   - the sign: gcd(lc f, lc g) = 7 > 11/2, so the first candidate wraps to -(X+1), which divides both; only the
 *     normalization makes it X+1;
 *   - an unlucky prime: X and X-7163 share a root mod 13, 19 and 29 (7163 = 13×19×29), so the primes alternate lucky
 *     and unlucky; X+50 needs two lucky ones combined (50 > 11/2, < 11×17/2), which the minimum degree keeps and a
 *     reset at every change of degree never gets;
 *   - a failing candidate: the first one, X-5 (50 mod 11 = 6, read as -5), must not be printed.
 * Not linked with the other tests (it includes src/casim.c itself). */
#define CASIM_GCD_PRIMES 11u, 13u, 17u, 19u, 23u, 29u
#include "../src/casim.c"

#include <stdio.h>
#include <string.h>

static unsigned char arena[65536];
static int failed, checks;

static void is(const char *f, const char *g, const char *want)
{
    char out[256];
    casim_status st = casim_pgcd(f, 'X' - 'A', g, CASIM_MODEL_35, CASIM_MODEL_35, CASIM_RAD, out, sizeof out, arena, sizeof arena);
    checks++;
    if (st != CASIM_OK || strcmp(out, want) != 0) {
        failed++;
        printf("tests/gcdprimes_check.c: FAIL pgcd %s, %s: %d \"%s\", want \"%s\"\n", f, g, (int)st, out, want);
    }
}

int main(void)
{
    is("(X+1)×(7×X+2)", "(X+1)×(7×X+3)", "X+1");          /* the sign */
    is("(X+50)×X", "(X+50)×(X-7163)", "X+50");             /* unlucky primes between the lucky ones */
    is("(X+50)×(X+1)", "(X+50)×(X+2)", "X+50");            /* two primes combined, no unlucky one */
    is("(X^2+3)×(X+1)", "(X^2+3)×(X-1)", "X^2+3");         /* a quadratic gcd through the small primes */
    printf("checks: %d failed: %d\n", checks, failed);
    return failed != 0;
}
