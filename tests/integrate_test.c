/* Unit CAS 006: simple indefinite integrals (work/006-integrals.md, with abacus #5120, #5124). Each answer's
 * exact text against 002's canonical form of an antiderivative written by hand (casim simplify, the accepted
 * contract); each answer by value on the core: its numeric derivative against the integrand, and F(x1)-F(x2)
 * against a quadrature of the integrand (which catches a dropped ABS on the negative side); the refusals; the
 * angle factors; the stamp; no "+C"; the visible examples. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static const int ANGLES[3] = {CASIM_DEG, CASIM_RAD, CASIM_GRAD};

static casim_status integ(const char *f, int angle, int model, char *out, size_t arena_cap)
{
    casim_req rq = {CASIM_INTEGRATE, f, model, 'X' - 'A', NULL, 0, angle, NULL};
    return casim_run(&rq, out, OUT, arena, arena_cap);
}

/* The core's value of text at X = x (angle as set), as a double; false on an error. */
static bool at(const char *text, const char *x, double *v)
{
    abn r;
    ce_set('X', x);
    if (ce_eval(text, &r) != ABN_OK) return false;
    *v = strtod(ce_text(&r), NULL);
    return true;
}

/* F is an antiderivative of f, by value: F' (central difference) = f at three points, and F(b) - F(a) = the
   integral of f from a to b (Simpson, 400 panels) on two intervals, one on each side of 0 where both are
   defined. */
static bool by_value(const char *f, const char *F, int angle, char *why, size_t n)
{
    static const double PTS[3] = {0.37, 1.3, -0.81}, IV[4][2] = {{0.4, 1.7}, {-1.6, -0.3}, {0.05, 0.25}, {-0.2, -0.05}};
    int judged = 0;
    ce_angle(angle);
    ce_set('A', "1.7");                 /* the constants other letters stand for */
    ce_set('B', "-0.6");
    for (int i = 0; i < 3; i++) {
        char xs[40], lo[40], hi[40];
        double fx, a, b, h = 1e-6, d;
        snprintf(xs, sizeof xs, "%.17g", PTS[i]);
        snprintf(lo, sizeof lo, "%.17g", PTS[i] - h);
        snprintf(hi, sizeof hi, "%.17g", PTS[i] + h);
        if (!at(f, xs, &fx) || !at(F, lo, &a) || !at(F, hi, &b)) continue;
        d = (b - a) / (2 * h);
        judged++;
        if (fabs(d - fx) > 1e-6 * fmax(1, fabs(fx))) {
            snprintf(why, n, "F'(%s) = %.12g, f = %.12g", xs, d, fx);
            return false;
        }
    }
    for (int k = 0; k < 4; k++) {
        double a = IV[k][0], b = IV[k][1], s = 0, Fa, Fb;
        char as[40], bs[40];
        bool ok = true;
        int N = 400;
        double prev = 0;
        for (int i = 0; i <= N && ok; i++) {
            char xs[40];
            double x = a + (b - a) * i / N, fx;
            snprintf(xs, sizeof xs, "%.17g", x);
            if (!at(f, xs, &fx)) ok = false;
            /* a pole inside: the samples grow large, or jump sign at large size (the judge's rule: an
               interval with no singularity) */
            else if (fabs(fx) > 1e3 || (i > 0 && fx * prev < 0 && fabs(fx - prev) > 20)) ok = false;
            else s += fx * (i == 0 || i == N ? 1 : i % 2 ? 4 : 2);
            prev = fx;
        }
        snprintf(as, sizeof as, "%.17g", a);
        snprintf(bs, sizeof bs, "%.17g", b);
        if (!ok || !at(F, as, &Fa) || !at(F, bs, &Fb)) continue;
        s *= (b - a) / (3.0 * N);
        judged++;
        if (fabs((Fb - Fa) - s) > 1e-7 * fmax(1, fabs(s))) {
            snprintf(why, n, "F(%g) - F(%g) = %.12g, the integral %.12g", b, a, Fb - Fa, s);
            return false;
        }
    }
    if (!judged) snprintf(why, n, "nothing judged");
    return judged > 0;
}

