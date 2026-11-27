/* Unit CAS 009: exact definite integrals (work/009-definite-integrals.md). Exact texts (by hand: F(hi) - F(lo) with
 * CAS 008's values); the refusals (IMPROPER, BAD_ARG, UNDEFINED, integrate's own); the stamp; work/009-examples.txt;
 * and seeded integrands with seeded exact bounds in all three modes: every printed answer equals a Simpson quadrature
 * of the integrand's core values (a coarse 1E-7: abacus judges at 1E-24), and is printed only when this file's own
 * pole analysis, independent of casim's, finds no pole or domain edge on the closed interval. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];
static const char *const MODE[3] = {"DEG", "RAD", "GRAD"};

static casim_status defint(const char *f, const char *lo, const char *hi, int angle, int model, char *out, size_t cap)
{
    casim_req rq = {CASIM_DEFINT, f, model, 'X' - 'A', lo, 0, angle, hi};
    return casim_run(&rq, out, OUT, arena, cap);
}

static bool num_of(const char *text, double *v)
{
    abn r;
    if (ce_eval(text, &r) != ABN_OK) return false;
    *v = strtod(ce_text(&r), NULL);
    return isfinite(*v);
}

/* tanh-sinh quadrature on the core's values of f over [a, b] (it converges where an endpoint's derivative is
   infinite: SQRT(X) on [0, 4], which Simpson's rule only reaches to 1E-4); false if f has no value at a node */
static bool simpson(const char *f, double a, double b, double *s)
{
    const double h = 1.0 / 64, halfpi = 1.5707963267948966;
    double c = (a + b) / 2, d = (b - a) / 2, acc = 0;
    if (a == b) return *s = 0, true;
    for (int k = -256; k <= 256; k++) {
        double t = k * h, u = halfpi * sinh(t), ch = cosh(u), x = c + d * tanh(u), w = halfpi * cosh(t) / (ch * ch), v;
        char xs[64];
        abn r;
        if (!(fabs(x - c) < fabs(d)) || w < 1e-300) continue;     /* the endpoints themselves are never sampled */
        snprintf(xs, sizeof xs, "%.17g", x);
        ce_set('X', xs);
        if (ce_eval(f, &r) != ABN_OK) return false;
        v = strtod(ce_text(&r), NULL);
        acc += w * v;
    }
    *s = acc * h * d;
    return isfinite(*s);
}

/* the answer equals the quadrature: out evaluated, against tanh-sinh on [lo, hi] (A = 1.7). An answer past a
   double (EXP(901)) can't be judged here: counted apart, and abacus judges it at 60 digits */
static int unjudged;
static bool by_value(const char *f, const char *lo, const char *hi, int angle, const char *out)
{
    double a, b, want, got;
    abn g;
    ce_angle(angle);
    ce_set('A', "1.7");
    if (ce_eval(out, &g) == ABN_OK && !isfinite(strtod(ce_text(&g), NULL))) return unjudged++, true;
    if (!num_of(lo, &a) || !num_of(hi, &b) || !num_of(out, &got) || !simpson(f, a, b, &want)) return false;
    if (!isfinite(want) || fabs(want) > 1e300) return unjudged++, true;
    return fabs(got - want) <= 1e-7 * (1 + fabs(want));
}

static void is(const char *f, const char *lo, const char *hi, int angle, const char *want)
{
    char out[OUT];
    casim_status st = defint(f, lo, hi, angle, CASIM_MODEL_35, out, sizeof arena);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "defint %s on [%s, %s] (%s): %d \"%s\", want \"%s\"", f, lo, hi,
          MODE[angle], (int)st, out, want);
    if (st == CASIM_OK && strcmp(out, want) == 0)
        CHECK(by_value(f, lo, hi, angle, out), "defint %s on [%s, %s] (%s) = %s: not the quadrature", f, lo, hi, MODE[angle],
              out);
}

static void refused(const char *f, const char *lo, const char *hi, int angle, casim_status want)
{
    char out[OUT];
    casim_status st = defint(f, lo, hi, angle, CASIM_MODEL_35, out, sizeof arena);
    CHECK(st == want && out[0] == '\0', "defint %s on [%s, %s] (%s): %d \"%s\", want status %d", f, lo, hi, MODE[angle],
          (int)st, out, (int)want);
}

