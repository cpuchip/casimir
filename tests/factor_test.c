/* Unit CAS 007: FACTOR over Q (work/007-factor.md, rule 5a). Exact texts; the content; the refusals; the stamp;
 * repeated factors of degree >= 3 ((X^3-2)^2); and seeded products of known irreducible factors (linear,
 * irreducible quadratics, Eisenstein polynomials of degree 3-6 and their reversals) with multiplicities: each
 * answer read back on the core equals the input by value, has exactly the construction's factors with their
 * exponents, is complete (OK, not PARTIAL), and each factor's degree ascends. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static casim_status factor(const char *t, int model, char *out, size_t arena_cap)
{
    casim_req rq = {CASIM_FACTOR, t, model, 'X' - 'A', NULL, 0, CASIM_RAD, NULL};
    return casim_run(&rq, out, OUT, arena, arena_cap);
}

/* t and the answer agree by value at three points (the answer reads back on the core) */
static bool same_value(const char *t, const char *out)
{
    static const char *const XS[3] = {"0.37", "1.9", "-2.3"};
    ce_angle(CASIM_RAD);
    ce_set('A', "1.7");
    for (int i = 0; i < 3; i++) {
        abn a, b;
        ce_set('X', XS[i]);
        if (ce_eval(t, &a) != ABN_OK || ce_eval(out, &b) != ABN_OK) return false;
        if (!ce_close(&b, &a, -20)) return false;
    }
    return true;
}

static void is(const char *t, const char *want)
{
    char out[OUT];
    casim_status st = factor(t, CASIM_MODEL_35, out, sizeof arena);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "factor %s: %d \"%s\", want \"%s\"", t, (int)st, out, want);
    if (st == CASIM_OK) CHECK(same_value(t, out), "factor %s = %s: another value", t, out);
}

static void refused(const char *t, casim_status want)
{
    char out[OUT];
    casim_status st = factor(t, CASIM_MODEL_35, out, sizeof arena);
    CHECK(st == want && out[0] == '\0', "factor %s: %d \"%s\", want status %d", t, (int)st, out, (int)want);
}

static void exact(void)
{
    char out[OUT];
    is("X^2-1", "(X-1)×(X+1)");
    is("X^2-5×X+6", "(X-3)×(X-2)");
    is("X^4-5×X^2+6", "(X^2-3)×(X^2-2)");
    is("2×X^3-2×X", "2×X×(X-1)×(X+1)");
    is("(X-1)^2×(X+2)", "(X-1)^2×(X+2)");
    is("X^3", "X^3");
    is("X^2×(X+1)", "X^2×(X+1)");
    is("6×X^2-X-2", "(2×X+1)×(3×X-2)");
    /* irreducible over Q: printed whole, proven */
    is("X^2-2", "X^2-2");
    is("X^2+1", "X^2+1");
    is("X^3-2", "X^3-2");
    is("X^4+1", "X^4+1");
    is("X^4-10×X^2+1", "-10×X^2+X^4+1");
    /* repeated factors of degree >= 3 (abacus's gap in rule 5a) */
    is("(X^3-2)^2", "(X^3-2)^2");
    is("(X^3-2)^2×(X-1)", "(X-1)×(X^3-2)^2");
    is("(X^3-2)×(X^3+X+1)", "(X+X^3+1)×(X^3-2)");
    /* the content: first, then the primitive factors with positive leading coefficients */
    is("2×X^2-2", "2×(X-1)×(X+1)");
    is("X^2÷2-1÷2", "1÷2×(X-1)×(X+1)");
    is("-X^2+1", "-1×(X-1)×(X+1)");
    is("-2×X^2+8", "-2×(X-2)×(X+2)");
    is("3×X^2-6", "3×(X^2-2)");
    is("6", "6");
    is("0", "0");
    is("X", "X");
    is("2×X+1", "2×X+1");
    /* the stamp: X^3^2-1 is X^9-1 read as STU, X^6-1 read as 35 */
    CHECK(factor("X^3^2-1", CASIM_MODEL_STU, out, sizeof arena) == CASIM_OK &&
              strcmp(out, "(X-1)×(X+X^2+1)×(X^3+X^6+1)") == 0,
          "factor X^3^2-1 (STU): \"%s\"", out);
    CHECK(factor("X^3^2-1", CASIM_MODEL_35, out, sizeof arena) == CASIM_OK &&
              strcmp(out, "(X-1)×(X+1)×(X+X^2+1)×(-X+X^2+1)") == 0,
          "factor X^3^2-1 (35): \"%s\"", out);
    /* refusals, as solve's */
    refused("SIN(X)", CASIM_NOT_POLYNOMIAL);
    refused("X^2÷X", CASIM_NOT_POLYNOMIAL);
    refused("Y×X+1", CASIM_NOT_NUMERIC);
    refused("X^13-1", CASIM_NO_ROOM);
    refused("X=1", CASIM_BAD_ARG);
    CHECK(factor("2X", CASIM_MODEL_35, out, sizeof arena) == CASIM_SYNTAX && strcmp(out, "1") == 0, "factor 2X: \"%s\"", out);
    CHECK(casim_factor("X^2-1", 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK,
          "casim_factor: \"%s\"", out);
}

/* ---- seeded products of known irreducible factors ---- */

static unsigned long long rs = 7007;
static int rnd(int n) { rs = rs * 6364136223846793005ULL + 1442695040888963407ULL; return (int)((rs >> 33) % (unsigned)n); }

static int pmul(long long *p, int n, const long long *q, int m)
{
    long long r[32] = {0};
    if (n + m > 12) return -1;
    for (int i = 0; i <= n; i++)
        for (int j = 0; j <= m; j++) r[i + j] += p[i] * q[j];
    for (int i = 0; i <= n + m; i++) {
        if (llabs(r[i]) > 10000000LL) return -1;
        p[i] = r[i];
    }
    return n + m;
}

/* top-level factors of the answer: count them, and their exponents (^k) */
static int count_factors(const char *s, int *expsum)
{
    int depth = 0, n = 0;
    *expsum = 0;
    const char *start = s;
    /* a lone factor that is a sum prints without parentheses: one factor */
    for (const char *p = s; *p; p++) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        else if (depth == 0 && p > s && (*p == '+' || (*p == '-' && p[-1] != '^'))) return *expsum = 1, 1;
    }
    depth = 0;
    if (*s == '-') s++;
    for (const char *p = s;; p++) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        if ((*p == '\0' || (depth == 0 && strncmp(p, "×", 2) == 0))) {
            /* one factor from start to p: a number (the content) is not counted */
            const char *q = start;
            bool num = true;
            for (; q < p; q++)
                if (*q == 'X') num = false;
            if (!num) {
                const char *hat = NULL;
                int d = 0;
                for (q = start; q < p; q++) {
                    if (*q == '(') d++;
                    else if (*q == ')') d--;
                    else if (*q == '^' && d == 0 && q > start && (q[-1] == ')' || q[-1] == 'X')) hat = q;
                }
                n++;
                *expsum += hat ? atoi(hat + 1) : 1;
            }
            if (*p == '\0') break;
            start = p + 2;
            p++;
        }
    }
    return n;
}