/* f integrates to exactly 002's form of the hand-written F, F has no "+C", and F is right by value. */
static void is(const char *f, int angle, const char *Fhand)
{
    char out[OUT], want[OUT], why[256] = "";
    casim_status st = integ(f, angle, CASIM_MODEL_35, out, sizeof arena), sw;
    sw = casim_simplify(Fhand, angle, want, OUT, arena, sizeof arena);
    CHECK(sw == CASIM_OK, "the hand-written %s does not simplify (%d)", Fhand, (int)sw);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "integrate %s (angle %d): %d \"%s\", want \"%s\" (by hand %s)", f, angle,
          (int)st, out, want, Fhand);
    if (st != CASIM_OK) return;
    CHECK(strlen(out) < 2 || strcmp(out + strlen(out) - 2, "+C") != 0, "integrate %s: \"%s\" ends in +C", f, out);
    CHECK(by_value(f, out, angle, why, sizeof why), "integrate %s (angle %d) = %s: %s", f, angle, out, why);
}

static void refused(const char *f, casim_status want)
{
    char out[OUT];
    casim_status st = integ(f, CASIM_RAD, CASIM_MODEL_35, out, sizeof arena);
    CHECK(st == want && out[0] == '\0', "integrate %s: %d \"%s\", want status %d", f, (int)st, out, (int)want);
}

static void table(void)
{
    /* linearity, powers, the constant term */
    is("3×X^2+2×X+1", CASIM_RAD, "X^3+X^2+X");
    is("7", CASIM_RAD, "7×X");
    is("X^5", CASIM_RAD, "X^6÷6");
    is("-X^3", CASIM_RAD, "-X^4÷4");
    is("SQRT(X)", CASIM_RAD, "2×X^(3÷2)÷3");
    is("X^(1÷3)", CASIM_RAD, "3×X^(4÷3)÷4");
    is("X^(-2)", CASIM_RAD, "-1÷X");
    is("1÷X^2", CASIM_RAD, "-1÷X");
    is("X÷2", CASIM_RAD, "X^2÷4");
    is("A×X^2+B", CASIM_RAD, "A×X^3÷3+B×X");               /* constants with other letters */
    is("(A+1)×X", CASIM_RAD, "(A+1)×X^2÷2");
    is("π×X", CASIM_RAD, "π×X^2÷2");
    /* 1÷X and 1÷(aX+b): LN(ABS(...)) */
    is("1÷X", CASIM_RAD, "LN(ABS(X))");
    is("X^(-1)", CASIM_RAD, "LN(ABS(X))");
    is("3÷X", CASIM_RAD, "3×LN(ABS(X))");
    is("1÷(2×X+1)", CASIM_RAD, "LN(ABS(2×X+1))÷2");
    is("1÷(1-X)", CASIM_RAD, "-LN(ABS(1-X))");
    is("(3×X-2)^(-1)", CASIM_RAD, "LN(ABS(3×X-2))÷3");
    /* functions of a linear argument */
    is("EXP(3×X-1)", CASIM_RAD, "EXP(3×X-1)÷3");
    is("EXP(X)", CASIM_RAD, "EXP(X)");
    is("SIN(2×X)", CASIM_RAD, "-COS(2×X)÷2");
    is("COS(X÷2)", CASIM_RAD, "2×SIN(X÷2)");
    is("TAN(X)", CASIM_RAD, "-LN(ABS(COS(X)))");
    is("TAN(2×X+1)", CASIM_RAD, "-LN(ABS(COS(2×X+1)))÷2");
    is("ALOG(2×X)", CASIM_RAD, "ALOG(2×X)÷(2×LN(10))");
    is("SINH(2×X)", CASIM_RAD, "COSH(2×X)÷2");
    is("COSH(X)", CASIM_RAD, "SINH(X)");
    is("(2×X+1)^3", CASIM_RAD, "(2×X+1)^4÷8");
    is("SQRT(4×X+1)", CASIM_RAD, "(4×X+1)^(3÷2)÷6");
    is("SQ(X-3)", CASIM_RAD, "(X-3)^3÷3");
    is("INV(X)", CASIM_RAD, "LN(ABS(X))");
    /* the angle factors (CAS 001 rule 3): SIN, COS, TAN carry 180÷π or 200÷π; SINH and COSH carry none */
    is("COS(X)", CASIM_DEG, "SIN(X)×180÷π");
    is("SIN(X)", CASIM_DEG, "-COS(X)×180÷π");
    is("SIN(3×X)", CASIM_GRAD, "-COS(3×X)×200÷π÷3");
    is("TAN(X)", CASIM_DEG, "-LN(ABS(COS(X)))×180÷π");
    is("SINH(2×X)", CASIM_DEG, "COSH(2×X)÷2");
    is("COSH(X)", CASIM_GRAD, "SINH(X)");
    is("EXP(2×X)", CASIM_DEG, "EXP(2×X)÷2");
    /* a polynomial times a polynomial: expanded first */
    is("X×(X+1)^2", CASIM_RAD, "X^4÷4+2×X^3÷3+X^2÷2");
    is("(X-1)×(X+1)", CASIM_RAD, "X^3÷3-X");
    is("2×X×(3×X-1)", CASIM_RAD, "2×X^3-X^2");
    /* several forms at once */
    is("3×COS(X)-2÷X", CASIM_RAD, "3×SIN(X)-2×LN(ABS(X))");
    is("X^2+EXP(2×X)+SIN(X)", CASIM_RAD, "X^3÷3+EXP(2×X)÷2-COS(X)");
}