static void exact(void)
{
    is("SIN(X)", "0", "π", CASIM_RAD, "2");
    is("SIN(X)", "0", "180", CASIM_DEG, "360÷π");
    is("SIN(X)", "0", "200", CASIM_GRAD, "400÷π");
    is("1÷X", "1", "3", CASIM_RAD, "LN(3)");
    is("1÷X", "-3", "-1", CASIM_RAD, "-LN(3)");
    is("SQRT(X)", "0", "4", CASIM_RAD, "16÷3");
    is("EXP(2×X)", "0", "LN(2)", CASIM_RAD, "3÷2");
    is("X^2", "0", "1", CASIM_RAD, "1÷3");
    is("X^2", "1", "0", CASIM_RAD, "-1÷3");
    is("X^2", "2", "2", CASIM_RAD, "0");
    is("SQRT(X)", "0", "0", CASIM_RAD, "0");                         /* rule 6a: an allowed edge, lo = hi */
    is("TAN(X)", "0", "π÷4", CASIM_RAD, "-LN(SQRT(2)÷2)");
    is("A×X", "0", "2", CASIM_RAD, "2×A");
    is("COS(X)", "0", "π÷2", CASIM_RAD, "1");
    is("3×X^2+2×X+1", "-1", "2", CASIM_RAD, "15");
    is("1÷(2×X+1)", "0", "1", CASIM_RAD, "LN(3)÷2");
    is("X^(1÷3)", "0", "8", CASIM_RAD, "12");
    is("1÷X^2", "1", "2", CASIM_RAD, "1÷2");
    is("EXP(X)", "0", "1", CASIM_RAD, "EXP(1)-1");
    is("SIN(2×X)", "0", "π÷4", CASIM_RAD, "1÷2");
    is("TAN(X)", "120", "240", CASIM_DEG, "0");                     /* between the poles at 90 and 270, odd about 180 */
    is("SQRT(3×X-1)", "1÷3", "17÷3", CASIM_RAD, "128÷9");          /* the edge exactly at a rational endpoint: allowed (u = 16 at 17/3) */
}

/* an answer (OK) that equals the quadrature: the text is not pinned (COS(-89) stays as written) */
static void answered(const char *f, const char *lo, const char *hi, int angle)
{
    char out[OUT];
    casim_status st = defint(f, lo, hi, angle, CASIM_MODEL_35, out, sizeof arena);
    CHECK(st == CASIM_OK && by_value(f, lo, hi, angle, out), "defint %s on [%s, %s] (%s): %d \"%s\", want an answer of its value",
          f, lo, hi, MODE[angle], (int)st, out);
}

/* TAN's poles placed exactly in DEG and GRAD (abacus #5577): intervals that just miss 90 + 180k and 100 + 200k are
   answered; one that holds only the next pole is refused (a sparse placement, period 360 or 400, would miss it) */
static void tan_poles(void)
{
    answered("TAN(X)", "91", "269", CASIM_DEG);
    answered("TAN(X)", "-89", "89", CASIM_DEG);
    answered("TAN(2×X)", "46", "134", CASIM_DEG);
    answered("TAN(X)", "101", "299", CASIM_GRAD);
    answered("TAN(X)", "-99", "99", CASIM_GRAD);
    refused("TAN(X)", "100", "280", CASIM_DEG, CASIM_IMPROPER);     /* 270, not 90 */
    refused("TAN(X)", "101", "350", CASIM_GRAD, CASIM_IMPROPER);    /* 300, not 100 */
    refused("TAN(X)", "-280", "-100", CASIM_DEG, CASIM_IMPROPER);   /* -270 */
}

