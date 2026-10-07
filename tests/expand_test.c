/* Unit CAS 003, expand: each rule; the visible E pairs; and a second, differently shaped instrument, our
 * own dense polynomial expander (exact integers over X and Y, no casim code). It builds a random product
 * or power of sums A, multiplies it out itself, and prints the result as B with its terms shuffled and
 * their factors in random order. expand(A) and expand(B) must be identical text, keep A's value through
 * the core (1E-24), and be a fixed point; also inside SIN and under a ÷. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
#define D 13                    /* degrees 0..12 in X and in Y */
static unsigned char arena[65536];
static unsigned long long rng = 0x6A09E667F3BCC909ull;

static unsigned rnd(unsigned n)
{
    rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
    return (unsigned)(rng % n);
}

static casim_status ex(const char *in, char *out)
{
    return casim_expand(in, CASIM_RAD, out, OUT, arena, sizeof arena);
}

/* The core's values of a and b agree to 1E-24 at two points (where a is defined). */
static bool same_value(const char *a, const char *b)
{
    static const char *const XS[2] = {"0.7", "1.45"};
    ce_angle(CASIM_RAD);
    for (int i = 0; i < 2; i++) {
        abn va, vb;
        ce_set('X', XS[i]);
        ce_set('Y', "1.3");
        ce_set('Z', "0.45");
        if (ce_eval(a, &va) != ABN_OK) continue;
        if (ce_eval(b, &vb) != ABN_OK || !ce_close(&vb, &va, -24)) return false;
    }
    return true;
}

/* a and b expand to the same text, with a's value, and the result is a fixed point. */
static void pair(const char *a, const char *b)
{
    char oa[OUT], ob[OUT], again[OUT];
    casim_status sa = ex(a, oa), sb = ex(b, ob);
    CHECK(sa == CASIM_OK && sb == CASIM_OK && strcmp(oa, ob) == 0, "expand differs:\n  %s\n  -> %s (%d)\n  %s\n  -> %s (%d)",
          a, oa, (int)sa, b, ob, (int)sb);
    if (sa != CASIM_OK) return;
    CHECK(same_value(a, oa), "expand %s = %s changes the value", a, oa);
    CHECK(ex(oa, again) == CASIM_OK && strcmp(again, oa) == 0, "expand is not a fixed point: %s -> %s", oa, again);
}

/* in expands to exactly want (and the checks of pair). */
static void is(const char *in, const char *want)
{
    char out[OUT];
    CHECK(ex(in, out) == CASIM_OK && strcmp(out, want) == 0, "expand %s: \"%s\", want \"%s\"", in, out, want);
    pair(in, want);
}

static void rules(void)
{
    /* × over + */
    pair("X×(Y+Z)", "X×Y+X×Z");
    pair("(X+1)×(X-1)", "X^2-1");
    pair("(X+Y)×(X-Y)", "X^2-Y^2");
    /* a sum to a whole power: (X+Y)^3, multinomial */
    pair("(X+Y)^3", "X^3+3×X^2×Y+3×X×Y^2+Y^3");
    pair("(X+Y+1)^2", "X^2+Y^2+1+2×X×Y+2×X+2×Y");
    pair("(2×X-3)^2", "4×X^2-12×X+9");
    is("(X+1)^0", "1");
    is("X^2", "X^2");
    /* at every depth: inside a function's argument, and nested */
    pair("SIN((X+1)×(X-1))", "SIN(X^2-1)");
    pair("COS(SIN((X+Y)^2)+X×(1+Y))", "COS(SIN(X^2+2×X×Y+Y^2)+X+X×Y)");
    pair("EXP(X×(Y+1))", "EXP(X×Y+X)");       /* the argument expanded; EXP(a+b) itself stays */
    /* not expanded: a sum under a ÷, or to a power that is not whole: expanded inside */
    pair("X÷((Y+1)×(Y-1))", "X÷(Y^2-1)");
    pair("(X+1)^-2", "1÷(X+1)^2");
    pair("((X+1)×(X-1))^-1", "1÷(X^2-1)");
    pair("SQRT((X+1)^2)", "SQRT(X^2+2×X+1)");
    pair("((X+1)×X)^0.5", "SQRT(X^2+X)");
    /* too long is a status (rule 8): past 191 bytes, whatever the cap; an expansion that cannot fit stops
       before it builds (more distinct terms than 64), so (X+1)^1000 returns at once */
    {
        char out[OUT];
        clock_t t0;
        CHECK(ex("(X+Y+1)^6", out) == CASIM_TOO_LONG && out[0] == '\0', "(X+Y+1)^6: \"%s\"", out);
        CHECK(ex("(X+Y+1)^2", out) == CASIM_OK, "(X+Y+1)^2: not OK");
        t0 = clock();
        CHECK(ex("(X+1)^1000", out) == CASIM_TOO_LONG, "(X+1)^1000: \"%s\"", out);
        CHECK(ex("(X+Y+Z+1)^30×(X-1)^30", out) == CASIM_TOO_LONG, "a large product: \"%s\"", out);
        CHECK((double)(clock() - t0) / CLOCKS_PER_SEC < 0.05, "the term bound did not stop them early");
    }
    /* the CAS 003 review: an overflowing multinomial count is TOO_LONG, never OK with empty text */
    {
        char out[OUT];
        static const char *const huge[] = {"(A+B+C+D+E+F+G+H+I+J+K+L)^4000", "(A+B+C+D+E+F+G+H+I+J+K+L)^4000×Y+1",
                                           "SIN((A+B+C+D+E+F+G+H+I+J+K+L)^4000)", "(A+B+C+D+E+F+G+H+I+J+K+L)^(-4000)"};
        for (size_t i = 0; i < sizeof huge / sizeof huge[0]; i++)
            CHECK(ex(huge[i], out) == CASIM_TOO_LONG && out[0] == '\0', "%s: \"%s\", want TOO_LONG", huge[i], out);
    }
    /* the review: products made while distributing are expanded again (a fixed point) */
    pair("(SQRT(X+1)×Y+1)×(SQRT(X+1)+1)", "SQRT(X+1)+Y×SQRT(X+1)+Y×X+Y+1");
    pair("(X÷(Y+1)+1)×(X÷(Y-1)+1)", "X^2÷(Y^2-1)+X÷(Y+1)+X÷(Y-1)+1");
    pair("(1÷(Y+1)+X)^2", "1÷(Y^2+2×Y+1)+2×X÷(Y+1)+X^2");
    /* like terms collect; L=R each side */
    pair("(X+1)^2-(X-1)^2", "4×X");
    pair("X×(X+1)=(X+1)^2", "X^2+X=X^2+2×X+1");
}