/* The review of the first build: in-scope forms it refused. A minus on any factor; a constant times a
   parenthesized sum; a polynomial times X^r (r > 0, not whole), as rule 2's "a polynomial times one of these,
   where expanding makes every term a form". */
static void review(void)
{
    is("-COS(2×X)÷2", CASIM_RAD, "-SIN(2×X)÷4");
    is("-EXP(X)÷2", CASIM_RAD, "-EXP(X)÷2");
    is("-SIN(X)×2", CASIM_RAD, "2×COS(X)");
    is("2×-SIN(X)", CASIM_DEG, "2×COS(X)×180÷π");
    is("X+-EXP(X)", CASIM_RAD, "X^2÷2-EXP(X)");
    is("-(EXP(X))÷2", CASIM_RAD, "-EXP(X)÷2");
    is("-SQRT(X)÷3", CASIM_RAD, "-2×X^(3÷2)÷9");
    is("-ALOG(X)×5", CASIM_RAD, "-5×ALOG(X)÷LN(10)");
    is("-INV(X)×2", CASIM_RAD, "-2×LN(ABS(X))");
    is("(EXP(X)+EXP(-X))÷2", CASIM_RAD, "(EXP(X)-EXP(-X))÷2");
    is("(SIN(X)+COS(X))÷2", CASIM_RAD, "(SIN(X)-COS(X))÷2");
    is("2×(SIN(X)+COS(X))", CASIM_GRAD, "400×SIN(X)÷π-400×COS(X)÷π");     /* 002 keeps no symbolic common factor */
    is("3×(X+SIN(X))", CASIM_RAD, "3×X^2÷2-3×COS(X)");
    is("X+(SIN(X)+1)", CASIM_RAD, "X^2÷2-COS(X)+X");
    is("-(X+COS(X))", CASIM_RAD, "-X^2÷2-SIN(X)");
    is("X×SQRT(X)", CASIM_RAD, "2×X^(5÷2)÷5");
    is("SQRT(X)×X", CASIM_RAD, "2×X^(5÷2)÷5");
    is("(X+1)×SQRT(X)", CASIM_RAD, "2×X^(5÷2)÷5+2×X^(3÷2)÷3");
    is("X^2×X^(1÷3)", CASIM_RAD, "3×X^(10÷3)÷10");
    is("SQRT(X)×X^(1÷3)", CASIM_RAD, "6×X^(11÷6)÷11");
    refused("X×X^(-1÷2)", CASIM_NOT_INTEGRABLE);             /* a negative power in a product, as X^2÷X */
    /* rule 6a (bd37a0b): powers of X combine; refused only where combining hides a point where the typed
       function is undefined (an X with a negative exponent, every combined term's exponent >= 0) */
    is("SQRT(X)^3", CASIM_RAD, "2×X^(5÷2)÷5");
    is("X^(1÷2)^3", CASIM_RAD, "2×X^(5÷2)÷5");               /* 35: (X^(1/2))^3 */
    is("INV(X^2)", CASIM_RAD, "-1÷X");
    is("1÷X÷X", CASIM_RAD, "-1÷X");
    is("X÷X^3", CASIM_RAD, "-1÷X");
    is("SQRT(X)÷X", CASIM_RAD, "2×SQRT(X)");
    is("3×X^2÷X^4", CASIM_RAD, "-3÷X");
    is("1÷(X×X)", CASIM_RAD, "-1÷X");
    is("(X+1)÷X", CASIM_RAD, "LN(ABS(X))+X");                /* rule 2: 1 + 1/X, X = 0 still undefined */
    is("(X^2+1)÷X^3", CASIM_RAD, "LN(ABS(X))-1÷(2×X^2)");
    refused("X^2÷X", CASIM_NOT_INTEGRABLE);
    refused("X^2÷SQRT(X)", CASIM_NOT_INTEGRABLE);
    refused("(X^2)^(1÷2)", CASIM_NOT_INTEGRABLE);            /* a fractional outer power: ABS(X) */
    refused("X^2×X^(-1)", CASIM_NOT_INTEGRABLE);
    refused("(X^2+X)÷X", CASIM_NOT_INTEGRABLE);              /* every term's exponent >= 0: X = 0 would vanish */
    refused("COS(π×X)", CASIM_NOT_INTEGRABLE);
    /* a product inside a minus or parentheses, as a term of its own (the review's fuzz) */
    is("-(1÷(2×X+1)^2)÷A", CASIM_RAD, "1÷(2×(2×X+1)×A)");
    is("2×(3×X^2)", CASIM_RAD, "2×X^3");
    is("-(3×SIN(X))", CASIM_RAD, "3×COS(X)");
    /* an arena too small for the walk is NO_ROOM, not NOT_INTEGRABLE */
    {
        char out[OUT];
        static unsigned char small[1000];
        casim_req rq = {CASIM_INTEGRATE, "(X+1)×(X+2)×(X+3)×(X+4)", CASIM_MODEL_35, 'X' - 'A', NULL, 0, CASIM_RAD, NULL};
        CHECK(casim_run(&rq, out, OUT, small, sizeof small) == CASIM_NO_ROOM, "a 1000-byte arena: not NO_ROOM");
    }
}