static void refusals(void)
{
    /* IMPROPER: a pole or domain edge on the closed interval */
    refused("1÷X", "-1", "1", CASIM_RAD, CASIM_IMPROPER);
    refused("1÷X", "0", "1", CASIM_RAD, CASIM_IMPROPER);
    refused("1÷X", "1", "0", CASIM_RAD, CASIM_IMPROPER);
    refused("1÷SQRT(X)", "0", "1", CASIM_RAD, CASIM_IMPROPER);
    refused("SQRT(X)", "-1", "1", CASIM_RAD, CASIM_IMPROPER);
    refused("X^(1÷3)", "-8", "1", CASIM_RAD, CASIM_IMPROPER);        /* the core: no real (-8)^(1÷3) */
    refused("TAN(X)", "0", "π÷2", CASIM_RAD, CASIM_IMPROPER);
    refused("TAN(X)", "0", "90", CASIM_DEG, CASIM_IMPROPER);
    refused("TAN(X)", "0", "100", CASIM_GRAD, CASIM_IMPROPER);
    refused("TAN(X)", "2", "5", CASIM_RAD, CASIM_IMPROPER);          /* 3π/2 inside */
    refused("TAN(X)", "100", "280", CASIM_DEG, CASIM_IMPROPER);      /* 270 inside */
    refused("1÷(2×X-1)", "0", "1", CASIM_RAD, CASIM_IMPROPER);
    refused("X^(-2)", "-1", "1", CASIM_RAD, CASIM_IMPROPER);
    refused("1÷(3×X-1)", "1÷3", "1", CASIM_RAD, CASIM_IMPROPER);    /* the pole exactly at a rational endpoint */
    /* rule 6a: lo = hi on a pole or edge is IMPROPER (the closed interval is the point); an allowed edge gives 0 */
    refused("1÷X", "0", "0", CASIM_RAD, CASIM_IMPROPER);
    refused("1÷SQRT(X)", "0", "0", CASIM_RAD, CASIM_IMPROPER);
    refused("SQRT(X)", "-1", "-1", CASIM_RAD, CASIM_IMPROPER);
    refused("TAN(X)", "90", "90", CASIM_DEG, CASIM_IMPROPER);
    /* BAD_ARG: a bound with a letter; a pole that another letter places */
    refused("X^2", "0", "X", CASIM_RAD, CASIM_BAD_ARG);
    refused("X^2", "B", "1", CASIM_RAD, CASIM_BAD_ARG);
    /* rule 2 first: CAS 006 refuses a letter in a linear argument, so 1÷(A×X+1) never reaches the interval check
       (a departure from the unit's example list, reported) */
    refused("1÷(A×X+1)", "0", "1", CASIM_RAD, CASIM_NOT_INTEGRABLE);
    refused("X^2=1", "0", "1", CASIM_RAD, CASIM_BAD_ARG);
    /* UNDEFINED: a bound with no value */
    refused("X^2", "LN(0)", "1", CASIM_RAD, CASIM_UNDEFINED);
    refused("X^2", "0", "1÷0", CASIM_RAD, CASIM_UNDEFINED);
    /* integrate's own refusal, passed through */
    refused("X×LN(X)", "1", "2", CASIM_RAD, CASIM_NOT_INTEGRABLE);
}

static void stamp(void)
{
    char out[OUT];
    /* X^2^3: STU X^8, so 1÷9 on [0, 1]; 35 (X^2)^3 = X^6, 1÷7 */
    CHECK(defint("X^2^3", "0", "1", CASIM_RAD, CASIM_MODEL_STU, out, sizeof arena) == CASIM_OK && strcmp(out, "1÷9") == 0,
          "STU: \"%s\"", out);
    CHECK(defint("X^2^3", "0", "1", CASIM_RAD, CASIM_MODEL_35, out, sizeof arena) == CASIM_OK && strcmp(out, "1÷7") == 0,
          "35: \"%s\"", out);
    /* the bounds are read as the integrand's model: 2^3^2 is 512 in STU, 64 in 35 */
    CHECK(defint("1", "0", "2^3^2", CASIM_RAD, CASIM_MODEL_STU, out, sizeof arena) == CASIM_OK && strcmp(out, "512") == 0,
          "STU bound: \"%s\"", out);
    CHECK(defint("1", "0", "2^3^2", CASIM_RAD, CASIM_MODEL_35, out, sizeof arena) == CASIM_OK && strcmp(out, "64") == 0,
          "35 bound: \"%s\"", out);
}

/* The test's own pole analysis, in doubles, independent of casim's: whether a pole or domain edge of the
   generated integrand lies on [a, b] (or within 1E-9 of it: casim may refuse near an edge) */
