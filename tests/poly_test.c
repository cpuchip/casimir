/* Unit CAS 010: polynomial division, GCD and cancelling over Q (work/010-polynomial-division-gcd.md). The visible
 * examples; seeded pairs judged against this file's own exact arithmetic (rationals in __int128 on the host, not
 * casim's): PDIV's q and r and PGCD's gcd (up to the constant rule 2 fixes) compared by value at 14 points, which
 * decides a polynomial of degree <= 12 exactly; CANCEL by its identity and its excluded rows against the planted gcd. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
#define DEG 12
static unsigned char arena[65536];

static casim_status run2(casim_op op, const char *f, const char *g, int model, char *out, size_t cap)
{
    casim_req rq = {op, f, model, 'X' - 'A', g, model, CASIM_RAD, NULL};
    return casim_run(&rq, out, OUT, arena, cap);
}

/* ---- exact rationals in __int128 (host only) ---- */
__extension__ typedef __int128 i128;           /* host only: the test's exact oracle */
typedef struct { i128 p, q; } qq;
typedef struct { int n; qq a[DEG + 1]; } qp;          /* n = -1: the zero polynomial */

static i128 gcd128(i128 a, i128 b) { if (a < 0) a = -a; if (b < 0) b = -b; while (b) { i128 t = a % b; a = b; b = t; } return a; }
static bool big(i128 v) { return v > (i128)1 << 100 || v < -((i128)1 << 100); }
static bool qmk(qq *r, i128 p, i128 q)
{
    i128 g;
    if (q == 0) return false;
    if (q < 0) p = -p, q = -q;
    g = p == 0 ? q : gcd128(p, q);
    r->p = p / g, r->q = q / g;
    return !big(r->p) && !big(r->q);
}
static bool qadd(qq *r, qq x, qq y) { return qmk(r, x.p * y.q + y.p * x.q, x.q * y.q); }
static bool qmul(qq *r, qq x, qq y) { return qmk(r, x.p * y.p, x.q * y.q); }
static bool qdiv(qq *r, qq x, qq y) { return y.p != 0 && qmk(r, x.p * y.q, x.q * y.p); }
static void trim(qp *f) { while (f->n >= 0 && f->a[f->n].p == 0) f->n--; }

/* f = q g + r */
static bool qdivmod(const qp *f, const qp *g, qp *q, qp *r)
{
    *r = *f;
    q->n = f->n - g->n;
    for (int i = 0; i <= DEG; i++) q->a[i] = (qq){0, 1};
    if (q->n < 0) { q->n = -1; return true; }
    for (int i = f->n; i >= g->n; i--) {
        qq c;
        if (!qdiv(&c, r->a[i], g->a[g->n])) return false;
        q->a[i - g->n] = c;
        for (int j = 0; j <= g->n; j++) {
            qq t;
            if (!qmul(&t, c, g->a[j]) || !qmk(&t, -t.p, t.q) || !qadd(&r->a[i - g->n + j], r->a[i - g->n + j], t)) return false;
        }
    }
    r->n = g->n - 1;
    trim(r), trim(q);
    return true;
}

/* the monic gcd (Euclid over Q); false on overflow */
static bool qgcd(qp a, qp b, qp *g)
{
    trim(&a), trim(&b);
    while (b.n >= 0) {
        qp q, r;
        if (!qdivmod(&a, &b, &q, &r)) return false;
        a = b, b = r;
    }
    *g = a;
    if (g->n >= 0) {
        qq l = g->a[g->n];
        for (int i = 0; i <= g->n; i++) if (!qdiv(&g->a[i], g->a[i], l)) return false;
    }
    return true;
}

static void qp_text(const qp *f, char *t, size_t n)
{
    size_t k = 0;
    t[0] = '\0';
    if (f->n < 0) { snprintf(t, n, "0"); return; }
    for (int i = f->n; i >= 0; i--)
        if (f->a[i].p != 0)
            k += (size_t)snprintf(t + k, n - k, "%s(%lld÷%lld)×X^%d", k ? "+" : "", (long long)f->a[i].p, (long long)f->a[i].q, i);
    if (!k) snprintf(t, n, "0");
}

