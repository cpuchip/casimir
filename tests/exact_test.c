/* Unit CAS 008: exact values (work/008-exact-values.md). The angle table in RAD, DEG and GRAD against values written
 * here by hand; the inverse functions' principal angles; LN, EXP and EXP of LN; perfect and imperfect powers; what
 * stays as written; CASIM_UNDEFINED; the stamp; work/008-examples.txt; and seeded constant expressions over the
 * table, each answer read back on the core equal to the input by rule 6 (relative 1E-24, or within 1E-30 of 0
 * where the exact value is 0: the core's π has 34 digits, so its SIN(π) is about -1.16E-34). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static casim_status simp(const char *t, int angle, int model, char *out)
{
    casim_req rq = {CASIM_SIMPLIFY, t, model, 0, NULL, 0, angle, NULL};
    return casim_run(&rq, out, OUT, arena, sizeof arena);
}

/* rule 6: got (the answer's core value) equals want (the input's), relative 1E-24, or an exact 0 against a
   residue within 1E-30 */
static bool rule6(const abn *got, const abn *want)
{
    abn tiny;
    if (abn_is_zero(got)) {
        abn w = abn_abs(want);
        return abn_pow10(&tiny, -30) == ABN_OK && abn_cmp(&w, &tiny) <= 0;
    }
    return ce_close(got, want, -24);
}

static bool same_value(const char *t, const char *out, int angle)
{
    abn a, b;
    ce_angle(angle);
    if (ce_eval(t, &a) != ABN_OK || ce_eval(out, &b) != ABN_OK) return false;
    return rule6(&b, &a);
}

static const char *const MODE[3] = {"DEG", "RAD", "GRAD"};

static void is(const char *t, int angle, const char *want)
{
    char out[OUT];
    casim_status st = simp(t, angle, CASIM_MODEL_35, out);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "simplify %s (%s): %d \"%s\", want \"%s\"", t, MODE[angle], (int)st,
          out, want);
    if (st == CASIM_OK && strcmp(out, want) == 0) {
        abn a;
        ce_angle(angle);
        if (ce_eval(t, &a) != ABN_OK) return;      /* no value on the core (ASIN(2), TAN(90)): the text is the check */
        CHECK(same_value(t, out, angle), "simplify %s (%s) = %s: another value", t, MODE[angle], out);
    }
}

/* sin(nπ/12) for n = 0..23 in 002's form (CAS 019: a fraction), by hand: "" where it is not exact (odd multiples of π/12) */
static const char *const SINV[24] = {
    "0", "", "1÷2", "SQRT(2)÷2", "SQRT(3)÷2", "", "1", "", "SQRT(3)÷2", "SQRT(2)÷2", "1÷2", "",
    "0", "", "-1÷2", "-SQRT(2)÷2", "-SQRT(3)÷2", "", "-1", "", "-SQRT(3)÷2", "-SQRT(2)÷2", "-1÷2", ""};
/* tan(nπ/12) for n = 0..11 (period 12); "" at the poles and odd multiples */
static const char *const TANV[12] = {"0", "", "SQRT(3)÷3", "1", "SQRT(3)", "", "", "", "-SQRT(3)", "-1",
                                     "-SQRT(3)÷3", ""};

/* every row of the table in all three modes: RAD n×π÷12, DEG 15n, GRAD 50n÷3 */
static void table(void)
{
    static const char *const FN[3] = {"SIN", "COS", "TAN"};
    for (int angle = 0; angle < 3; angle++)
        for (int n = -24; n <= 24; n++)
            for (int f = 0; f < 3; f++) {
                char t[64], out[OUT];
                int m = ((n % 24) + 24) % 24;
                const char *want = f == 0 ? SINV[m] : f == 1 ? SINV[(m + 6) % 24] : TANV[m % 12];
                if (angle == CASIM_RAD) snprintf(t, sizeof t, "%s(%d×π÷12)", FN[f], n);
                else if (angle == CASIM_DEG) snprintf(t, sizeof t, "%s(%d)", FN[f], 15 * n);
                else snprintf(t, sizeof t, "%s(%d÷3)", FN[f], 50 * n);
                if (*want) is(t, angle, want);
                else if (f == 2 && m % 12 == 6) {
                    /* TAN's poles (CAS 016 rule 1): UNDEFINED */
                    casim_status st = simp(t, angle, CASIM_MODEL_35, out);
                    CHECK(st == CASIM_UNDEFINED, "simplify %s (%s): %d \"%s\", want UNDEFINED", t, MODE[angle], (int)st, out);
                } else {
                    /* not exact: stays a function of its (canonical) argument */
                    casim_status st = simp(t, angle, CASIM_MODEL_35, out);
                    CHECK(st == CASIM_OK && strncmp(out, FN[f], 3) == 0, "simplify %s (%s): %d \"%s\", want it as written",
                          t, MODE[angle], (int)st, out);
                }
            }
}