typedef struct {
    int kind;           /* 0 none, 1 u = 0 is a pole, 2 u < 0 is outside, 3 TAN */
    double p, q;        /* u = p X + q */
} hazard;

static bool on_interval(const hazard *h, double a, double b, int angle, bool *near)
{
    double lo = a < b ? a : b, hi = a < b ? b : a, ua = h->p * lo + h->q, ub = h->p * hi + h->q, umin = ua < ub ? ua : ub,
           umax = ua < ub ? ub : ua, tol = 1e-9 * (1 + fabs(umin) + fabs(umax));
    *near = false;
    if (h->kind == 1) {
        if (umin < -tol && umax > tol) return true;
        if (fabs(umin) <= tol || fabs(umax) <= tol) return *near = true, true;
        return false;
    }
    if (h->kind == 2) {
        if (umin < -tol) return true;
        if (umin < tol && umin != 0) *near = true;
        return false;
    }
    if (h->kind == 3) {
        double half = angle == CASIM_RAD ? 3.14159265358979323846 / 2 : angle == CASIM_DEG ? 90 : 100, per = 2 * half;
        double k = ceil((umin - half) / per - 1e-12), pole = half + k * per;
        if (pole <= umax + tol) {
            if (fabs(pole - umin) <= tol || fabs(pole - umax) <= tol) *near = true;
            return true;
        }
    }
    return false;
}

static unsigned seed = 9009;
static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (unsigned)n);
}

static void seeded(void)
{
    static const char *const BOUND_ANY[] = {"0", "1", "-1", "2", "-2", "3", "0.5", "-1.5", "1÷3", "SQRT(2)", "LN(2)",
                                            "-SQRT(3)", "EXP(1)"};
    static const char *const BOUND_RAD[] = {"π", "π÷4", "π÷6", "-π÷3", "2×π÷3", "π÷2", "-π"};
    static const char *const BOUND_DEG[] = {"30", "45", "90", "-60", "180", "135", "270"};
    static const char *const BOUND_GRAD[] = {"50", "100", "-100÷3", "200", "150", "300"};
    int judged = 0, improper = 0, near = 0;
    for (int t = 0; t < 700; t++) {
        char f[96], lo[32], hi[32], out[OUT];
        int angle = rnd(3), a = 1 + rnd(3), b = rnd(7) - 3, n = rnd(5) + 1;
        hazard h = {0, a, b};
        casim_status st;
        double va, vb;
        bool nr, hit;
        switch (rnd(7)) {
        case 0: snprintf(f, sizeof f, "%d×X^%d+%d", 1 + rnd(4), n, rnd(9) - 4); break;
        case 1: snprintf(f, sizeof f, "1÷(%d×X%+d)", a, b), h.kind = 1; break;
        case 2: snprintf(f, sizeof f, "SQRT(%d×X%+d)", a, b), h.kind = 2; break;
        case 3: snprintf(f, sizeof f, "(%d×X%+d)^(-%d)", a, b, 1 + rnd(2) + 1), h.kind = 1; break;
        case 4: snprintf(f, sizeof f, "TAN(%d×X%+d)", a, b), h.kind = 3; break;
        case 5: snprintf(f, sizeof f, "%s(%d×X%+d)", rnd(2) ? "SIN" : "COS", a, b); break;
        default: snprintf(f, sizeof f, "EXP(%d×X%+d)÷%d", a, b, 1 + rnd(3)); break;
        }
        for (int k = 0; k < 2; k++) {
            char *d = k ? hi : lo;
            const char *src = rnd(2) ? BOUND_ANY[rnd(13)]
                            : angle == CASIM_RAD ? BOUND_RAD[rnd(7)] : angle == CASIM_DEG ? BOUND_DEG[rnd(7)] : BOUND_GRAD[rnd(6)];
            snprintf(d, 32, "%s", src);
        }
        ce_angle(angle);
        if (!num_of(lo, &va) || !num_of(hi, &vb)) continue;
        st = defint(f, lo, hi, angle, CASIM_MODEL_35, out, sizeof arena);
        hit = on_interval(&h, va, vb, angle, &nr);
        if (st == CASIM_IMPROPER) {
            improper++;
            if (nr) near++;
            CHECK(hit || nr, "defint %s on [%s, %s] (%s): IMPROPER, but no pole or edge is on or near it", f, lo, hi,
                  MODE[angle]);
            continue;
        }
        if (st == CASIM_TOO_LONG || st == CASIM_NOT_INTEGRABLE) continue;
        CHECK(st == CASIM_OK, "defint %s on [%s, %s] (%s): status %d", f, lo, hi, MODE[angle], (int)st);
        if (st != CASIM_OK) continue;
        CHECK(!hit, "defint %s on [%s, %s] (%s) = %s: printed across a pole or edge", f, lo, hi, MODE[angle], out);
        judged++;
        CHECK(by_value(f, lo, hi, angle, out), "defint %s on [%s, %s] (%s) = %s: not the quadrature", f, lo, hi, MODE[angle],
              out);
    }
    printf("defint: %d seeded answers judged by quadrature (%d past a double), %d IMPROPER (%d near an edge)\n", judged,
           unjudged, improper, near);
    CHECK(judged > 300 && improper > 40, "%d judged, %d improper", judged, improper);
}