static void outside(void)
{
    refused("X×LN(X)", CASIM_NOT_INTEGRABLE);               /* by parts */
    refused("LN(X)", CASIM_NOT_INTEGRABLE);
    refused("X×EXP(X)", CASIM_NOT_INTEGRABLE);
    refused("X×EXP(X^2)", CASIM_NOT_INTEGRABLE);            /* a substitution that is not linear */
    refused("SIN(X^2)", CASIM_NOT_INTEGRABLE);
    refused("1÷(X^2-2)", CASIM_NOT_INTEGRABLE);             /* D > 0, not a square (CAS 013 rule 5) */
    refused("1÷(X^2+1)^2", CASIM_NOT_INTEGRABLE);           /* a quadratic squared (CAS 013 rule 5) */
    refused("SIN(X)^2", CASIM_NOT_INTEGRABLE);
    refused("SIN(X)×COS(X)", CASIM_NOT_INTEGRABLE);
    refused("SIN(X)+1÷(X^2-1)", CASIM_NOT_INTEGRABLE);      /* mixed (CAS 013 rule 1): X^2÷X answers since 013 */
    refused("(X+A)÷(X^2-1)", CASIM_NOT_INTEGRABLE);         /* another letter: integrate's status kept (CAS 013, #6686) */
    refused("1÷(X^3-2)", CASIM_NOT_INTEGRABLE);             /* a cubic factor */
    refused("(X^2+1)^6", CASIM_NOT_INTEGRABLE);             /* degree 12: its X^13 is past the bound (CAS 013 rule 2) */
    {
        /* an arena too small for partfrac to start: the table's refusal stands (CAS 013 rule 2, the review) */
        char out[OUT];
        casim_status st = integ("X^X", CASIM_RAD, CASIM_MODEL_35, out, 5500);
        CHECK(st == CASIM_NOT_INTEGRABLE, "integrate X^X at 5,500 B: %d", (int)st);
    }
    refused("2^X", CASIM_NOT_INTEGRABLE);
    refused("ABS(X)", CASIM_NOT_INTEGRABLE);
    refused("SIN(A×X)", CASIM_NOT_INTEGRABLE);              /* a linear argument's coefficients are numbers */
    refused("X=1", CASIM_BAD_ARG);                          /* an equation */
    {
        char out[OUT];
        CHECK(integ("2X", CASIM_RAD, CASIM_MODEL_35, out, sizeof arena) == CASIM_SYNTAX && strcmp(out, "1") == 0,
              "integrate 2X: \"%s\"", out);
    }
}