static const char *const XS[14] = {"-3.1", "-2.3", "-1.7", "-1.2", "-0.6", "-0.25", "0.15", "0.45", "0.8", "1.3", "1.9", "2.4",
                                   "2.9", "3.7"};

/* a and b agree by value at the 14 points (relative 1E-20, or both within 1E-20 of 0) */
static bool same14(const char *a, const char *b)
{
    for (int i = 0; i < 14; i++) {
        abn va, vb;
        ce_set('X', XS[i]);
        if (ce_eval(a, &va) != ABN_OK || ce_eval(b, &vb) != ABN_OK) return false;
        if (!ce_close(&va, &vb, -20)) return false;
    }
    return true;
}

/* b = k a for one constant k != 0, at the 14 points (decides it for degree <= 12): k from the first point, then
   |b - k a| <= 1E-20 |b| at every point, in abn */
static bool proportional(const char *a, const char *b)
{
    abn va, vb, k, ka, d, tol, s;
    for (int i = 0; i < 14; i++) {
        ce_set('X', XS[i]);
        if (ce_eval(a, &va) != ABN_OK || ce_eval(b, &vb) != ABN_OK) return false;
        if (i == 0) {
            if (abn_is_zero(&va) || abn_div(&k, &vb, &va) != ABN_OK) return false;
            continue;
        }
        if (abn_mul(&ka, &k, &va) != ABN_OK || abn_sub(&d, &vb, &ka) != ABN_OK || abn_pow10(&tol, -20) != ABN_OK) return false;
        d = abn_abs(&d), s = abn_abs(&vb);
        if (abn_mul(&tol, &tol, &s) != ABN_OK || abn_cmp(&d, &tol) > 0) return false;
    }
    return true;
}

/* ---- seeded polynomials ---- */
static unsigned seed = 1010;
static int rnd(int n) { seed = seed * 1103515245u + 12345u; return (int)((seed >> 16) % (unsigned)n); }
static void rq(qp *f, int deg, int cm, bool frac)
{
    f->n = deg;
    for (int i = 0; i <= deg; i++) f->a[i] = (qq){rnd(2 * cm + 1) - cm, frac && rnd(3) == 0 ? 1 + rnd(4) : 1};
    if (f->a[deg].p == 0) f->a[deg].p = 1;
    for (int i = 0; i <= deg; i++) qmk(&f->a[i], f->a[i].p, f->a[i].q);
}
static bool qpmul(qp *r, const qp *a, const qp *b)
{
    qp t = {a->n + b->n, {{0, 1}}};
    for (int i = 0; i <= DEG; i++) t.a[i] = (qq){0, 1};
    if (t.n > DEG) return false;
    for (int i = 0; i <= a->n; i++)
        for (int j = 0; j <= b->n; j++) {
            qq x;
            if (!qmul(&x, a->a[i], b->a[j]) || !qadd(&t.a[i + j], t.a[i + j], x)) return false;
        }
    *r = t;
    return true;
}

static void pdiv_seeded(void)
{
    int judged = 0, noroom = 0;
    for (int t = 0; t < 500; t++) {
        qp f, g, q, r;
        char ft[1024], gt[1024], out[OUT], qt[1024], rt[1024], *semi;
        int df = 1 + rnd(9), dg = rnd(df + 2);
        casim_status st;
        rq(&f, df, 9, true), rq(&g, dg, 9, true);
        if (!qdivmod(&f, &g, &q, &r)) continue;
        qp_text(&f, ft, sizeof ft), qp_text(&g, gt, sizeof gt), qp_text(&q, qt, sizeof qt), qp_text(&r, rt, sizeof rt);
        st = run2(CASIM_PDIV, ft, gt, CASIM_MODEL_35, out, sizeof arena);
        if (st == CASIM_NO_ROOM || st == CASIM_TOO_LONG) { noroom++; continue; }
        judged++;
        semi = strstr(out, "; ");
        CHECK(st == CASIM_OK && semi, "pdiv %s by %s: %d \"%s\"", ft, gt, (int)st, out);
        if (st != CASIM_OK || !semi) continue;
        *semi = '\0';
        CHECK(same14(out, qt) && same14(semi + 2, rt), "pdiv %s by %s: \"%s; %s\", want q %s, r %s", ft, gt, out, semi + 2, qt, rt);
    }
    printf("poly: pdiv %d judged, %d NO_ROOM or TOO_LONG\n", judged, noroom);
    CHECK(judged > 400, "pdiv %d judged", judged);
}