/* work/009-examples.txt at the 8,800 B arena */
static void visible(void)
{
    static unsigned char small[8800];
    FILE *fp = fopen("work/009-examples.txt", "r");
    char line[1024];
    int n = 0;
    CHECK(fp != NULL, "work/009-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[9], *s = line, out[OUT];
        int k = 0, angle;
        casim_status st;
        const char *name;
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 9) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 8) continue;
        n++;
        angle = strcmp(f[5], "DEG") == 0 ? CASIM_DEG : strcmp(f[5], "GRAD") == 0 ? CASIM_GRAD : CASIM_RAD;
        {
            casim_req rq = {CASIM_DEFINT, f[2], strcmp(f[6], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35, 'X' - 'A', f[3], 0,
                            angle, f[4]};
            st = casim_run(&rq, out, OUT, small, sizeof small);
        }
        name = st == CASIM_OK ? out : st == CASIM_IMPROPER ? "IMPROPER" : st == CASIM_BAD_ARG ? "BAD_ARG"
             : st == CASIM_UNDEFINED ? "UNDEFINED" : st == CASIM_NOT_INTEGRABLE ? "NOT_INTEGRABLE" : st == CASIM_NO_ROOM ? "NO_ROOM"
             : "another status";
        CHECK(strcmp(name, f[7]) == 0, "%s: defint %s on [%s, %s] (%s): \"%s\", want \"%s\"", f[0], f[2], f[3], f[4], f[5], name,
              f[7]);
        if (st == CASIM_OK && strcmp(name, f[7]) == 0 && strcmp(f[6], "STU") != 0)
            CHECK(by_value(f[2], f[3], f[4], angle, out), "%s: %s not the quadrature", f[0], out);
    }
    fclose(fp);
    CHECK(n >= 25, "%d visible examples", n);
}

/* CAS 044 (work/044-trig-definite-integrals.md, abacus #17449): polynomials in SIN and COS through CAS 042's form,
   after the table refuses; each text exactly, each value against the core's quadrature; the refusals; and a seeded draw
   of products and sums over bounds at π's multiples and plain numbers in each mode, every value by quadrature */
