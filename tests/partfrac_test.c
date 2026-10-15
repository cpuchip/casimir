/* Unit CAS 012: partial fractions over Q (work/012-partial-fractions.md). The visible examples at the 8,800 B arena; then
 * seeded fractions over planted factors (linear, irreducible quadratics, powers up to the 3rd, a planted common factor),
 * each answer judged by the core's own evaluator against the input at 14 points, its rows against the planted common
 * factor, and its status at 8,800 B against its status at 64 KB (a textbook shape never runs out of arena, rule 11).
 * scripts/partfrac-values.py judges the form (rule 4), the order (rule 5) and the uniqueness exactly. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static const char *const XS[14] = {"-3.1", "-2.3", "-1.7", "-1.2", "-0.6", "-0.35", "0.15", "0.45", "0.8", "1.3",
                                   "1.9", "2.4", "3.7", "5.3"};

static casim_status run(const char *text, int model, char *out, size_t arena_cap)
{
    casim_req rq = {CASIM_PARTFRAC, text, model, 'X' - 'A', NULL, model, CASIM_RAD, NULL};
    return casim_run(&rq, out, OUT, arena, arena_cap);
}

static const char *status_name(casim_status st)
{
    return st == CASIM_UNDEFINED ? "UNDEFINED" : st == CASIM_NO_ROOM ? "NO_ROOM" : st == CASIM_NOT_POLYNOMIAL ? "NOT_POLYNOMIAL"
         : st == CASIM_NOT_NUMERIC ? "NOT_NUMERIC" : st == CASIM_BAD_ARG ? "BAD_ARG" : st == CASIM_TOO_LONG ? "TOO_LONG"
         : "another status";
}

/* work/012-examples.txt at the 8,800 B arena */
static void visible(void)
{
    static unsigned char small[8800];
    FILE *fp = fopen("work/012-examples.txt", "r");
    char line[1024];
    int n = 0;
    CHECK(fp != NULL, "work/012-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[7], *s = line, out[OUT], got[OUT + 16];
        int k = 0;
        casim_status st;
        if (line[0] == '#' || line[0] == '\n') continue;
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
        {
            int model = strcmp(f[4], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35;
            casim_req rq = {CASIM_PARTFRAC, f[2], model, 'X' - 'A', NULL, model, CASIM_RAD, NULL};
            st = casim_run(&rq, out, OUT, small, sizeof small);
        }
        if (st == CASIM_OK) snprintf(got, sizeof got, "%s", out);
        else if (st == CASIM_PARTIAL) snprintf(got, sizeof got, "PARTIAL %s", out);
        else snprintf(got, sizeof got, "%s", status_name(st));
        CHECK(strcmp(got, f[5]) == 0, "%s: partfrac %s: \"%s\", want \"%s\"", f[0], f[2], got, f[5]);
    }
    fclose(fp);
    CHECK(n >= 40, "%d visible examples", n);
}

/* ---- the seeded fractions ---- */

static unsigned long long seed = 0x0c4a5012u;
static int rnd(int lo, int hi)
{
    seed = seed * 6364136223846793005ull + 1442695040888963407ull;
    return lo + (int)((seed >> 33) % (unsigned long long)(hi - lo + 1));
}

/* integer polynomials, low degree first */
typedef struct { int n; long long a[16]; } ip;

static void ip_mul(ip *r, const ip *x)
{
    ip t = {r->n + x->n, {0}};
    for (int i = 0; i <= r->n; i++)
        for (int j = 0; j <= x->n; j++) t.a[i + j] += r->a[i] * x->a[j];
    *r = t;
}

static int ip_text(const ip *p, char *s, size_t cap)
{
    int len = 0, first = 1;
    for (int i = p->n; i >= 0; i--) {
        long long a = p->a[i];
        if (a == 0) continue;
        len += snprintf(s + len, cap - (size_t)len, "%s", a < 0 ? "-" : first ? "" : "+");
        if (a < 0) a = -a;
        if (i == 0 || a != 1) len += snprintf(s + len, cap - (size_t)len, "%lld%s", a, i ? "×" : "");
        if (i == 1) len += snprintf(s + len, cap - (size_t)len, "X");
        if (i > 1) len += snprintf(s + len, cap - (size_t)len, "X^%d", i);
        first = 0;
    }
    if (first) len += snprintf(s + len, cap - (size_t)len, "0");
    return len;
}

/* irreducible over Q */
static const ip QUADS[] = {{2, {1, 0, 1}}, {2, {2, 0, 1}}, {2, {1, 1, 1}}, {2, {-2, 0, 1}}, {2, {3, 1, 2}}, {2, {2, -1, 1}}};

static void seeded(void)
{
    int ok = 0, too_long = 0, no_room = 0, rows_planted = 0;
    for (int trial = 0; trial < 600; trial++) {
        ip fs[4], num = {0, {0}};
        int ks[4], nf = rnd(1, 3), deg = 0, common = rnd(0, 3) == 0 ? rnd(-3, 3) : 99;
        char text[1024], out[OUT], out8[OUT], want_row[32] = "";
        int len = 0;
        casim_status st, st8;
        bool dup = false;
        for (int i = 0; i < nf; i++) {
            if (rnd(0, 2)) {
                int a = rnd(1, 4), b = rnd(-5, 5);
                while (b == 0 && a != 1) b = rnd(-5, 5);
                for (int g = 2; g <= 5; g++)
                    if (a % g == 0 && b % g == 0) a /= g, b /= g;
                fs[i] = (ip){1, {b, a}};
            } else fs[i] = QUADS[rnd(0, 5)];
            ks[i] = rnd(1, 3) == 3 ? rnd(2, 3) : 1;
            for (int j = 0; j < i; j++) dup |= fs[j].n == fs[i].n && memcmp(fs[j].a, fs[i].a, sizeof fs[i].a) == 0;
            deg += fs[i].n * ks[i];
        }
        if (dup || deg > 8) continue;
        /* the common factor X-c must not divide d already (its row would then be d's own) */
        for (int i = 0; i < nf && common != 99; i++)
            if (fs[i].n == 1 && fs[i].a[1] * common + fs[i].a[0] == 0) common = 99;
        num.n = rnd(0, deg + 1);
        for (int i = 0; i <= num.n; i++) num.a[i] = rnd(-9, 9);
        if (num.a[num.n] == 0) num.a[num.n] = 1;
        {
            /* a numerator sharing a factor with d would cancel more than planted: keep only the planted */
            bool shares = false;
            for (int i = 0; i < nf; i++) {
                if (fs[i].n != 1) {
                    /* a quadratic divides num iff num's pseudo-remainder by it is 0 */
                    ip r = num;
                    while (r.n >= 2) {
                        long long lead = r.a[r.n];
                        for (int k = 0; k <= r.n; k++) r.a[k] *= fs[i].a[2];
                        for (int k = 0; k <= 2; k++) r.a[r.n - 2 + k] -= lead * fs[i].a[k];
                        r.n--;
                    }
                    shares |= r.a[0] == 0 && (r.n < 1 || r.a[1] == 0);
                    continue;
                }
                /* num(-b/a) = 0 <=> sum num_k (-b)^k a^(n-k) = 0 */
                long long v = 0, pb = 1;
                for (int k = 0; k <= num.n; k++) {
                    long long pa = 1;
                    for (int m = k; m < num.n; m++) pa *= fs[i].a[1];
                    v += num.a[k] * pb * pa;
                    pb *= -fs[i].a[0];
                }
                shares |= v == 0;
            }
            if (shares) continue;
        }
        if (common != 99) {
            ip lin = {1, {-common, 1}};
            ip_mul(&num, &lin);
            snprintf(want_row, sizeof want_row, "X=%d", common);
        }
        len += snprintf(text + len, sizeof text - (size_t)len, "(");
        len += ip_text(&num, text + len, sizeof text - (size_t)len);
        len += snprintf(text + len, sizeof text - (size_t)len, ")÷(");
        if (common != 99) len += snprintf(text + len, sizeof text - (size_t)len, "(X%+d)×", -common);
        for (int i = 0; i < nf; i++) {
            len += snprintf(text + len, sizeof text - (size_t)len, "%s(", i ? "×" : "");
            len += ip_text(&fs[i], text + len, sizeof text - (size_t)len);
            len += snprintf(text + len, sizeof text - (size_t)len, ")");
            if (ks[i] > 1) len += snprintf(text + len, sizeof text - (size_t)len, "^%d", ks[i]);
        }
        snprintf(text + len, sizeof text - (size_t)len, ")");
        st = run(text, CASIM_MODEL_35, out, sizeof arena);
        st8 = run(text, CASIM_MODEL_35, out8, 8800);
        CHECK(st8 == st && (st != CASIM_OK || strcmp(out8, out) == 0), "partfrac %s at 8,800 B: %d \"%s\", at 64 KB %d \"%s\"",
              text, (int)st8, out8, (int)st, out);
        if (st == CASIM_TOO_LONG) {
            too_long++;
            continue;
        }
        if (st == CASIM_NO_ROOM) {
            no_room++;
            continue;
        }
        CHECK(st == CASIM_OK, "partfrac %s: status %d", text, (int)st);
        if (st != CASIM_OK) continue;
        ok++;
        {
            char *semi = strstr(out, "; ");
            if (want_row[0]) {
                rows_planted++;
                CHECK(semi && strstr(semi, want_row), "partfrac %s = %s: no row %s", text, out, want_row);
            } else CHECK(semi == NULL, "partfrac %s = %s: a row with no common factor", text, out);
            if (semi) *semi = '\0';
        }
        {
            bool same = true;
            for (int i = 0; i < 14 && same; i++) {
                abn va, vb;
                ce_set('X', XS[i]);
                if (ce_eval(text, &va) != ABN_OK) continue;      /* a pole */
                same = ce_eval(out, &vb) == ABN_OK && ce_close(&vb, &va, -20);
            }
            CHECK(same, "partfrac %s = %s: another value", text, out);
        }
    }
    printf("partfrac: %d judged by value (%d with a planted row), %d TOO_LONG, %d NO_ROOM\n", ok, rows_planted, too_long,
           no_room);
    CHECK(ok > 300 && rows_planted > 50, "%d judged, %d rows", ok, rows_planted);
    CHECK(no_room == 0, "%d NO_ROOM on textbook shapes", no_room);
}

int main(void)
{
    ce_init();
    ce_angle(CASIM_RAD);
    visible();
    seeded();
    return check_done();
}
