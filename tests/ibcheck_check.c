/* CAS 017's substitution check, reached by construction: a test build where G is spoiled (doubled) before it is
 * integrated, so G(u)×u' is no longer the term and the check must refuse that substitution. Every answer is then either
 * the one integrate gives as built (another rule reached it) or a refusal, never the spoiled one. As built, no input
 * reaches the check (G(u)×u' rebuilds the term exactly when u was replaced exactly), so a mutant that skips it is seen
 * only here. Not linked with the other tests (it includes src/casim.c itself). */
static int mode;                    /* 0 as built, 1 G spoiled, 2 G spoiled in the steps' replay only */
#define CASIM_IB_PLANT(c, g) (mode == 1 ? mul2(c, g, small(c, C_2)) : (g))
#define CASIM_IB_REPLAY_PLANT(c, g) (mode == 2 ? mul2(c, g, small(c, C_2)) : (g))
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
    /* the steps (work/017 §Steps): with G spoiled no substitution answers, so no page may show one. The plan reads a
       substitution's note, which a refused try leaves; it must check G(u)×u' itself, or it shows the spoiled G */
    {
        /* each answers by parts with G spoiled, after a substitution by back-substitution (u = X^2) was tried */
        static const char *const both[] = {"LN(X^2)", "X×LN(X^2)", "X^3×LN(X^2)"};
        int walked = 0;
        mode = 1;
        for (int i = 0; i < (int)(sizeof both / sizeof both[0]); i++) {
            casim_req rq = {.op = CASIM_INTEGRATE, .eqn = both[i], .model = CASIM_MODEL_35, .var = 'X' - 'A',
                            .angle = CASIM_RAD};
            casim_step_info info;
            char out[512];
            if (casim_step(&rq, -1, 0, &info, out, sizeof out, arena, sizeof arena) != CASIM_OK) continue;
            walked++;
            for (int k = 0; k < info.count; k++) {
                checks++;
                if (casim_step(&rq, k, 0, &info, out, sizeof out, arena, sizeof arena) == CASIM_OK &&
                    info.rule == CASIM_RULE_SUBSTITUTION) {
                    failed++;
                    printf("FAIL ibcheck_check: steps of %s with G spoiled, page %d: %s\n", both[i], k, out);
                }
            }
        }
        checks++;
        if (walked != (int)(sizeof both / sizeof both[0])) {
            failed++;
            printf("FAIL ibcheck_check: %d of the walks answered with G spoiled\n", walked);
        }
    }
    /* the plan's own check of a noted substitution (ib_choose): a refused try leaves no note (ib_unnote), so G spoiled in
       the search never reaches it. Spoiled in the replay only, the search still answers by substitution and the plan
       must refuse to show it: rule 8's one page, its answer integrate's, where as built the walk has the U pages */
    {
        static const char *const sub[] = {"X×EXP(X^2)", "LN(X)÷X", "15×X^2÷(5×X^3+2)", "X^3×SQRT(X^2+1)"};
        for (int i = 0; i < (int)(sizeof sub / sizeof sub[0]); i++) {
            casim_req rq = {.op = CASIM_INTEGRATE, .eqn = sub[i], .model = CASIM_MODEL_35, .var = 'X' - 'A',
                            .angle = CASIM_RAD};
            casim_step_info info;
            char built[512], spoiled[512];
            int n0 = 0, n2 = 0;
            casim_status s0, s2;
            mode = 0;
            s0 = casim_step(&rq, -1, 0, &info, built, sizeof built, arena, sizeof arena);
            if (s0 == CASIM_OK) n0 = info.count, s0 = casim_step(&rq, n0 - 1, 0, &info, built, sizeof built, arena,
                                                                   sizeof arena);
            mode = 2;
            s2 = casim_step(&rq, -1, 0, &info, spoiled, sizeof spoiled, arena, sizeof arena);
            if (s2 == CASIM_OK) n2 = info.count, s2 = casim_step(&rq, n2 - 1, 0, &info, spoiled, sizeof spoiled, arena,
                                                                   sizeof arena);
            checks++;
            if (s0 != CASIM_OK || s2 != CASIM_OK || n0 < 2 || n2 != 1 || strcmp(built, spoiled) != 0) {
                failed++;
                printf("FAIL ibcheck_check: steps of %s, G spoiled in the replay: %d pages (as built %d), \"%s\" (as "
                       "built \"%s\")\n", sub[i], n2, n0, spoiled, built);
            }
        }
        mode = 0;
    }
    /* a refused substitution's note is dropped (ib_unnote): it points into the arena ib_fail gave back, and the plan
       read freed nodes from it (the cold review, under ASan). LN(X^2+1) and LN(X^2+4) try u = X^2+1 (+4), refuse it,
       and answer by parts: no note may be left */
    {
        static const char *const parts[] = {"LN(X^2+1)", "LN(X^2+4)", "LN(2×X^2+3)"};
        mode = 0;
        for (int i = 0; i < (int)(sizeof parts / sizeof parts[0]); i++) {
            ctx c;
            ib_t B = {0, false, IBH_NONE, {0}};
            ref n, t, F;
            start(&c, parts[i], CASIM_RAD, arena, sizeof arena);
            c.opaque = true, c.var = 'X' - 'A', c.sub_var = -1;
            n = parse(&c);
            t = ib_canon(&c, n);
            c.own_text = true;
            F = ib(&c, &B, t, 'X' - 'A', -1);
            checks++;
            if (c.st != CASIM_OK || F == SPARE || B.how == IBH_SUB) {
                failed++;
                printf("FAIL ibcheck_check: %s: status %d, a substitution's note %s after parts answered\n", parts[i],
                       (int)c.st, B.how == IBH_SUB ? "left" : "not left");
            }
        }
    }
    /* S2.6's discriminant, exact or no answer: a product past 63 bits keeps the ABS (the cold review's overflow) */
    {
        static const int64_t q[][4] = {{1, 2, 2, 1}, {1, 2, 1, 0}, {1, 3000000000, 1, 0}, {3000000000, 1, 3000000000, 0},
                                       {3037000500, 0, 3037000500, 0}, {-3000000000, 1, -3000000000, 0},
                                       {2500000000, 1, 2000000000, 0}};   /* 4ac wraps to a positive */
        for (int i = 0; i < (int)(sizeof q / sizeof q[0]); i++) {
            checks++;
            if (ib_noroot(q[i][0], q[i][1], q[i][2]) != (q[i][3] != 0)) {
                failed++;
                printf("FAIL ibcheck_check: ib_noroot(%lld, %lld, %lld) is %d\n", (long long)q[i][0], (long long)q[i][1],
                       (long long)q[i][2], (int)ib_noroot(q[i][0], q[i][1], q[i][2]));
            }
        }
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