static unsigned seed044 = 44;
static void trig044(void)
{
    static const char *const FS[] = {"SIN(X)^2", "COS(X)^4", "SIN(X)^3×COS(X)^2", "SIN(X)×SIN(3×X)", "COS(2×X+1)^2",
                                     "SIN(X)^5", "3×SIN(X)^2+COS(X)", "SIN(X)×COS(X)"};
    static const char *const BR[][2] = {{"0", "π"}, {"0", "π÷2"}, {"0", "π÷6"}, {"π÷4", "π÷3"}, {"0", "1"}, {"-1", "2"},
                                        {"π", "0"}, {"-π÷3", "π÷4"}};
    static const char *const BD[][2] = {{"0", "180"}, {"30", "60"}, {"-45", "45"}, {"0", "1"}};
    static const char *const BG[][2] = {{"0", "200"}, {"50", "150"}, {"0", "1"}};
    int judged = 0;
    is("SIN(X)^2", "0", "π", CASIM_RAD, "π÷2");
    is("COS(X)^4", "0", "π÷2", CASIM_RAD, "3×π÷16");
    is("SIN(X)^3", "0", "π", CASIM_RAD, "4÷3");
    is("SIN(X)×SIN(3×X)", "0", "π", CASIM_RAD, "0");
    is("SIN(X)^2", "0", "π÷6", CASIM_RAD, "π÷12-SQRT(3)÷8");
    is("SIN(X)^2", "0", "1", CASIM_RAD, "-SIN(2)÷4+1÷2");
    is("SIN(X)^5", "0", "π÷3", CASIM_RAD, "53÷480");
    is("SIN(X)^2", "0", "180", CASIM_DEG, "90");
    is("SIN(X)^2", "0", "45", CASIM_DEG, "-45÷π+45÷2");
    is("SIN(X)^2", "0", "200", CASIM_GRAD, "100");
    is("A×SIN(X)^2", "0", "π", CASIM_RAD, "A×π÷2");                   /* letters with number bounds (ruling 1) */
    is("SIN(X)^2÷A", "0", "π", CASIM_RAD, "π÷2÷A");
    is("SIN(X)^3", "-π", "π", CASIM_RAD, "0");                         /* the table's bounds see the 0 */
    is("SIN(X)^3", "-1", "1", CASIM_RAD, "COS(-1)-COS(1)-COS(-1)^3÷3+COS(1)^3÷3");     /* Not yet: a 0 not seen */
    is("SIN(X)", "0", "π", CASIM_RAD, "2");                            /* the table's, as at 1.10.0 */
    refused("COS(X)^2", "A", "B", CASIM_RAD, CASIM_BAD_ARG);           /* a letter bound (ruling 3) */
    refused("SIN(X)^2×X÷X", "-1", "1", CASIM_RAD, CASIM_NOT_INTEGRABLE);   /* CAS 017 rule 2's hole */
    refused("X×SIN(X)^2", "0", "π", CASIM_RAD, CASIM_NOT_INTEGRABLE);
    refused("TAN(X)^2", "0", "1", CASIM_RAD, CASIM_NOT_INTEGRABLE);
    for (int m = 0; m < 3; m++) {
        int angle = m == 0 ? CASIM_RAD : m == 1 ? CASIM_DEG : CASIM_GRAD, nb = m == 0 ? 8 : m == 1 ? 4 : 3;
        for (size_t i = 0; i < sizeof FS / sizeof FS[0]; i++)
            for (int j = 0; j < nb; j++) {
                const char *lo = m == 0 ? BR[j][0] : m == 1 ? BD[j][0] : BG[j][0], *hi = m == 0 ? BR[j][1] : m == 1 ? BD[j][1] : BG[j][1];
                char out[OUT];
                casim_status st = defint(FS[i], lo, hi, angle, CASIM_MODEL_35, out, 16384);
                CHECK(st == CASIM_OK, "defint %s on [%s, %s] (%s): %d", FS[i], lo, hi, MODE[angle], (int)st);
                if (st != CASIM_OK) continue;
                judged++;
                CHECK(by_value(FS[i], lo, hi, angle, out), "defint %s on [%s, %s] (%s) = %s: not the quadrature", FS[i], lo,
                      hi, MODE[angle], out);
            }
    }
    for (int t = 0; t < 60; t++) {                                     /* drawn: products of SIN and COS of m×X */
        char f[160] = "", out[OUT];
        int terms = 1 + (int)((seed044 = seed044 * 1103515245u + 12345u) >> 16) % 2, angle = CASIM_RAD;
        for (int i = 0; i < terms; i++) {
            size_t L = strlen(f);
            int k = 1 + (int)((seed044 = seed044 * 1103515245u + 12345u) >> 16) % 4;
            int m1 = 1 + (int)((seed044 = seed044 * 1103515245u + 12345u) >> 16) % 3;
            int e1 = 1 + (int)((seed044 = seed044 * 1103515245u + 12345u) >> 16) % 3;
            int m2 = 1 + (int)((seed044 = seed044 * 1103515245u + 12345u) >> 16) % 3;
            snprintf(f + L, sizeof f - L, "%s%d×SIN(%d×X)^%d×COS(%d×X)", i ? "+" : "", k, m1, e1, m2);
        }
        if (defint(f, "-π÷6", "π÷4", angle, CASIM_MODEL_35, out, 16384) != CASIM_OK) {
            CHECK(false, "defint %s on [-π÷6, π÷4]: %s", f, out);
            continue;
        }
        judged++;
        CHECK(by_value(f, "-π÷6", "π÷4", angle, out), "defint %s on [-π÷6, π÷4] = %s: not the quadrature", f, out);
    }
    printf("defint: CAS 044, %d SIN and COS polynomials judged by value\n", judged);
    CHECK(judged >= 180, "CAS 044: %d judged", judged);
}