/* ---- the dense polynomial instrument ---- */

typedef long long poly[D][D];   /* p[i][j] is the coefficient of X^i×Y^j */

static void p_zero(poly p)
{
    memset(p, 0, sizeof(poly));
}

static bool p_mul(poly r, poly a, poly b)
{
    poly t;
    p_zero(t);
    for (int i = 0; i < D; i++)
        for (int j = 0; j < D; j++)
            if (a[i][j])
                for (int k = 0; k < D; k++)
                    for (int l = 0; l < D; l++)
                        if (b[k][l]) {
                            if (i + k >= D || j + l >= D) return false;
                            t[i + k][j + l] += a[i][j] * b[k][l];
                        }
    memcpy(r, t, sizeof(poly));
    return true;
}

static void put(char *o, const char *s)
{
    if (strlen(o) + strlen(s) < OUT) strcat(o, s);
}

/* A monomial's text, its factors (coefficient, X^i, Y^j) in an order of `ord`. */
static void monomial(char *o, long long c, int i, int j, unsigned ord)
{
    char f[3][40];
    int n = 0, idx[3] = {0, 1, 2};
    snprintf(f[n++], sizeof f[0], c < 0 ? "(%lld)" : "%lld", c);
    if (i) snprintf(f[n++], sizeof f[0], i == 1 ? "X" : "X^%d", i);
    if (j) snprintf(f[n++], sizeof f[0], j == 1 ? "Y" : "Y^%d", j);
    for (int k = n - 1; k > 0; k--) {
        int r = (int)(ord % (unsigned)(k + 1)), x = idx[k];
        ord /= (unsigned)(k + 1);
        idx[k] = idx[r], idx[r] = x;
    }
    for (int k = 0; k < n; k++) {
        if (k) put(o, "×");
        put(o, f[idx[k]]);
    }
}

/* A random sum of 1-3 monomials: its text, and its polynomial. */
static void gen_sum(char *o, poly p)
{
    int n = 1 + (int)rnd(3);
    p_zero(p);
    put(o, "(");
    for (int t = 0; t < n; t++) {
        long long c = (long long)rnd(7) - 3;
        int i = (int)rnd(3), j = (int)rnd(3);
        if (!c) c = 1;
        if (t) put(o, "+");
        monomial(o, c, i, j, rnd(6));
        p[i][j] += c;
    }
    put(o, ")");
}

/* B: the polynomial's terms, shuffled. */
static void print_poly(char *o, poly p)
{
    int ti[D * D], tj[D * D], n = 0;
    for (int i = 0; i < D; i++)
        for (int j = 0; j < D; j++)
            if (p[i][j]) ti[n] = i, tj[n] = j, n++;
    for (int k = n - 1; k > 0; k--) {
        int r = (int)rnd((unsigned)k + 1), x = ti[k], y = tj[k];
        ti[k] = ti[r], tj[k] = tj[r], ti[r] = x, tj[r] = y;
    }
    if (n == 0) put(o, "0");
    for (int k = 0; k < n; k++) {
        if (k) put(o, "+");
        monomial(o, p[ti[k]][tj[k]], ti[k], tj[k], rnd(6));
    }
}