static void inverses(void)
{
    is("ASIN(0.5)", CASIM_RAD, "π÷6");
    is("ASIN(0.5)", CASIM_DEG, "30");
    is("ASIN(0.5)", CASIM_GRAD, "100÷3");
    is("ACOS(-0.5)", CASIM_RAD, "2×π÷3");
    is("ACOS(-0.5)", CASIM_DEG, "120");
    is("ATAN(1)", CASIM_RAD, "π÷4");
    is("ATAN(1)", CASIM_DEG, "45");
    is("ATAN(1)", CASIM_GRAD, "50");
    is("ATAN(SQRT(3))", CASIM_RAD, "π÷3");
    is("ATAN(-SQRT(3)÷3)", CASIM_RAD, "-π÷6");
    is("ASIN(-SQRT(2)÷2)", CASIM_RAD, "-π÷4");
    is("ACOS(-1)", CASIM_RAD, "π");
    is("ACOS(0)", CASIM_RAD, "π÷2");
    is("ACOS(1)", CASIM_DEG, "0");
    is("ASIN(-1)", CASIM_DEG, "-90");
    is("ACOS(SQRT(3)÷2)", CASIM_DEG, "30");
    is("ATAN(0)", CASIM_RAD, "0");
    /* the principal branch: ACOS in [0, π], ASIN and ATAN in [-π/2, π/2] */
    is("ACOS(-SQRT(3)÷2)", CASIM_RAD, "5×π÷6");
    is("ASIN(SIN(5×π÷6))", CASIM_RAD, "π÷6");
    /* not a table value: as written */
    is("ASIN(2)", CASIM_RAD, "ASIN(2)");
    is("ATAN(2)", CASIM_DEG, "ATAN(2)");
    is("ACOS(0.3)", CASIM_RAD, "ACOS(0.3)");
}

static void logs_and_powers(void)
{
    is("LN(1)", CASIM_RAD, "0");
    is("EXP(0)", CASIM_RAD, "1");
    is("EXP(2×LN(3))", CASIM_RAD, "9");
    is("EXP(LN(2)÷2)", CASIM_RAD, "SQRT(2)");
    is("EXP(-LN(4))", CASIM_RAD, "1÷4");
    is("EXP(0.5×LN(8))", CASIM_RAD, "SQRT(8)");
    is("EXP(X×LN(2))", CASIM_RAD, "EXP(X×LN(2))");          /* var inside: as written (002's order, unchanged) */
    is("4^(3÷2)", CASIM_RAD, "8");
    is("0.25^1.5", CASIM_RAD, "0.125");
    is("(27÷8)^(2÷3)", CASIM_RAD, "9÷4");
    is("8^(1÷3)", CASIM_RAD, "2");
    is("16^(-1÷4)", CASIM_RAD, "1÷2");
    is("9^(-1.5)", CASIM_RAD, "1÷27");
    is("SQRT(0.25)", CASIM_RAD, "0.5");
    is("SQRT(8)", CASIM_RAD, "SQRT(8)");
    is("2^(1÷2)", CASIM_RAD, "SQRT(2)");
    is("12^(1÷3)", CASIM_RAD, "12^(1÷3)");
    is("SIN(X)+COS(0)", CASIM_RAD, "SIN(X)+1");
}

static void pole(const char *t, int angle)
{
    char out[OUT];
    casim_status st = simp(t, angle, CASIM_MODEL_35, out);
    CHECK(st == CASIM_UNDEFINED && out[0] == '\0', "simplify %s (%s): %d \"%s\", want UNDEFINED", t, MODE[angle], (int)st,
          out);
}

static void as_written(void)
{
    is("SIN(π÷12)", CASIM_RAD, "SIN(π÷12)");
    is("SIN(15)", CASIM_DEG, "SIN(15)");
    pole("TAN(π÷2)", CASIM_RAD);                    /* CAS 016 rule 1: a pole has no value, as 1÷0 has none */
    pole("TAN(90)", CASIM_DEG);
    is("SIN(π÷6)", CASIM_DEG, "SIN(π÷6)");
    is("SIN(1)", CASIM_RAD, "SIN(1)");
    is("COS(X+π)", CASIM_RAD, "COS(X+π)");
}

static void undefined(void)
{
    static const char *const U[] = {"1÷COS(π÷2)", "1÷SIN(π)", "TAN(π÷4)÷SIN(0)", "1÷COS(90)", "LN(1)^(-1)"};
    for (unsigned i = 0; i < sizeof U / sizeof U[0]; i++) {
        char out[OUT];
        int angle = strstr(U[i], "90") ? CASIM_DEG : CASIM_RAD;
        casim_status st = simp(U[i], angle, CASIM_MODEL_35, out);
        CHECK(st == CASIM_UNDEFINED && out[0] == '\0', "simplify %s: %d \"%s\", want UNDEFINED", U[i], (int)st, out);
    }
}