/* CAS 045 (work/045-typed-domain-positive.md, abacus #18154): a typed condition u > 0 or u >= 0 whose canonical u is
   EXP, ALOG, COSH or a letter-free positive base to a power holds everywhere, so LN(EXP(u)) and SQRT(EXP(K))^2 are
   answered, not refused IMPROPER or BAD_ARG; the true refusals beside them stay */
static void td045(void)
{
    is("COS(LN(EXP(X+1)))", "1", "2", CASIM_RAD, "-SIN(2)+SIN(3)");    /* 1.11.0's four */
    is("LN(EXP(SIN(X)))", "0", "2", CASIM_RAD, "-COS(2)+1");
    is("LN(EXP(SIN(X+1)))", "2", "3", CASIM_RAD, "COS(3)-COS(4)");
    is("LN(EXP(COS(X+1)))", "-1", "0.5", CASIM_RAD, "SIN(1.5)");
    is("LN(EXP(SIN(X)))", "2", "0", CASIM_RAD, "COS(2)-1");
    is("LN(EXP(SIN(X)))", "0", "90", CASIM_DEG, "180÷π");
    is("LN(EXP(SIN(X)))", "0", "100", CASIM_GRAD, "200÷π");
    is("LN(EXP(SIN(X)))^3", "0", "π", CASIM_RAD, "4÷3");
    is("SIN(X)^2+LN(EXP(X))-X", "0", "1", CASIM_RAD, "-SIN(2)÷4+1÷2");
    is("LOG(ALOG(SIN(X)))", "0", "π", CASIM_RAD, "2");                 /* ALOG */
    is("LN(EXP(K))×SIN(X)", "0", "1", CASIM_RAD, "K-K×COS(1)");         /* a letter's (ruling 1) */
    is("LN(EXP(K))×X", "0", "1", CASIM_RAD, "K÷2");                     /* the table's path */
    is("LN(EXP(K×SIN(X)))", "0", "1", CASIM_RAD, "K-K×COS(1)");
    is("SQRT(EXP(K))^2×SIN(X)", "0", "1", CASIM_RAD, "EXP(K)-EXP(K)×COS(1)");      /* u >= 0 */
    is("SQRT(2^K)^2×SIN(X)", "0", "1", CASIM_RAD, "2^K-COS(1)×2^K");               /* a positive base */
    refused("SQRT(K)^2×SIN(X)", "0", "1", CASIM_RAD, CASIM_BAD_ARG);   /* K < 0: no value */
    refused("SQRT(K^K)^2×SIN(X)", "0", "1", CASIM_RAD, CASIM_BAD_ARG); /* a letter's base */
    refused("SQRT((-2)^K)^2×SIN(X)", "0", "1", CASIM_RAD, CASIM_BAD_ARG);
    refused("LN(EXP(SIN(X)))", "A", "2", CASIM_RAD, CASIM_BAD_ARG);     /* a letter bound */
    refused("1÷(X-1)", "0", "2", CASIM_RAD, CASIM_IMPROPER);
    refused("TAN(X)", "1", "2", CASIM_RAD, CASIM_IMPROPER);
    refused("TAN(X)×COS(X)", "1", "2", CASIM_RAD, CASIM_NOT_INTEGRABLE);   /* CAS 017 rule 2, before any interval */
    refused("LN(EXP(TAN(X)))×COS(X)", "1", "2", CASIM_RAD, CASIM_NOT_INTEGRABLE);
}

int main(void)
{
    ce_init();
    exact();
    refusals();
    tan_poles();
    stamp();
    seeded();
    visible();
    trig044();
    td045();
    return check_done();
}
