/* CAS 017's substitution check, reached by construction: a test build where G is spoiled (doubled) before it is
 * integrated, so G(u)×u' is no longer the term and the check must refuse that substitution. Every answer is then either
 * the one integrate gives as built (another rule reached it) or a refusal, never the spoiled one. As built, no input
 * reaches the check (G(u)×u' rebuilds the term exactly when u was replaced exactly), so a mutant that skips it is seen
 * only here. Not linked with the other tests (it includes src/casim.c itself). */
static int mode;                    /* 0 as built, 1 G spoiled */
#define CASIM_IB_PLANT(c, g) (mode == 1 ? mul2(c, g, small(c, C_2)) : (g))
#include "../src/casim.c"

#include <stdio.h>
#include <string.h>

static unsigned char arena[65536];
static int failed, checks;

int main(void)
{
    static const char *const subs[] = {"X×EXP(X^2)", "SIN(X)^2×COS(X)", "LN(X)÷X", "X×SQRT(X^2+1)", "X^3×SQRT(X^2+1)",
                                       "EXP(X)÷(1+EXP(X))", "ATAN(X)÷(1+X^2)", "COS(X)×EXP(SIN(X))", "X^2×(X^3+1)^5"};
    int refused = 0;
    for (int i = 0; i < (int)(sizeof subs / sizeof subs[0]); i++) {
        char built[256], spoiled[256];
        casim_status s0, s1;
        mode = 0;
        s0 = casim_integrate(subs[i], 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, built, sizeof built, arena, sizeof arena);
        mode = 1;
        s1 = casim_integrate(subs[i], 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, spoiled, sizeof spoiled, arena, sizeof arena);
        checks += 2;
        if (s0 != CASIM_OK) {
            failed++;
            printf("FAIL ibcheck_check: integrate %s as built: status %d\n", subs[i], (int)s0);
        }
        if (s1 == CASIM_OK && strcmp(built, spoiled) != 0) {
            failed++;
            printf("FAIL ibcheck_check: integrate %s with G spoiled: \"%s\" printed, as built \"%s\"\n", subs[i], spoiled,
                   built);
        }
        refused += s1 == CASIM_NOT_INTEGRABLE;
    }
    checks++;
    if (refused == 0) {                 /* the plant reached nothing: the test would test nothing */
        failed++;
        printf("FAIL ibcheck_check: no substitution was refused with G spoiled\n");
    }
    printf("ibcheck_check: %d substitutions, %d refused with G spoiled\n", (int)(sizeof subs / sizeof subs[0]), refused);
    printf("checks: %d failed: %d\n", checks, failed);
    return failed != 0;
}