static void pgcd_seeded(void)
{
    int judged = 0;
    for (int t = 0; t < 500; t++) {
        qp a, b, c, f, g, m;
        char ft[1024], gt[1024], mt[1024], out[OUT];
        casim_status st;
        rq(&c, rnd(4), 9, false), rq(&a, rnd(5), 9, true), rq(&b, rnd(5), 9, true);
        if (!qpmul(&f, &a, &c) || !qpmul(&g, &b, &c) || !qgcd(f, g, &m)) continue;
        qp_text(&f, ft, sizeof ft), qp_text(&g, gt, sizeof gt), qp_text(&m, mt, sizeof mt);
        st = run2(CASIM_PGCD, ft, gt, CASIM_MODEL_35, out, sizeof arena);
        if (st == CASIM_TOO_LONG) continue;
        judged++;
        CHECK(st == CASIM_OK && (m.n <= 0 ? strcmp(out, "1") == 0 : proportional(mt, out)), "pgcd %s, %s: %d \"%s\", want ∝ %s", ft,
              gt, (int)st, out, mt);
    }
    printf("poly: pgcd %d judged\n", judged);
    CHECK(judged > 400, "pgcd %d judged", judged);
}

/* CANCEL: a planted gcd of linear factors (rows X=a) and perhaps X^2-2 (a row X^2-2=0), random cofactors; the
   answer's first row equals the input by value away from the removed values, and the rows are the planted ones */