/* The stamp (rule 6): X^2^3 is X^8 read as STU, X^6 read as 35. */
static void stamp(void)
{
    char out[OUT], want[OUT];
    CHECK(integ("X^2^3", CASIM_RAD, CASIM_MODEL_STU, out, sizeof arena) == CASIM_OK &&
              casim_simplify("X^9÷9", CASIM_RAD, want, OUT, arena, sizeof arena) == CASIM_OK && strcmp(out, want) == 0,
          "integrate X^2^3 (STU): \"%s\"", out);
    CHECK(integ("X^2^3", CASIM_RAD, CASIM_MODEL_35, out, sizeof arena) == CASIM_OK &&
              casim_simplify("X^7÷7", CASIM_RAD, want, OUT, arena, sizeof arena) == CASIM_OK && strcmp(out, want) == 0,
          "integrate X^2^3 (35): \"%s\"", out);
    CHECK(casim_integrate("X^2", 'X' - 'A', CASIM_MODEL_35, CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK,
          "casim_integrate X^2: \"%s\"", out);
}

/* work/006-examples.txt at the device's 8 KB arena. */
static void visible(void)
{
    static unsigned char small[8192];
    FILE *fp = fopen("work/006-examples.txt", "r");
    char line[2048];
    int n = 0;
    CHECK(fp != NULL, "work/006-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[8], *s = line, out[OUT], want[OUT];
        int k = 0, angle;
        casim_req rq;
        casim_status st;
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 8) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 7) continue;
        n++;
        angle = strcmp(f[4], "DEG") == 0 ? CASIM_DEG : strcmp(f[4], "GRAD") == 0 ? CASIM_GRAD : CASIM_RAD;
        rq = (casim_req){CASIM_INTEGRATE, f[2], strcmp(f[5], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35, f[3][0] - 'A',
                         NULL, 0, angle, NULL};
        st = casim_run(&rq, out, OUT, small, sizeof small);
        if (strcmp(f[6], "NOT_INTEGRABLE") == 0) CHECK(st == CASIM_NOT_INTEGRABLE, "%s: %d \"%s\"", f[0], (int)st, out);
        else if (strcmp(f[6], "BAD_ARG") == 0) CHECK(st == CASIM_BAD_ARG, "%s: %d \"%s\"", f[0], (int)st, out);
        else if (strcmp(f[6], "SYNTAX") == 0) CHECK(st == CASIM_SYNTAX, "%s: %d \"%s\"", f[0], (int)st, out);
        else if (strcmp(f[6], "TOO_LONG") == 0) CHECK(st == CASIM_TOO_LONG, "%s: %d \"%s\"", f[0], (int)st, out);
        else {
            rq.op = CASIM_SIMPLIFY, rq.eqn = f[6];
            CHECK(casim_run(&rq, want, OUT, arena, sizeof arena) == CASIM_OK, "%s: the hand-written %s", f[0], f[6]);
            CHECK(st == CASIM_OK && strcmp(out, want) == 0, "%s: integrate %s: %d \"%s\", want \"%s\"", f[0], f[2], (int)st, out,
                  want);
        }
    }
    fclose(fp);
    CHECK(n == 35, "%d visible examples, want 35", n);
}

