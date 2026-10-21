/* CAS 021's series route, forced (built with CASIM_SER=2: the route alone, on every taylor and limit line of the golden
 * corpus), judged against the corpus's answer by the core (work/021-series.md, the oracle). Where both answer, the
 * texts are the same or equal in value: a Taylor polynomial at n + 1 points X = A + j÷(4(n + 1)), a limit as its
 * constant, each with K = 37÷100, to 1E-20. A status that contradicts the other's (a value against UNDEFINED, ∞ against
 * -∞) fails; a refusal on either side is counted, never compared. The comparison's control: a corpus answer with one
 * coefficient changed must be refused. */
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define LINE 8192
#define OUT 65536

static _Alignas(8) unsigned char arena[1 << 19];
static char out[OUT];

static char *unescape(char *s)
{
    char *w = s;
    if (strcmp(s, "\\N") == 0) return NULL;
    for (char *r = s; *r; r++) {
        if (*r == '\\' && r[1]) {
            r++;
            *w++ = *r == 't' ? '\t' : *r == 'n' ? '\n' : *r;
        } else *w++ = *r;
    }
    *w = '\0';
    return s;
}

static int answered(const char *st) { return !strcmp(st, "OK") || !strcmp(st, "DECIMAL_FORM"); }

/* 1 when a and b agree as functions of X near the point (n + 1 points; a limit: n < 0, the constant alone), 0 when they
   differ, -1 when the core can't value one */
static int agree(int angle, const char *point, int n, const char *a, const char *b)
{
    ce_angle(angle);
    ce_set('K', "0.37");
    for (int j = 1; j <= (n < 0 ? 1 : n + 1); j++) {
        abn p, h, d, va, vb;
        char t[32];
        if (n >= 0) {
            if (ce_eval(point, &p) != ABN_OK) return -1;
            snprintf(t, sizeof t, "%d", j);
            h = ce_num(t);
            snprintf(t, sizeof t, "%d", 4 * (n + 1));
            d = ce_num(t);
            if (abn_div(&h, &h, &d) != ABN_OK || abn_add(&p, &p, &h) != ABN_OK) return -1;
            ce_set_abn('X', p);
        }
        if (ce_eval(a, &va) != ABN_OK || ce_eval(b, &vb) != ABN_OK) return -1;
        if (!ce_close(&va, &vb, -20)) return 0;
    }
    return 1;
}

/* rows the corpus can't hold (its answers are today's routes'): what the route alone must give, the truth or a refusal
   (UNDECIDED, NO_ROOM); "" is a refusal only, the rule's give-up, and "=" the truth only (the route must answer: the
   precision's retries). Z is 0 exactly, by an identity, with terms near 10^25: rule 8's zero test must not take it, or
   Z×10^30, for a number. */
#define Z "(10^25×SIN(1)^2+10^25×COS(1)^2-10^25)"
#define N42(s) "((((((((((((((((((((((((((((((((((((((((((" s
#define P42 "+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)" \
            "+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)+1)"
static const struct {
    casim_op op;
    const char *eqn, *point, *n_or_side, *want;
} ROWS[] = {
    {CASIM_LIMIT, "(1÷X^2-1÷SIN(X)^2)+(" Z "×10^20)÷X", "0", "", "-1÷3"},
    {CASIM_LIMIT, "(1÷X^2-1÷SIN(X)^2)+(" Z "×10^20)÷X^2", "0", "+", "-1÷3"},
    {CASIM_LIMIT, "(1÷X^2-1÷SIN(X)^2)+(" Z "×10^30)÷X", "0", "", "-1÷3"},
    {CASIM_TAYLOR, "ABS(" Z "×10^30+X)×EXP(SIN(X))", "0", "9", "UNDEFINED"},
    {CASIM_TAYLOR, "1÷(" Z "×10^20+X)", "0", "1", "UNDEFINED"},    /* a divisor's leading coefficient undecided */
    {CASIM_LIMIT, "SQRT(X)", "0", "+", "0"},                      /* t^(1÷2): Puiseux, no series */
    {CASIM_LIMIT, "(-2-X)^(1÷3)", "0", "", ""},                   /* a fractional power of a negative base */
    {CASIM_TAYLOR, N42("X÷X") P42, "0", "1", "UNDEFINED"},        /* rule 7 ran out (the depth guard): no series */
    {CASIM_LIMIT, "(1÷X^2-1÷SIN(X)^2)×K×ABS(X)÷X", "0", "", ""},  /* the sides -K÷3 and K÷3: apart unless K is 0 */
    {CASIM_TAYLOR, "EXP(SIN(X)-X+X^3÷6-X^5÷120+X^7÷5040)", "0", "1", "=1"},       /* 4 terms cancel: P = 4 is short */
    {CASIM_LIMIT, "(EXP(X)-1-X-X^2÷2-X^3÷6-X^4÷24-X^5÷120-X^6÷720-X^7÷5040)÷X^8", "0", "", "=1÷40320"},
};

