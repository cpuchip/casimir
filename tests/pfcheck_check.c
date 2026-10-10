/* CAS 012 rule 7's check, reached by construction: a test build that spoils one numerator coefficient after the
 * numerators are computed and before the check (the first factor's lowest, or the last factor's highest), which must
 * then refuse every answer (NO_ROOM, never the spoiled text); with the spoiling off, the same inputs answer. Not linked
 * with the other tests (it includes src/casim.c itself). */
static int spoil;
/* a coefficient c becomes c + 1 (p += q keeps it reduced) */
#define CASIM_PF_PLANT(pool, used) ((void)(spoil == 1 ? ((pool)[0].p += (pool)[0].q) : \
                                           spoil == 2 ? ((pool)[(used) - 1].p += (pool)[(used) - 1].q) : 0))
#include "../src/casim.c"

#include <stdio.h>
#include <string.h>

static unsigned char arena[65536];
static int failed, checks;

static void is(const char *text, int spoiled, casim_status want)
{
    char out[256];
    casim_status st;
    spoil = spoiled;
    st = casim_partfrac(text, 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, out, sizeof out, arena, sizeof arena);
    checks++;
    if (st != want) {
        failed++;
        printf("FAIL pfcheck_check: partfrac %s%s: status %d \"%s\", want %d\n", text, spoiled ? " (spoiled)" : "", (int)st,
               out, (int)want);
    }
}

int main(void)
{
    static const char *const texts[] = {"(3×X+5)÷((X-3)×(X+4))", "(X^4+1)÷(X^3+1)", "(7×X^2+9×X-1)÷(3×X-2)^4",
                                        "1÷(1-X^4)", "(X^2-1)÷((X-1)×(X+2))", "(3×X^2+2×X+1)÷((X+2)×(X^2+X+1)^2)"};
    for (int i = 0; i < (int)(sizeof texts / sizeof texts[0]); i++) {
        is(texts[i], 0, CASIM_OK);
        is(texts[i], 1, CASIM_NO_ROOM);
        is(texts[i], 2, CASIM_NO_ROOM);
    }
    printf("checks: %d failed: %d\n", checks, failed);
    return failed ? 1 : 0;
}