/* CAS 013: work/013-examples.txt at the 8 KB arena, each answer byte for byte (rule 7 joins the terms, each in 002's
   form, so the file's text is compared as it is) and by value in its angle mode */
static void visible013(void)
{
    static unsigned char small[8192];
    FILE *fp = fopen("work/013-examples.txt", "r");
    char line[2048];
    int n = 0;
    CHECK(fp != NULL, "work/013-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[8], *s = line, out[OUT], why[256] = "";
        int k = 0, angle;
        casim_req rq;
        casim_status st;
        if (line[0] == '#') continue;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 8) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 7) continue;
        n++;
        angle = strcmp(f[4], "DEG") == 0 ? CASIM_DEG : strcmp(f[4], "GRAD") == 0 ? CASIM_GRAD : CASIM_RAD;
        rq = (casim_req){CASIM_INTEGRATE, f[2], CASIM_MODEL_35, f[3][0] - 'A', NULL, 0, angle, NULL};
        st = casim_run(&rq, out, OUT, small, sizeof small);
        if (strcmp(f[6], "NOT_INTEGRABLE") == 0) CHECK(st == CASIM_NOT_INTEGRABLE, "%s: %d \"%s\"", f[0], (int)st, out);
        else if (strcmp(f[6], "NO_ROOM") == 0) CHECK(st == CASIM_NO_ROOM, "%s: %d \"%s\"", f[0], (int)st, out);
        else if (strcmp(f[6], "TOO_LONG") == 0) CHECK(st == CASIM_TOO_LONG, "%s: %d \"%s\"", f[0], (int)st, out);
        else {
            CHECK(st == CASIM_OK && strcmp(out, f[6]) == 0, "%s: integrate %s: %d \"%s\", want \"%s\"", f[0], f[2], (int)st,
                  out, f[6]);
            CHECK(st != CASIM_OK || by_value(f[2], out, angle, why, sizeof why), "%s: %s = %s: %s", f[0], f[2], out, why);
        }
    }
    fclose(fp);
    CHECK(n == 19, "%d CAS 013 examples, want 19", n);
}

/* CAS 013: seeded n÷d over planted factors (linear to the 3rd power, quadratics with D < 0), each integrated in the
   three angle modes and judged by value (the core's own evaluation): F' = f at points and F(b) - F(a) against
   Simpson. A NO_ROOM or TOO_LONG is counted, not judged; any other refusal fails. */
static unsigned long long seed013 = 0x013u;
static int rnd013(int lo, int hi)
{
    seed013 = seed013 * 6364136223846793005ull + 1442695040888963407ull;
    return lo + (int)((seed013 >> 33) % (unsigned long long)(hi - lo + 1));
}