int main(void)
{
    static const char *const STATUSES[] = {"OK", "SYNTAX", "NOT_DIFFERENTIABLE", "TOO_LONG", "BAD_ARG", "NO_ROOM",
                                           "NOT_POLYNOMIAL", "NOT_NUMERIC", "NOT_INTEGRABLE", "PARTIAL", "UNDEFINED",
                                           "IMPROPER", "UNDECIDED", "NO_CLOSED_FORM", "DECIMAL_FORM"};
    static char line[LINE];
    long text[2] = {0}, value[2] = {0}, only[2] = {0}, refused[2] = {0}, lines[2] = {0};
    FILE *f = fopen("tests/golden/corpus.txt", "r");
    ce_init();
    CHECK(f != NULL, "tests/golden/corpus.txt");
    if (!f) return check_done();
    /* the control: a changed coefficient is seen */
    CHECK(agree(CASIM_RAD, "0", 3, "X-X^3÷6", "X-X^3÷6") == 1 && agree(CASIM_RAD, "0", 3, "X-X^3÷6", "X-X^3÷5") == 0 &&
              agree(CASIM_RAD, "0", -1, "-1÷3", "-1÷3+1÷10^15") == 0,
          "the comparison's control");
    while (fgets(line, sizeof line, f)) {
        char *fl[12] = {0};
        int k = 0, lim, st, n, a;
        const char *got;
        size_t len = strlen(line);
        casim_req rq;
        if (len && line[len - 1] == '\n') line[--len] = '\0';
        if (!len || line[0] == '#') continue;
        for (char *p = line; k < 12 && p; k++) {
            fl[k] = p;
            if ((p = strchr(p, '\t'))) *p++ = '\0';
        }
        if (k != 12 || (strcmp(fl[0], "taylor") && strcmp(fl[0], "limit"))) continue;
        lim = !strcmp(fl[0], "limit");
        lines[lim]++;
        rq = (casim_req){lim ? CASIM_LIMIT : CASIM_TAYLOR, unescape(fl[7]), atoi(fl[1]), atoi(fl[2]), unescape(fl[8]),
                         atoi(fl[4]), atoi(fl[3]), unescape(fl[9])};
        st = (int)casim_run(&rq, out, (size_t)strtoul(fl[6], NULL, 10), arena, (size_t)strtoul(fl[5], NULL, 10));
        got = STATUSES[st];
        unescape(fl[11]);
        n = lim ? -1 : atoi(rq.repl2);
        if (!answered(got) && strcmp(got, "UNDEFINED")) {
            refused[lim]++;                     /* the route refused: nothing to compare */
            continue;
        }
        if (!answered(fl[10]) && strcmp(fl[10], "UNDEFINED")) {
            only[lim]++;                        /* the route answers where the corpus refuses */
            continue;
        }
        if (strcmp(got, "UNDEFINED") == 0 || strcmp(fl[10], "UNDEFINED") == 0) {
            CHECK(strcmp(got, fl[10]) == 0, "%s %s at %s: forced %s %s, the corpus %s %s", fl[0], rq.eqn, rq.repl, got,
                  out, fl[10], fl[11]);
            continue;
        }
        if (strcmp(out, fl[11]) == 0) {
            text[lim]++;
            continue;
        }
        a = strstr(out, "\xe2\x88\x9e") || strstr(fl[11], "\xe2\x88\x9e") ? 0 :
            agree(atoi(fl[3]), lim ? "0" : rq.repl, n, out, fl[11]);
        CHECK(a == 1, "%s %s at %s (order %s, arena %s): forced %s, the corpus %s", fl[0], rq.eqn, rq.repl, rq.repl2,
              fl[5], out, fl[11]);
        value[lim] += a == 1;
    }
    fclose(f);
    for (size_t i = 0; i < sizeof ROWS / sizeof ROWS[0]; i++) {
        casim_req rq = {ROWS[i].op, ROWS[i].eqn, CASIM_MODEL_35, 'X' - 'A', ROWS[i].point, CASIM_MODEL_35, CASIM_RAD,
                        ROWS[i].n_or_side};
        int st = (int)casim_run(&rq, out, sizeof out, arena, 65536);
        const char *want = ROWS[i].want + (*ROWS[i].want == '=');
        int refused = *ROWS[i].want != '=' && (st == CASIM_UNDECIDED || st == CASIM_NO_ROOM);
        CHECK(refused || (!strcmp(want, "UNDEFINED") ? st == CASIM_UNDEFINED
                                                     : *want && st == CASIM_OK && !strcmp(out, want)),
              "%s at %s %s: %s %s, the truth %s or a refusal", rq.eqn, rq.repl, rq.repl2, STATUSES[st], out,
              ROWS[i].want);
    }
    printf("series forced: taylor %ld lines, %ld the same text, %ld equal in value, %ld answered only by the route, "
           "%ld refused by it; limit %ld lines, %ld, %ld, %ld, %ld\n",
           lines[0], text[0], value[0], only[0], refused[0], lines[1], text[1], value[1], only[1], refused[1]);
    return check_done();
}
