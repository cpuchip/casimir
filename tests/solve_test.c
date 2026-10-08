/* Unit CAS 005: exact polynomial solving (work/005-solve.md, with abacus's #4841 and #4843). Each status and
 * each root form as exact text; the refusals; the STU stamp; the edges abacus named (a true factor at the
 * coefficient bound, near-coincident roots, an irreducible quartic with real roots); and seeded polynomials
 * built from known factors, each printed root evaluated on the core and matched by value to the factors it
 * was built from, with its multiplicity, and the status and the degree left checked against the
 * construction. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static casim_status solve(const char *eqn, int var, int model, char *out)
{
    casim_req rq = {CASIM_SOLVE, eqn, model, var, NULL, 0, CASIM_RAD};
    return casim_run(&rq, out, OUT, arena, sizeof arena);
}

static void is(const char *eqn, const char *want)
{
    char out[OUT];
    casim_status st = solve(eqn, 'X' - 'A', CASIM_MODEL_35, out);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "solve %s: %d \"%s\", want \"%s\"", eqn, (int)st, out, want);
}

static void refused(const char *eqn, casim_status want)
{
    char out[OUT];
    casim_status st = solve(eqn, 'X' - 'A', CASIM_MODEL_35, out);
    CHECK(st == want && out[0] == '\0', "solve %s: %d \"%s\", want status %d", eqn, (int)st, out, (int)want);
}

static void exact(void)
{
    char out[OUT];
    /* rational roots, ascending, each once */
    is("X^2-5×X+6", "SOLVED: X=2; X=3");
    is("X^2-5×X+6=0", "SOLVED: X=2; X=3");
    is("X^2=5×X-6", "SOLVED: X=2; X=3");
    is("2×X^2-5×X-3", "SOLVED: X=-1÷2; X=3");
    is("X", "SOLVED: X=0");
    is("X^3-X", "SOLVED: X=-1; X=0; X=1");
    is("0.5×X-0.25", "SOLVED: X=1÷2");                    /* decimals cleared exactly */
    is("X÷3-2÷7", "SOLVED: X=6÷7");
    /* multiplicities */
    is("(X-1)^2×(X+2)", "SOLVED: X=-2; X=1 (MULT 2)");
    is("X^3", "SOLVED: X=0 (MULT 3)");
    is("(X^2-2)^2", "SOLVED: X=-SQRT(2) (MULT 2); X=SQRT(2) (MULT 2)");
    /* quadratic irrationals, (a±b×SQRT(r))÷d in lowest terms */
    is("X^2-2", "SOLVED: X=-SQRT(2); X=SQRT(2)");
    is("2×X^2-1", "SOLVED: X=-SQRT(2)÷2; X=SQRT(2)÷2");
    is("X^2-X-3", "SOLVED: X=(1-SQRT(13))÷2; X=(1+SQRT(13))÷2");
    is("X^2-2×X-1", "SOLVED: X=1-SQRT(2); X=1+SQRT(2)");
    is("X^2-6×X+1", "SOLVED: X=3-2×SQRT(2); X=3+2×SQRT(2)");
    is("X^4-5×X^2+6", "SOLVED: X=-SQRT(3); X=-SQRT(2); X=SQRT(2); X=SQRT(3)");      /* (X^2-2)(X^2-3) */
    is("(X-1)×(X^2-2×X-1)", "SOLVED: X=1-SQRT(2); X=1; X=1+SQRT(2)");
    /* the statuses, by the factorization over Q */
    is("X^2+1", "NO REAL ROOTS");
    is("(X^2+1)×(X^2+X+1)", "NO REAL ROOTS");
    is("(X-1)×(X^2+1)", "SOLVED: X=1");                   /* a quadratic without real roots adds nothing */
    is("X^3-2", "PARTIAL, DEGREE 3 LEFT");
    is("(X-1)×(X^3-2)", "PARTIAL, DEGREE 3 LEFT: X=1");
    is("(X^3-2)^2", "PARTIAL, DEGREE 6 LEFT");
    is("X^4-10×X^2+1", "PARTIAL, DEGREE 4 LEFT");         /* irreducible, with real roots ±√2±√3 */
    is("3=0", "NO SOLUTION");
    is("X=X+1", "NO SOLUTION");
    is("X-X", "EVERY X");
    is("0", "EVERY X");
    /* another variable */
    CHECK(solve("T^2-4", 'T' - 'A', CASIM_MODEL_35, out) == CASIM_OK && strcmp(out, "SOLVED: T=-2; T=2") == 0,
          "solve T^2-4 in T: \"%s\"", out);
    /* the stamp (rule 7): X^3^2-1 is X^9-1 read as STU, X^6-1 read as 35 */
    CHECK(solve("X^3^2-1", 'X' - 'A', CASIM_MODEL_STU, out) == CASIM_OK && strcmp(out, "PARTIAL, DEGREE 6 LEFT: X=1") == 0,
          "solve X^3^2-1 (STU): \"%s\"", out);
    CHECK(solve("X^3^2-1", 'X' - 'A', CASIM_MODEL_35, out) == CASIM_OK && strcmp(out, "SOLVED: X=-1; X=1") == 0,
          "solve X^3^2-1 (35): \"%s\"", out);
    /* the refusals */
    refused("SIN(X)-1", CASIM_NOT_POLYNOMIAL);
    refused("1÷X-2", CASIM_NOT_POLYNOMIAL);
    refused("SQRT(X)-2", CASIM_NOT_POLYNOMIAL);
    refused("X^0.5-2", CASIM_NOT_POLYNOMIAL);
    refused("X^(-1)+1", CASIM_NOT_POLYNOMIAL);
    refused("X^Y-1", CASIM_NOT_POLYNOMIAL);
    refused("Y×X-1", CASIM_NOT_NUMERIC);
    refused("π×X-1", CASIM_NOT_NUMERIC);
    refused("SQRT(2)×X-1", CASIM_NOT_NUMERIC);
    refused("X^2+Y", CASIM_NOT_NUMERIC);
    refused("X^13-1", CASIM_NO_ROOM);                     /* degree past 12 */
    refused("20000000×X-1", CASIM_NO_ROOM);               /* a coefficient past 10^7 after clearing */
    refused("X÷20000000-1", CASIM_NO_ROOM);
    CHECK(solve("2X", 'X' - 'A', CASIM_MODEL_35, out) == CASIM_SYNTAX && strcmp(out, "1") == 0, "solve 2X: \"%s\"", out);
    /* too long: twelve irrational roots */
    CHECK(solve("(X^2-X-3)×(X^2-X-5)×(X^2-X-7)×(X^2-X-11)×(X^2-X-13)×(X^2-X-17)", 'X' - 'A', CASIM_MODEL_35, out) ==
              CASIM_TOO_LONG && out[0] == '\0',
          "twelve irrational roots: \"%.60s\"", out);
    /* bad arguments */
    {
        casim_req rq = {CASIM_SOLVE, "X", CASIM_MODEL_35, 26, NULL, 0, CASIM_RAD};
        CHECK(casim_run(&rq, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "var 26 accepted");
    }
    CHECK(casim_solve("X^2-1", 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
              strcmp(out, "SOLVED: X=-1; X=1") == 0,
          "casim_solve X^2-1: \"%s\"", out);
}

/* abacus #4843: the edges. */
static void edges(void)
{
    /* a true linear factor at the bound (p, q near 10^7) beside a near-coincident root (9999973/9999991 by 1) */
    is("9999991×X^12-9999973×X^11+9999991×X-9999973", "PARTIAL, DEGREE 10 LEFT: X=-1; X=9999973÷9999991");
    is("(9999991×X-9999973)×(X+1)", "SOLVED: X=-1; X=9999973÷9999991");
    refused("(9999991×X-9999973)×(X-1)", CASIM_NO_ROOM);        /* its middle coefficient is past 10^7 */
    /* two rational roots 2E-7 apart, every coefficient under 10^7: ordered by value */
    is("(2236×X-2235)×(2235×X-2234)", "SOLVED: X=2234÷2235; X=2235÷2236");
    /* f at ±10^7 with a quotient that climbs past it: (X-1)×g, g rising to 5×10^7 */
    {
        char t[1024], out[OUT];
        long long g[12], a[13];
        int len = 0;
        for (int i = 0; i < 12; i++) g[i] = 10000000LL * (i < 6 ? i + 1 : 12 - i) - 3;
        a[0] = -g[0];
        for (int i = 1; i < 12; i++) a[i] = g[i - 1] - g[i];
        a[12] = g[11];
        for (int i = 12; i >= 0; i--)
            len += snprintf(t + len, sizeof t - (size_t)len, "%s%lld×X^%d", i == 12 || a[i] < 0 ? "" : "+", a[i], i);
        CHECK(solve(t, 'X' - 'A', CASIM_MODEL_35, out) == CASIM_OK && strstr(out, "X=1") != NULL, "%s: \"%s\"", t, out);
    }
    /* an irreducible quartic with real roots, times a rational and a quadratic root pair */
    is("(X^4-10×X^2+1)×(2×X+1)×(X^2-3)", "PARTIAL, DEGREE 4 LEFT: X=-SQRT(3); X=-1÷2; X=SQRT(3)");
}

/* ---- seeded polynomials from known factors ---- */

typedef struct { int deg; long long a, b, c; } fac;     /* deg 1: a X - b; deg 2: a X^2 + b X + c; deg 3: X^3 - c */

static unsigned long long rs = 2005;
static int rnd(int n) { rs = rs * 6364136223846793005ULL + 1442695040888963407ULL; return (int)((rs >> 33) % (unsigned)n); }
static long long gcdll(long long a, long long b) { a = llabs(a), b = llabs(b); while (b) { long long t = a % b; a = b, b = t; } return a; }

/* polynomial product into p (coefficients a[0..n]); false past 10^7 or degree 12 */
static int pmul(long long *p, int n, const long long *q, int m)
{
    long long r[32] = {0};
    for (int i = 0; i <= n; i++)
        for (int j = 0; j <= m; j++) r[i + j] += p[i] * q[j];
    for (int i = 0; i <= n + m; i++) {
        if (llabs(r[i]) > 10000000LL) return -1;
        p[i] = r[i];
    }
    return n + m;
}

/* the printed roots: each "X=text" and its multiplicity */
static int parse_roots(const char *out, char roots[][256], int *mult, int cap)
{
    const char *p = strchr(out, ':');
    int k = 0;
    if (!p) return 0;
    p++;
    while (*p && k < cap) {
        const char *end = strstr(p, "; ");
        size_t len = end ? (size_t)(end - p) : strlen(p);
        char item[256];
        memcpy(item, p, len);
        item[len] = '\0';
        mult[k] = 1;
        {
            char *m = strstr(item, " (MULT ");
            if (m) mult[k] = atoi(m + 7), *m = '\0';
        }
        {
            char *eq = strchr(item, '=');
            snprintf(roots[k], 256, "%s", eq ? eq + 1 : item);
        }
        k++;
        if (!end) break;
        p = end + 2;
    }
    return k;
}

static void seeded(void)
{
    int judged = 0, bad = 0, over = 0;
    ce_angle(CASIM_RAD);
    for (int n = 0; n < 3000 && bad < 6; n++) {
        long long p[32] = {1};
        int deg = 0, nf = 0, mults[16], realroots = 0, left = 0, k;
        fac fs[16];
        char text[2048], out[OUT], roots[32][256];
        int rm[32];
        double want[32];
        int wmult[32], nw = 0;
        bool fail = false;
        /* factors with multiplicities, degree <= 12 */
        while (deg < 12 && nf < 8 && !fail) {
            int kind = rnd(10), m = 1 + (rnd(4) == 0) + (rnd(8) == 0);
            fac f;
            long long q[4];
            int qn;
            if (kind < 5) {
                long long a = 1 + rnd(9), b = rnd(19) - 9;
                if (gcdll(a, b) != 1) continue;
                f = (fac){1, a, b, 0};
                q[0] = -b, q[1] = a, qn = 1;
            } else if (kind < 8) {
                long long a = 1 + rnd(3), b = rnd(7) - 3, c = rnd(21) - 10, d = b * b - 4 * a * c;
                long long s = (long long)llround(sqrt((double)(d < 0 ? 0 : d)));
                if (c == 0 || (d >= 0 && s * s == d) || gcdll(gcdll(a, b), c) != 1) continue;
                f = (fac){2, a, b, c};
                q[0] = c, q[1] = b, q[2] = a, qn = 2;
            } else {
                long long c = 2 + rnd(9);
                if (c == 8) continue;
                f = (fac){3, 1, 0, c};
                q[0] = -c, q[1] = 0, q[2] = 0, q[3] = 1, qn = 3;
            }
            /* a factor equal to one already there would merge its multiplicity: skip it */
            for (int i = 0; i < nf; i++)
                if (fs[i].deg == f.deg && fs[i].a == f.a && fs[i].b == f.b && fs[i].c == f.c) fail = true;
            if (fail) { fail = false; continue; }
            if (deg + m * qn > 12) break;
            for (int i = 0; i < m && !fail; i++) {
                int r = pmul(p, deg, q, qn);
                if (r < 0) fail = true;
                else deg = r;
            }
            if (fail) break;
            fs[nf] = f, mults[nf] = m, nf++;
        }
        if (fail || deg == 0) { over += fail; continue; }
        /* the text, the expected roots (by value) and the degree left */
        {
            int len = 0;
            for (int i = deg; i >= 0; i--)
                if (p[i]) len += snprintf(text + len, sizeof text - (size_t)len, "%s%lld×X^%d", p[i] < 0 || len == 0 ? "" : "+", p[i], i);
        }
        for (int i = 0; i < nf; i++) {
            if (fs[i].deg == 1) want[nw] = (double)fs[i].b / (double)fs[i].a, wmult[nw++] = mults[i], realroots++;
            else if (fs[i].deg == 2) {
                double d = (double)(fs[i].b * fs[i].b - 4 * fs[i].a * fs[i].c);
                if (d > 0) {
                    want[nw] = (-(double)fs[i].b - sqrt(d)) / (2.0 * (double)fs[i].a), wmult[nw++] = mults[i];
                    want[nw] = (-(double)fs[i].b + sqrt(d)) / (2.0 * (double)fs[i].a), wmult[nw++] = mults[i];
                    realroots += 2;
                }
            } else left += 3 * mults[i];
        }
        k = 0;
        {
            casim_status st = solve(text, 'X' - 'A', CASIM_MODEL_35, out);
            char head[64];
            if (st == CASIM_TOO_LONG) continue;
            judged++;
            if (st != CASIM_OK) { if (bad++ < 6) CHECK(false, "%s: status %d", text, (int)st); continue; }
            if (left) snprintf(head, sizeof head, "PARTIAL, DEGREE %d LEFT", left);
            else snprintf(head, sizeof head, "%s", realroots ? "SOLVED" : "NO REAL ROOTS");
            if (strncmp(out, head, strlen(head)) != 0) {
                if (bad++ < 6) CHECK(false, "%s: \"%s\", want it to start \"%s\"", text, out, head);
                continue;
            }
            k = parse_roots(out, roots, rm, 32);
        }
        /* every expected root printed once with its multiplicity, and nothing else; each text evaluates to it */
        {
            bool ok = k == nw;
            for (int i = 0; i < k && ok; i++) {
                abn v;
                double x;
                bool found = false;
                if (ce_eval(roots[i], &v) != ABN_OK) { ok = false; break; }
                x = strtod(ce_text(&v), NULL);             /* the core's text, mantissa E exponent */
                for (int j = 0; j < nw; j++)
                    if (fabs(x - want[j]) <= 1e-9 * fmax(1, fabs(want[j])) && rm[i] == wmult[j]) found = true;
                if (!found) ok = false;
                if (i > 0) {
                    abn u;
                    if (ce_eval(roots[i - 1], &u) == ABN_OK && abn_cmp(&u, &v) >= 0) ok = false;      /* ascending */
                }
            }
            if (!ok && bad++ < 6) CHECK(false, "%s: \"%s\" (%d expected roots)", text, out, nw);
        }
    }
    printf("solve: %d seeded polynomials judged, %d wrong, %d past the bounds\n", judged, bad, over);
    CHECK(bad == 0 && judged > 1500, "%d seeded polynomials judged, %d wrong (%d past the bounds)", judged, bad, over);
}

/* Every printed root makes the equation 0 on the core (rule 5's check: the text evaluates to a root). */
static void roots_are_roots(void)
{
    static const char *const eqs[] = {"X^2-X-3", "2×X^2-1", "X^4-5×X^2+6", "(X-1)×(X^2-2×X-1)", "6×X^2-X-2",
                                      "X^3-3×X^2-X+3", "9×X^2-12×X+2"};
    for (size_t i = 0; i < sizeof eqs / sizeof eqs[0]; i++) {
        char out[OUT], roots[32][256];
        int rm[32], k;
        if (solve(eqs[i], 'X' - 'A', CASIM_MODEL_35, out) != CASIM_OK) { CHECK(false, "%s did not solve", eqs[i]); continue; }
        k = parse_roots(out, roots, rm, 32);
        CHECK(k > 0, "%s: no roots in \"%s\"", eqs[i], out);
        for (int j = 0; j < k; j++) {
            /* the equation at the root, near 0 in absolute terms (a relative tolerance has no scale at 0) */
            abn x, f, tiny = ce_num("1E-25"), mag;
            if (ce_eval(roots[j], &x) != ABN_OK) {
                CHECK(false, "%s: root %s does not evaluate", eqs[i], roots[j]);
                continue;
            }
            ce_set_abn('X', x);
            CHECK(ce_eval(eqs[i], &f) == ABN_OK && (mag = abn_abs(&f), abn_cmp(&mag, &tiny) < 0), "%s at X=%s: %s", eqs[i],
                  roots[j], ce_text(&f));
        }
    }
}

/* work/005-examples.txt at the device's 8 KB arena: each answer's exact text, or its status. */
static void visible(void)
{
    static unsigned char small[8192];
    FILE *fp = fopen("work/005-examples.txt", "r");
    char line[2048];
    int n = 0;
    CHECK(fp != NULL, "work/005-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[7], *s = line, out[OUT];
        int k = 0;
        casim_req rq;
        casim_status st;
        const char *name;
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 7) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 6) continue;
        n++;
        rq = (casim_req){CASIM_SOLVE, f[2], strcmp(f[4], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35, f[3][0] - 'A', NULL, 0,
                         CASIM_RAD};
        st = casim_run(&rq, out, OUT, small, sizeof small);
        name = st == CASIM_OK ? out : st == CASIM_NOT_POLYNOMIAL ? "NOT_POLYNOMIAL" : st == CASIM_NOT_NUMERIC ? "NOT_NUMERIC"
             : st == CASIM_NO_ROOM ? "NO_ROOM" : st == CASIM_TOO_LONG ? "TOO_LONG" : "another status";
        CHECK(strcmp(name, f[5]) == 0, "%s: solve %s: \"%s\", want \"%s\"", f[0], f[2], name, f[5]);
    }
    fclose(fp);
    CHECK(n == 20, "%d visible examples, want 20", n);
}

int main(void)
{
    ce_init();
    visible();
    exact();
    edges();
    seeded();
    roots_are_roots();
    return check_done();
}