static void rational(void)
{
    /* roots outside by_value's points and intervals (a weak pole inside an interval escapes its detection) */
    static const char *const LIN[] = {"(X-2)", "(X+2)", "(2×X-5)", "(3×X+7)", "X", "(X-4)"};
    static const char *const QUAD[] = {"(X^2+1)", "(X^2+X+1)", "(2×X^2-X+3)", "(X^2+4)", "(X^2-2×X+5)"};
    int judged = 0, refused_ = 0, cancelled = 0;
    for (int trial = 0; trial < 300; trial++) {
        char text[512], out[OUT], why[256] = "";
        int len = 0, used[11] = {0}, nf = rnd013(1, 3), deg = 0, angle = ANGLES[trial % 3];
        casim_status st;
        len += snprintf(text + len, sizeof text - (size_t)len, "(");
        for (int k = rnd013(0, 3), first = 1; k >= 0; k--, first = 0)
            len += snprintf(text + len, sizeof text - (size_t)len, first ? "%d×X^%d" : "%+d×X^%d", rnd013(-9, 9), k);
        len += snprintf(text + len, sizeof text - (size_t)len, ")÷(");
        for (int i = 0; i < nf; i++) {
            int q = rnd013(0, 2) == 0, which = q ? rnd013(0, 4) : rnd013(0, 5), p = q ? 1 : rnd013(1, 3);
            if (used[q * 6 + which]++) continue;
            len += snprintf(text + len, sizeof text - (size_t)len, "%s%s^%d", deg ? "×" : "", q ? QUAD[which] : LIN[which], p);
            deg += (q ? 2 : 1) * p;
        }
        snprintf(text + len, sizeof text - (size_t)len, ")");
        if (deg == 0 || deg > 10) continue;
        st = integ(text, angle, CASIM_MODEL_35, out, sizeof arena);
        if (st == CASIM_NO_ROOM || st == CASIM_TOO_LONG) {
            refused_++;
            continue;
        }
        {
            /* a common factor (partfrac prints its rows) is refused (rule 8, #6708) */
            char pf[OUT];
            casim_req rq = {CASIM_PARTFRAC, text, CASIM_MODEL_35, 'X' - 'A', NULL, 0, angle, NULL};
            casim_status ps = casim_run(&rq, pf, OUT, arena, sizeof arena);
            if (ps == CASIM_OK && strstr(pf, "; ")) {
                CHECK(st == CASIM_NOT_INTEGRABLE, "integrate %s cancels (%s): status %d \"%s\"", text, pf, (int)st, out);
                cancelled++;
                continue;
            }
        }
        CHECK(st == CASIM_OK, "integrate %s (angle %d): status %d", text, angle, (int)st);
        if (st != CASIM_OK) continue;
        judged++;
        CHECK(by_value(text, out, angle, why, sizeof why), "integrate %s (angle %d) = %s: %s", text, angle, out, why);
    }
    printf("integrate: CAS 013, %d seeded rational integrands judged by value, %d refused for a common factor, %d NO_ROOM or "
           "TOO_LONG\n", judged, cancelled, refused_);
    CHECK(judged > 200, "%d judged", judged);
}

/* Every angle, every function form: by value. */
static void angles(void)
{
    static const char *const FS[] = {"SIN(2×X+1)", "COS(3×X)", "TAN(X÷2)", "SINH(X)", "COSH(2×X)", "EXP(X÷3)", "ALOG(X)"};
    for (int a = 0; a < 3; a++)
        for (size_t i = 0; i < sizeof FS / sizeof FS[0]; i++) {
            char out[OUT], why[256] = "";
            casim_status st = integ(FS[i], ANGLES[a], CASIM_MODEL_35, out, sizeof arena);
            CHECK(st == CASIM_OK && by_value(FS[i], out, ANGLES[a], why, sizeof why), "integrate %s (angle %d) = %s: %d %s", FS[i],
                  ANGLES[a], out, (int)st, why);
        }
}

int main(void)
{
    ce_init();
    table();
    review();
    outside();
    stamp();
    visible();
    angles();
    visible013();
    rational();
    return check_done();
}