static void stamp(void)
{
    char out[OUT];
    /* 4^3^0.5: STU reads 4^(3^0.5), not a perfect power; 35 reads (4^3)^0.5 = 8 */
    CHECK(simp("4^3^0.5", CASIM_RAD, CASIM_MODEL_35, out) == CASIM_OK && strcmp(out, "8") == 0, "4^3^0.5 (35): \"%s\"", out);
    CHECK(simp("4^3^0.5", CASIM_RAD, CASIM_MODEL_STU, out) == CASIM_OK && strcmp(out, "4^SQRT(3)") == 0,
          "4^3^0.5 (STU): \"%s\"", out);
}

/* seeded constant expressions over the table: each answer equals the input by value (rule 6) */
static unsigned seed = 8008;
static int rnd(int n)
{
    seed = seed * 1103515245u + 12345u;
    return (int)((seed >> 16) % (unsigned)n);
}

static void seeded(void)
{
    static const char *const F[] = {"SIN", "COS", "TAN"};
    static const char *const V[] = {"0", "0.5", "-0.5", "SQRT(2)÷2", "-SQRT(2)÷2", "SQRT(3)÷2", "1", "-1", "SQRT(3)",
                                    "SQRT(3)÷3"};
    static const char *const INV[] = {"ASIN", "ACOS", "ATAN"};
    int judged = 0;
    for (int t = 0; t < 600; t++) {
        char text[256], out[OUT];
        int angle = rnd(3), len = 0;
        casim_status st;
        for (int k = 0, terms = 1 + rnd(3); k < terms; k++) {
            char arg[48];
            int n = rnd(49) - 24;
            if (angle == CASIM_RAD) snprintf(arg, sizeof arg, "%d×π÷12", n);
            else if (angle == CASIM_DEG) snprintf(arg, sizeof arg, "%d", 15 * n);
            else snprintf(arg, sizeof arg, "%d÷3", 50 * n);
            if (rnd(4) == 0)
                len += snprintf(text + len, sizeof text - (size_t)len, "%s%d×%s(%s)", k ? "+" : "", 1 + rnd(5),
                                INV[rnd(3)], V[rnd(10)]);
            else
                len += snprintf(text + len, sizeof text - (size_t)len, "%s%d×%s(%s)%s", k ? "-" : "", 1 + rnd(5), F[rnd(3)],
                                arg, rnd(3) ? "" : "^2");
        }
        {
            abn a;
            ce_angle(angle);
            if (ce_eval(text, &a) != ABN_OK) continue;       /* outside the domain (ASIN(SQRT(3)), TAN at a pole) */
        }
        st = simp(text, angle, CASIM_MODEL_35, out);
        if (st == CASIM_UNDEFINED || st == CASIM_TOO_LONG) continue;
        judged++;
        CHECK(st == CASIM_OK && same_value(text, out, angle), "simplify %s (%s): %d \"%s\" has another value", text,
              MODE[angle], (int)st, out);
    }
    printf("exact: %d seeded constant expressions judged\n", judged);
    CHECK(judged > 400, "%d judged", judged);
}

/* work/008-examples.txt at the 8,800 B arena */
static void visible(void)
{
    static unsigned char small[8800];
    FILE *fp = fopen("work/008-examples.txt", "r");
    char line[1024];
    int n = 0;
    CHECK(fp != NULL, "work/008-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[7], *s = line, out[OUT];
        int k = 0, angle;
        casim_req rq;
        casim_status st;
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
        angle = strcmp(f[3], "DEG") == 0 ? CASIM_DEG : strcmp(f[3], "GRAD") == 0 ? CASIM_GRAD : CASIM_RAD;
        rq = (casim_req){CASIM_SIMPLIFY, f[2], strcmp(f[4], "STU") == 0 ? CASIM_MODEL_STU : CASIM_MODEL_35, 0, NULL, 0, angle, NULL};
        st = casim_run(&rq, out, OUT, small, sizeof small);
        name = st == CASIM_OK ? out : st == CASIM_UNDEFINED ? "UNDEFINED" : st == CASIM_NO_ROOM ? "NO_ROOM" : "another status";
        CHECK(strcmp(name, f[5]) == 0, "%s: simplify %s (%s): \"%s\", want \"%s\"", f[0], f[2], f[3], name, f[5]);
        if (st == CASIM_OK && strcmp(name, f[5]) == 0) {
            /* the input read as its stamp, the answer (casim's own text) as 35 */
            abn a, b;
            ce_angle(angle);
            ce_model = strcmp(f[4], "STU") == 0 ? CASIM_MODEL_STU : 35;
            abn_status es = ce_eval(f[2], &a);
            ce_model = 35;
            if (es == ABN_OK)                      /* else no value on the core (ASIN(2), TAN(90)): the text is the check */
                CHECK(ce_eval(out, &b) == ABN_OK && rule6(&b, &a), "%s: %s another value", f[0], f[2]);
        }
    }
    fclose(fp);
    CHECK(n >= 30, "%d visible examples", n);
}

int main(void)
{
    ce_init();
    table();
    inverses();
    logs_and_powers();
    as_written();
    undefined();
    stamp();
    seeded();
    visible();
    return check_done();
}