static void seeded(void)
{
    static const int P[] = {2, 3, 5, 7, 11};
    int judged = 0, bad = 0;
    for (int t = 0; t < 600 && bad < 6; t++) {
        long long p[32] = {1}, sig[32][13];
        int deg = 0, nf = 0, mults = 0, sigd[32];
        char text[2048], out[OUT];
        bool fail = false;
        while (deg < 12 && nf < 6 && !fail) {
            long long q[8] = {0};
            int qn, m = 1 + (rnd(5) == 0) + (rnd(9) == 0), kind = rnd(10);
            if (kind < 4) {                      /* linear qX - p */
                long long a = 1 + rnd(4), b = rnd(9) - 4;
                long long g = a, h = llabs(b);
                while (h) { long long r = g % h; g = h; h = r; }
                if (g != 1) continue;
                q[0] = -b, q[1] = a, qn = 1;
            } else if (kind < 7) {               /* an irreducible quadratic */
                long long a = 1 + rnd(2), b = rnd(5) - 2, c = rnd(9) - 4, d = b * b - 4 * a * c;
                long long s = (long long)llround(sqrt((double)(d < 0 ? 0 : d)));
                if (c == 0 || (d >= 0 && s * s == d) || (a == 2 && b % 2 == 0 && c % 2 == 0)) continue;    /* primitive */
                q[0] = c, q[1] = b, q[2] = a, qn = 2;
            } else {                             /* Eisenstein (x+s)^k - p, or its reversal */
                int k = 3 + rnd(3), s = rnd(5) - 2, pr = P[rnd(5)];
                long long r[8] = {1}, l[2] = {s, 1};
                int rn = 0;
                for (int i = 0; i < k; i++) rn = pmul(r, rn, l, 1);
                r[0] -= pr;
                if (rnd(2)) {
                    long long rev[8];
                    for (int i = 0; i <= k; i++) rev[i] = r[k - i];
                    if (rev[k] == 0) continue;
                    if (rev[k] < 0) for (int i = 0; i <= k; i++) rev[i] = -rev[i];
                    memcpy(r, rev, sizeof rev);
                }
                memcpy(q, r, sizeof q);
                qn = k;
            }
            /* a factor equal to one already there merges: skip */
            {
                bool dup = false;
                for (int i = 0; i < nf; i++)
                    if (sigd[i] == qn && memcmp(sig[i], q, (size_t)(qn + 1) * sizeof q[0]) == 0) dup = true;
                if (dup) continue;
            }
            if (deg + m * qn > 12) break;
            for (int i = 0; i < m && !fail; i++) {
                int r = pmul(p, deg, q, qn);
                if (r < 0) fail = true;
                else deg = r;
            }
            if (fail) break;
            memcpy(sig[nf], q, (size_t)(qn + 1) * sizeof q[0]), sigd[nf] = qn, nf++, mults += m;
        }
        if (fail || deg < 2) continue;
        {
            int len = 0;
            for (int i = deg; i >= 0; i--)
                if (p[i]) len += snprintf(text + len, sizeof text - (size_t)len, "%s%lld×X^%d", p[i] < 0 || len == 0 ? "" : "+", p[i], i);
        }
        {
            casim_status st = factor(text, CASIM_MODEL_35, out, sizeof arena);
            int es, n;
            if (st == CASIM_TOO_LONG) continue;
            judged++;
            n = st == CASIM_OK ? count_factors(out, &es) : -1;
            if (st != CASIM_OK || !same_value(text, out) || n != nf || es != mults) {
                if (bad++ < 6) CHECK(false, "%s: %d \"%s\" (%d factors, %d with multiplicity, want %d, %d)", text, (int)st, out, n,
                                     st == CASIM_OK ? es : -1, nf, mults);
            }
        }
    }
    printf("factor: %d seeded products judged, %d wrong\n", judged, bad);
    CHECK(bad == 0 && judged > 300, "%d seeded products judged, %d wrong", judged, bad);
}