static void cancel_seeded(void)
{
    int judged = 0;
    for (int t = 0; t < 400; t++) {
        char text[2048], out[OUT], want_rows[512] = "", *semi;
        qp num, den, g, m;
        int roots[4], nr = 1 + rnd(3), len = 0;
        bool quad = rnd(3) == 0;
        casim_status st;
        /* distinct roots in -4..4 */
        for (int i = 0; i < nr; i++) {
            int r;
            bool dup;
            do { r = rnd(9) - 4; dup = false; for (int j = 0; j < i; j++) dup |= roots[j] == r; } while (dup);
            roots[i] = r;
        }
        for (int i = 1; i < nr; i++) for (int j = i; j > 0 && roots[j - 1] > roots[j]; j--) { int x = roots[j]; roots[j] = roots[j - 1]; roots[j - 1] = x; }
        rq(&num, rnd(3), 5, false), rq(&den, rnd(3), 5, false);
        if (!qgcd(num, den, &m) || m.n > 0) continue;            /* the cofactors must be coprime */
        {
            bool zero = false;
            for (int i = 0; i < nr; i++) {
                qq v = {0, 1};
                for (int k = num.n; k >= 0; k--) qmul(&v, v, (qq){roots[i], 1}), qadd(&v, v, num.a[k]);
                zero |= v.p == 0;
                v = (qq){0, 1};
                for (int k = den.n; k >= 0; k--) qmul(&v, v, (qq){roots[i], 1}), qadd(&v, v, den.a[k]);
                zero |= v.p == 0;
            }
            if (zero) continue;                                  /* a cofactor sharing a root would cancel more */
        }
        len += snprintf(text + len, sizeof text - (size_t)len, "(");
        {
            char nt[512];
            qp_text(&num, nt, sizeof nt);
            len += snprintf(text + len, sizeof text - (size_t)len, "%s)", nt);
        }
        for (int i = 0; i < nr; i++) len += snprintf(text + len, sizeof text - (size_t)len, "×(X%+d)", -roots[i]);
        if (quad) len += snprintf(text + len, sizeof text - (size_t)len, "×(X^2-2)");
        {
            char dt[512];
            qp_text(&den, dt, sizeof dt);
            len += snprintf(text + len, sizeof text - (size_t)len, "÷((%s)", dt);
        }
        for (int i = 0; i < nr; i++) len += snprintf(text + len, sizeof text - (size_t)len, "×(X%+d)", -roots[i]);
        if (quad) len += snprintf(text + len, sizeof text - (size_t)len, "×(X^2-2)");
        len += snprintf(text + len, sizeof text - (size_t)len, ")");
        for (int i = 0, k = 0; i < nr; i++) k += snprintf(want_rows + k, sizeof want_rows - (size_t)k, "; X=%d", roots[i]);
        if (quad) strcat(want_rows, "; X^2-2=0");
        st = run2(CASIM_CANCEL, text, NULL, CASIM_MODEL_35, out, sizeof arena);
        if (st == CASIM_TOO_LONG || st == CASIM_NO_ROOM) continue;
        judged++;
        semi = strstr(out, "; ");
        CHECK(st == CASIM_OK && semi && strcmp(semi, want_rows) == 0, "cancel %s: %d \"%s\", want rows \"%s\"", text, (int)st, out,
              want_rows);
        if (st != CASIM_OK || !semi) continue;
        *semi = '\0';
        {
            /* the identity at the points (none of them is a removed value: they are not integers, nor ±SQRT(2)) */
            bool ok = true;
            (void)g;
            for (int i = 0; i < 14 && ok; i++) {
                abn va, vb;
                ce_set('X', XS[i]);
                if (ce_eval(text, &va) != ABN_OK) continue;
                ok = ce_eval(out, &vb) == ABN_OK && ce_close(&vb, &va, -20);
            }
            CHECK(ok, "cancel %s = %s: another value", text, out);
        }
    }
    printf("poly: cancel %d judged\n", judged);
    CHECK(judged > 250, "cancel %d judged", judged);
}

/* work/010-examples.txt at the 8 KB arena */
static void visible(void)
{
    static unsigned char small[8192];
    FILE *fp = fopen("work/010-examples.txt", "r");
    char line[1024];
    int n = 0;
    CHECK(fp != NULL, "work/010-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[7], *s = line, out[OUT];
        int k = 0;
        casim_status st;
        casim_op op;
        const char *name;
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
        op = strcmp(f[1], "pdiv") == 0 ? CASIM_PDIV : strcmp(f[1], "pgcd") == 0 ? CASIM_PGCD : CASIM_CANCEL;
        {
            int model = strcmp(f[4], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35;
            casim_req rq = {op, f[2], model, 'X' - 'A', strcmp(f[3], "-") == 0 ? NULL : f[3], model, CASIM_RAD, NULL};
            st = casim_run(&rq, out, OUT, small, sizeof small);
        }
        name = st == CASIM_OK ? out : st == CASIM_UNDEFINED ? "UNDEFINED" : st == CASIM_NO_ROOM ? "NO_ROOM"
             : st == CASIM_NOT_POLYNOMIAL ? "NOT_POLYNOMIAL" : st == CASIM_BAD_ARG ? "BAD_ARG" : "another status";
        CHECK(strcmp(name, f[5]) == 0, "%s: %s %s, %s: \"%s\", want \"%s\"", f[0], f[1], f[2], f[3], name, f[5]);
    }
    fclose(fp);
    CHECK(n >= 20, "%d visible examples", n);
}

int main(void)
{
    ce_init();
    ce_angle(CASIM_RAD);
    visible();
    pdiv_seeded();
    pgcd_seeded();
    cancel_seeded();
    return check_done();
}
