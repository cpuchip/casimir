/* CAS 013's check and refusals, reached by construction: a test build where each term's antiderivative can be spoiled
 * (doubled) before its check, which must then refuse it (NOT_INTEGRABLE, never the spoiled text); and where the check
 * can be taken away, so rule 5's refused forms (D > 0, a quadratic to a power >= 2, a cubic) are seen to be refused by
 * rule 5 itself, not only by a check that fails. With neither, the same answers print. Not linked with the other
 * tests (it includes src/casim.c itself). */
static int mode;                    /* 0 as built, 1 a term spoiled, 2 no check */
#define CASIM_IR_PLANT(c, F) (mode == 1 ? mul2(c, F, small(c, C_2)) : (F))
#define CASIM_IR_CLOSES(c, F, f) (mode == 2 || closes(c, F, f))
#include "../src/casim.c"

#include <stdio.h>
#include <string.h>

static unsigned char arena[65536];
static int failed, checks;

static void is(const char *text, int m, casim_status want)
{
    char out[256];
    casim_status st;
    mode = m;
    st = casim_integrate(text, 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, out, sizeof out, arena, sizeof arena);
    checks++;
    if (st != want) {
        failed++;
        printf("FAIL ircheck_check: integrate %s (mode %d): status %d \"%s\", want %d\n", text, m, (int)st, out, (int)want);
    }
}

int main(void)
{
    static const char *const answers[] = {"(5×X+1)÷(X^2+X-2)", "1÷(1-X^4)", "1÷(X^2+X+1)", "(X^2+1)÷(X-1)^3",
                                          "(X^4+1)÷(X^3+1)", "1÷(4×X^2+9)"};
    static const char *const refused[] = {"1÷(X^2-2)", "1÷(X^2+1)^2", "X÷(X^2+X+1)^2", "1÷(X^3-2)", "(X+1)÷(X^2-3×X+1)"};
    for (int i = 0; i < (int)(sizeof answers / sizeof answers[0]); i++) {
        is(answers[i], 0, CASIM_OK);
        is(answers[i], 1, CASIM_NOT_INTEGRABLE);            /* a spoiled term: the check refuses */
        is(answers[i], 2, CASIM_OK);
    }
    for (int i = 0; i < (int)(sizeof refused / sizeof refused[0]); i++) {
        is(refused[i], 0, CASIM_NOT_INTEGRABLE);
        is(refused[i], 2, CASIM_NOT_INTEGRABLE);            /* refused by rule 5, not by a check that fails */
    }
    printf("checks: %d failed: %d\n", checks, failed);
    return failed ? 1 : 0;
}
