/* CAS 018's antidifference check, reached by construction: a test build where F is spoiled (doubled) before it is
 * checked, so F(k+1) - F(k) is no longer q(k) and the check must refuse. Every answer is then the one sum gives as
 * built (no group reached the spoiled F) or NO_CLOSED_FORM, never a spoiled sum. As built, no input reaches the check
 * (the antidifference is solved exactly), so a mutant that skips it is seen only here. Not linked with the other tests
 * (it includes src/casim.c itself). */
static int mode;                    /* 0 as built, 1 F spoiled */
#define CASIM_SM_PLANT(c, F) (mode == 1 ? mul2(c, F, small(c, C_2)) : (F))
#include "../src/casim.c"

#include <stdio.h>
#include <string.h>

static unsigned char arena[65536];

int main(void)
{
    /* polynomial groups, rational and irrational ratios (the W path), to N and to ∞ */
    static const char *const sums[][3] = {{"K", "1", "N"}, {"K^3", "1", "N"}, {"2^K", "0", "N"}, {"K×2^K", "1", "N"},
                                          {"EXP(K)", "0", "N"}, {"2^(K÷2)", "0", "N"}, {"(1÷2)^K", "0", "\xe2\x88\x9e"},
                                          {"K÷2^K", "1", "\xe2\x88\x9e"}, {"K", "M", "N"}};
    int failed = 0, checks = 0, refused = 0;
    for (int i = 0; i < (int)(sizeof sums / sizeof sums[0]); i++) {
        char built[256], spoiled[256];
        casim_status s0, s1;
        mode = 0;
        s0 = casim_sum(sums[i][0], 'K' - 'A', sums[i][1], sums[i][2], CASIM_MODEL_35, CASIM_RAD, built, sizeof built,
                       arena, sizeof arena);
        mode = 1;
        s1 = casim_sum(sums[i][0], 'K' - 'A', sums[i][1], sums[i][2], CASIM_MODEL_35, CASIM_RAD, spoiled, sizeof spoiled,
                       arena, sizeof arena);
        checks += 2;
        if (s0 != CASIM_OK) {
            failed++;
            printf("FAIL smcheck_check: sum %s as built: status %d\n", sums[i][0], (int)s0);
        }
        if (s1 == CASIM_OK && strcmp(built, spoiled) != 0) {
            failed++;
            printf("FAIL smcheck_check: sum %s with F spoiled: \"%s\" printed, as built \"%s\"\n", sums[i][0], spoiled,
                   built);
        }
        refused += s1 == CASIM_NO_CLOSED_FORM;
    }
    checks++;
    if (refused != (int)(sizeof sums / sizeof sums[0])) {
        failed++;
        printf("FAIL smcheck_check: %d of %d spoiled sums refused\n", refused, (int)(sizeof sums / sizeof sums[0]));
    }
    printf("smcheck: %d sums, each refused with F spoiled\nchecks: %d failed: %d\n", (int)(sizeof sums / sizeof sums[0]),
           checks, failed);
    return failed != 0;
}