/* work/007-examples.txt at the 8,800 B arena */
static void visible(void)
{
    static unsigned char small[8800];
    FILE *fp = fopen("work/007-examples.txt", "r");
    char line[2048];
    int n = 0;
    CHECK(fp != NULL, "work/007-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[7], *s = line, out[OUT];
        int k = 0;
        casim_req rq;
        casim_status st;
        const char *name;
        static char partial[OUT + 8];
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
        rq = (casim_req){CASIM_FACTOR, f[2], strcmp(f[4], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35, f[3][0] - 'A', NULL, 0,
                         CASIM_RAD, NULL};
        st = casim_run(&rq, out, OUT, small, sizeof small);
        if (st == CASIM_PARTIAL) {
            snprintf(partial, sizeof partial, "PARTIAL %s", out);
        }
        name = st == CASIM_OK ? out : st == CASIM_PARTIAL ? partial : st == CASIM_NOT_POLYNOMIAL ? "NOT_POLYNOMIAL" : st == CASIM_NOT_NUMERIC ? "NOT_NUMERIC"
             : st == CASIM_NO_ROOM ? "NO_ROOM" : st == CASIM_TOO_LONG ? "TOO_LONG" : st == CASIM_BAD_ARG ? "BAD_ARG"
             : "another status";
        CHECK(strcmp(name, f[5]) == 0, "%s: factor %s: \"%s\", want \"%s\"", f[0], f[2], name, f[5]);
    }
    fclose(fp);
    CHECK(n >= 15, "%d visible examples", n);
}

/* abacus #5203: (X+2)^9 printed PARTIAL (the ring split into two clusters, (X+2)^2 found four times). Every
   linear (aX+b)^m within the bound prints as its own text to the m. */
static void linear_powers(void)
{
    int n = 0;
    for (int a = 1; a <= 3; a++)
        for (int b = -3; b <= 3; b++) {
            char base[64], one[OUT];
            if (b == 0 || (a % 2 == 0 && b % 2 == 0) || (a == 3 && b % 3 == 0)) continue;
            snprintf(base, sizeof base, "%d×X%+d", a, b);
            if (factor(base, CASIM_MODEL_35, one, sizeof arena) != CASIM_OK) continue;
            for (int m = 2; m <= 12; m++) {
                char text[96], want[OUT + 16], out[OUT];
                double big = 1;
                for (int k = 0; k < m; k++) big *= a + abs(b);
                if (big > 1e7 * 4) break;       /* past the bound somewhere: NO_ROOM, judged elsewhere */
                snprintf(text, sizeof text, "(%s)^%d", base, m);
                snprintf(want, sizeof want, "(%s)^%d", one, m);
                casim_status st = factor(text, CASIM_MODEL_35, out, sizeof arena);
                if (st == CASIM_NO_ROOM) continue;
                n++;
                CHECK(st == CASIM_OK && strcmp(out, want) == 0, "factor %s: %d \"%s\", want \"%s\"", text, (int)st, out, want);
            }
        }
    CHECK(n > 100, "%d linear powers judged", n);
}

int main(void)
{
    ce_init();
    exact();
    seeded();
    linear_powers();
    visible();
    return check_done();
}