static void instrument(void)
{
    int judged = 0;
    for (int c = 0; c < 1500; c++) {
        char a[OUT] = "", b[OUT] = "", wa[OUT], wb[OUT];
        poly acc, s;
        int kind = (int)rnd(3);
        bool ok = true;
        if (kind == 0) {                    /* a product of 2-3 sums */
            int n = 2 + (int)rnd(2);
            gen_sum(a, acc);
            for (int k = 1; k < n && ok; k++) {
                put(a, "×");
                gen_sum(a, s);
                ok = p_mul(acc, acc, s);
            }
        } else if (kind == 1) {             /* a sum to the 2nd or 3rd power */
            int e = 2 + (int)rnd(2);
            char t[16];
            poly base;
            gen_sum(a, base);
            snprintf(t, sizeof t, "^%d", e);
            put(a, t);
            memcpy(acc, base, sizeof(poly));
            for (int k = 1; k < e && ok; k++) ok = p_mul(acc, acc, base);
        } else {                            /* a sum to a power, times a sum */
            poly base;
            gen_sum(a, base);
            put(a, "^2×");
            gen_sum(a, s);
            ok = p_mul(acc, base, base) && p_mul(acc, acc, s);
        }
        if (!ok) continue;
        print_poly(b, acc);
        if (strlen(b) > 180) continue;      /* abacus keeps every B under 190 bytes */
        judged++;
        pair(a, b);
        /* inside a function's argument, and under a ÷ */
        snprintf(wa, sizeof wa, "SIN(%s)", a);
        snprintf(wb, sizeof wb, "SIN(%s)", b);
        if (strlen(wb) < 185) pair(wa, wb);
        snprintf(wa, sizeof wa, "Z÷(%s)", a);
        snprintf(wb, sizeof wb, "Z÷(%s)", b);
        if (strlen(wb) < 185) pair(wa, wb);
    }
    printf("expand: %d instrument cases judged\n", judged);
    CHECK(judged > 500, "only %d instrument cases judged", judged);
}

static void visible(void)
{
    FILE *fp = fopen("work/003-examples.txt", "r");
    char line[4096];
    int n = 0;
    CHECK(fp != NULL, "work/003-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[8], *s = line;
        int k = 0;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 8) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 7 || strcmp(f[1], "expand") != 0) continue;
        n++;
        pair(f[2], f[3]);
        {
            abn got, want = ce_num(f[6]);
            char out[OUT];
            ce_angle(CASIM_RAD);
            for (char *as = strtok(f[5], ";"); as; as = strtok(NULL, ";"))
                if (as[0] >= 'A' && as[0] <= 'Z' && as[1] == '=') ce_set(as[0], as + 2);
            CHECK(ex(f[2], out) == CASIM_OK && ce_eval(out, &got) == ABN_OK && ce_close(&got, &want, -24),
                  "%s: %s at its point", f[0], out);
        }
    }
    fclose(fp);
    CHECK(n == 16, "%d visible E cases, want 16", n);
}

/* Every arena size from 0 to 8192 gives NO_ROOM (TOO_LONG before CAS 004) or the one answer, for expand and collect: the arena
   is compacted between steps (copied, slid down, references rewritten), so a size that runs out mid-copy
   must still fail cleanly. */
static void starved(void)
{
    static const char *const in[] = {
        "(X+1)×(Y-2)+(X+3)×(Y-4)+(X+5)×(Y-6)+SIN((X+1)^2)",
        "X÷((X+1)×(X-1))+(X+Y)^3",
        "(X+Y+1)^2×(X-1)",
        "IP((X+1)^2)+RAND×(X+1)+(X+2)!",
    };
    for (size_t i = 0; i < sizeof in / sizeof in[0]; i++) {
        char want[2][OUT], got[OUT];
        int bad = 0;
        casim_status s0 = casim_expand(in[i], CASIM_RAD, want[0], OUT, arena, sizeof arena);
        casim_status s1 = casim_collect(in[i], 'X' - 'A', CASIM_RAD, want[1], OUT, arena, sizeof arena);
        CHECK(s0 == CASIM_OK && s1 == CASIM_OK, "%s: %d %d with the full arena", in[i], (int)s0, (int)s1);
        for (size_t n = 0; n <= 8192; n++) {
            casim_status a = casim_expand(in[i], CASIM_RAD, got, OUT, arena, n);
            if (!((a == CASIM_NO_ROOM && got[0] == '\0') || (a == CASIM_OK && strcmp(got, want[0]) == 0))) bad++;
            a = casim_collect(in[i], 'X' - 'A', CASIM_RAD, got, OUT, arena, n);
            if (!((a == CASIM_NO_ROOM && got[0] == '\0') || (a == CASIM_OK && strcmp(got, want[1]) == 0))) bad++;
        }
        CHECK(bad == 0, "%s: %d arena sizes gave another answer", in[i], bad);
    }
}

int main(void)
{
    ce_init();
    starved();
    rules();
    instrument();
    visible();
    return check_done();
}
