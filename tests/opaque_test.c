/* Unit CAS 003, opaque names (#4264 points 2 and 3, accepted): every name derive refuses, postfix ! and
 * the indirect (i)/(j) are opaque in simplify, expand, collect and subst: their arguments simplified, like
 * terms and factors collected. RAND and SEED are impure: a subtree holding either is never merged,
 * cancelled, folded, dropped or duplicated, never distributed over or expanded, and impure terms keep their
 * order. Based numbers, complex literals and vectors stay refused; derive refuses all of them. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static void simp(const char *in, const char *want)
{
    char out[OUT];
    casim_status st = casim_simplify(in, CASIM_RAD, out, OUT, arena, sizeof arena);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "simplify %s: %d \"%s\", want \"%s\"", in, (int)st, out, want);
}

static void expd(const char *in, const char *want)
{
    char out[OUT];
    casim_status st = casim_expand(in, CASIM_RAD, out, OUT, arena, sizeof arena);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "expand %s: %d \"%s\", want \"%s\"", in, (int)st, out, want);
}

static void refused(const char *in, const char *name)
{
    char out[OUT];
    casim_status st = casim_simplify(in, CASIM_RAD, out, OUT, arena, sizeof arena);
    CHECK(st == CASIM_NOT_DIFFERENTIABLE && strcmp(out, name) == 0, "simplify %s: %d \"%s\", want refused %s", in,
          (int)st, out, name);
}

int main(void)
{
    char out[OUT];
    ce_init();
    /* opaque: arguments simplified, like terms and factors collected, in every operation */
    simp("IP(X)+IP(X)", "2×IP(X)");
    simp("IP(X+X)", "IP(2×X)");
    simp("FP(Y)×FP(Y)", "FP(Y)^2");
    simp("IDIV(X,2)+IDIV(X,2)", "2×IDIV(X,2)");
    simp("IDIV(X+X,2)-IDIV(2×X,2)", "0");
    simp("n+n", "2×n");
    simp("REGX×REGX", "REGX^2");
    simp("X!+X!", "2×X!");
    simp("(X+1)!", "(X+1)!");
    simp("⒤+⒤", "2×⒤");
    simp("SGN(Y+X)-SGN(X+Y)", "0");
    simp("→DEG(X)+→DEG(X)", "2×→DEG(X)");
    expd("IP((X+1)^2)", "IP(2×X+X^2+1)");             /* 002's order: X before X^2 */
    CHECK(casim_collect("IP(X)×X+X", 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
              strcmp(out, "X+X×IP(X)") == 0, "collect IP(X)×X+X: \"%s\"", out);
    CHECK(casim_subst("IP(X)+IP(X)", 'X' - 'A', "Y+1", CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
              strcmp(out, "2×IP(Y+1)") == 0, "subst IP(X)+IP(X): \"%s\"", out);
    /* impure: RAND and SEED, never merged, cancelled, folded, dropped, duplicated; order kept */
    simp("RAND+RAND", "RAND+RAND");
    simp("RAND-RAND", "RAND-RAND");
    simp("RAND×RAND", "RAND×RAND");
    simp("0×RAND+X", "X+0×RAND");
    simp("SIN(RAND)+SIN(RAND)", "SIN(RAND)+SIN(RAND)");
    simp("SEED(X)+SEED(X)", "SEED(X)+SEED(X)");
    simp("2×RAND+RAND", "2×RAND+RAND");
    simp("COS(RAND)+RAND", "COS(RAND)+RAND");       /* the draws' order: COS(RAND) is drawn first */
    simp("RAND+COS(RAND)", "RAND+COS(RAND)");
    expd("(RAND+1)^2", "(1+RAND)^2");              /* pure first: still one draw */
    expd("X×(RAND+1)", "X×(1+RAND)");
    expd("RAND×(X+1)", "(X+1)×RAND");               /* pure operands first, impure after, in order */
    expd("(X+1)^2+RAND", "2×X+X^2+1+RAND");          /* the pure part still expands */
    /* still refused: based numbers, complex literals, vectors; derive refuses every opaque name */
    refused("1Fh+X", "1Fh");
    refused("X+1i2", "1i2");
    refused("[X,1]", "[");
    CHECK(casim_derive("IP(X)", 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_NOT_DIFFERENTIABLE &&
              strcmp(out, "IP") == 0, "derive IP(X): \"%s\"", out);
    CHECK(casim_derive("RAND×X", 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_NOT_DIFFERENTIABLE &&
              strcmp(out, "RAND") == 0, "derive RAND×X: \"%s\"", out);
    CHECK(casim_derive("X!", 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_NOT_DIFFERENTIABLE &&
              strcmp(out, "!") == 0, "derive X!: \"%s\"", out);
    /* values through the core, where they are deterministic */
    {
        static const char *const det[] = {"IP(X)+IP(X)", "IDIV(X+X,2)-IDIV(2×X,2)", "X!+X!", "FP(Y)×FP(Y)"};
        ce_angle(CASIM_RAD);
        ce_set('X', "3.7");
        ce_set('Y', "1.3");
        for (size_t i = 0; i < sizeof det / sizeof det[0]; i++) {
            abn a, b;
            CHECK(casim_simplify(det[i], CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK && ce_eval(det[i], &a) == ABN_OK &&
                      ce_eval(out, &b) == ABN_OK && ce_close(&b, &a, -24),
                  "%s = %s: value", det[i], out);
        }
    }
    return check_done();
}
