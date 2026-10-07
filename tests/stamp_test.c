/* Unit CAS 004, rules 1 and 2: each text read as the model its equation was stamped with (unit 031), and
 * one output text that reads the same under every model. STU groups ^ right to left, and a minus starting
 * an exponent covers the chain after it; 33 and 35 group left to right. Exact texts; values against the
 * core's own eqn_eval at the same model over seeded power chains; the round trip under all three models;
 * the five old calls read 35. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];
static const int MODELS[3] = {CASIM_MODEL_STU, CASIM_MODEL_33, CASIM_MODEL_35};

static casim_status run(casim_op op, const char *eqn, int model, const char *repl, int repl_model, char *out)
{
    casim_req rq = {op, eqn, model, 'X' - 'A', repl, repl_model, CASIM_RAD};
    return casim_run(&rq, out, OUT, arena, sizeof arena);
}

static void is(casim_op op, const char *eqn, int model, const char *want)
{
    char out[OUT];
    casim_status st = run(op, eqn, model, NULL, 0, out);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "op %d %s (model %d): %d \"%s\", want \"%s\"", (int)op, eqn, model,
          (int)st, out, want);
}

/* out, read again under every model, gives out again (rule 2, for all three stamps). */
static void round_trip(casim_op op, const char *eqn, int model)
{
    char out[OUT], again[OUT];
    if (run(op, eqn, model, NULL, 0, out) != CASIM_OK) return;
    for (int m = 0; m < 3; m++) {
        casim_status st = run(CASIM_SIMPLIFY, out, MODELS[m], NULL, 0, again);
        char want[OUT];
        /* simplify's own result is a fixed point under every model; another op's result simplifies to the
           same text under every model */
        run(CASIM_SIMPLIFY, out, CASIM_MODEL_35, NULL, 0, want);
        CHECK(st == CASIM_OK && strcmp(again, want) == 0, "%s (op %d, model %d) = %s reads as %s under model %d, as %s under 35",
              eqn, (int)op, model, out, again, MODELS[m], want);
        if (op == CASIM_SIMPLIFY)
            CHECK(strcmp(again, out) == 0, "%s = %s is not a fixed point under model %d: %s", eqn, out, MODELS[m], again);
    }
}

static void exact(void)
{
    /* the unit's test: 2^3^2 as 2^(3^2) stamped STU, (2^3)^2 stamped 35 */
    is(CASIM_SIMPLIFY, "2^3^2", CASIM_MODEL_STU, "512");
    is(CASIM_SIMPLIFY, "2^3^2", CASIM_MODEL_35, "64");
    is(CASIM_SIMPLIFY, "2^3^2", CASIM_MODEL_33, "64");
    is(CASIM_SIMPLIFY, "2^3^2", 0, "64");
    is(CASIM_SIMPLIFY, "X^3^2", CASIM_MODEL_STU, "X^9");
    is(CASIM_SIMPLIFY, "X^3^2", CASIM_MODEL_35, "X^6");
    is(CASIM_DERIVE, "X^3^2", CASIM_MODEL_STU, "9×X^8");
    is(CASIM_DERIVE, "X^3^2", CASIM_MODEL_35, "6×X^5");
    is(CASIM_DERIVE, "2^3^X", CASIM_MODEL_STU, "LN(2)×LN(3)×2^(3^X)×3^X");
    is(CASIM_SIMPLIFY, "X^Y^Z", CASIM_MODEL_STU, "X^(Y^Z)");
    is(CASIM_SIMPLIFY, "X^Y^Z", CASIM_MODEL_35, "(X^Y)^Z");
    /* a minus starting an exponent: STU covers the chain after it, 35 binds it to the next operand */
    is(CASIM_SIMPLIFY, "2^-3^2", CASIM_MODEL_STU, "0.001953125");   /* 2^-9 */
    is(CASIM_SIMPLIFY, "2^-3^2", CASIM_MODEL_35, "0.015625");       /* (2^-3)^2 */
    is(CASIM_SIMPLIFY, "2^-1^2", CASIM_MODEL_STU, "0.5");
    is(CASIM_SIMPLIFY, "-2^3^2", CASIM_MODEL_STU, "-512");
    is(CASIM_SIMPLIFY, "2^1^2^3", CASIM_MODEL_STU, "2");
    is(CASIM_SIMPLIFY, "2^1^2^3", CASIM_MODEL_35, "64");
    /* precedence otherwise unchanged (unit 031 rule 4) */
    is(CASIM_SIMPLIFY, "-2^2", CASIM_MODEL_STU, "-4");
    is(CASIM_SIMPLIFY, "2^-2", CASIM_MODEL_STU, "0.25");
    is(CASIM_SIMPLIFY, "2^-3×4", CASIM_MODEL_STU, "0.5");
    is(CASIM_SIMPLIFY, "X^2×Y^3^2", CASIM_MODEL_STU, "X^2×Y^9");
    is(CASIM_EXPAND, "(X+1)^2^1", CASIM_MODEL_STU, "2×X+X^2+1");
    is(CASIM_COLLECT, "X^2^2+X", CASIM_MODEL_STU, "X^4+X");
    /* subst: the replacement read as its own model */
    {
        char out[OUT];
        CHECK(run(CASIM_SUBST, "X^2", CASIM_MODEL_35, "Y^3^2", CASIM_MODEL_STU, out) == CASIM_OK && strcmp(out, "Y^18") == 0,
              "subst X^2 (35), X = Y^3^2 (STU): \"%s\"", out);
        CHECK(run(CASIM_SUBST, "X^2", CASIM_MODEL_35, "Y^3^2", CASIM_MODEL_35, out) == CASIM_OK && strcmp(out, "Y^12") == 0,
              "subst X^2 (35), X = Y^3^2 (35): \"%s\"", out);
        CHECK(run(CASIM_SUBST, "X^2^Y", CASIM_MODEL_STU, "Z", CASIM_MODEL_STU, out) == CASIM_OK && strcmp(out, "Z^(2^Y)") == 0,
              "subst X^2^Y (STU), X = Z: \"%s\"", out);
        CHECK(run(CASIM_SUBST, "X+1", CASIM_MODEL_STU, "2Y", CASIM_MODEL_STU, out) == CASIM_SYNTAX && strcmp(out, "5") == 0,
              "subst SYNTAX offset in repl, STU: \"%s\"", out);
    }
    /* bad arguments */
    {
        char out[OUT];
        casim_req rq = {CASIM_SIMPLIFY, "X", 7, 0, NULL, 0, CASIM_RAD};
        CHECK(casim_run(&rq, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "model 7 accepted");
        rq.model = CASIM_MODEL_STU, rq.op = (casim_op)9;
        CHECK(casim_run(&rq, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "op 9 accepted");
        rq = (casim_req){CASIM_SUBST, "X", CASIM_MODEL_STU, 23, "Y", 2, CASIM_RAD};
        CHECK(casim_run(&rq, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "repl_model 2 accepted");
        rq = (casim_req){CASIM_SUBST, "X", CASIM_MODEL_STU, 23, NULL, CASIM_MODEL_STU, CASIM_RAD};
        CHECK(casim_run(&rq, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "subst without repl accepted");
    }
    /* the review: 0^(-1/2) prints with its ^ (not SQRT), so a chain through it keeps its parentheses */
    {
        static const char *const Z[] = {"Y^(0^-0.5)", "(0^-0.5)^X", "(0^-0.5)^2.5", "(X-X)^(-1÷2)^Y", "Y^(X-X)^(-1÷2)"};
        for (size_t i = 0; i < sizeof Z / sizeof Z[0]; i++)
            for (int m = 0; m < 3; m++) {
                char out[OUT];
                if (run(CASIM_SIMPLIFY, Z[i], MODELS[m], NULL, 0, out) == CASIM_OK)
                    CHECK(!bare_chain(out), "%s (model %d) = %s: a bare chain", Z[i], MODELS[m], out);
                round_trip(CASIM_SIMPLIFY, Z[i], MODELS[m]);
                round_trip(CASIM_DERIVE, Z[i], MODELS[m]);
            }
    }
    /* the review: no text at all is a bad argument, for every op */
    for (int op = CASIM_DERIVE; op <= CASIM_SUBST; op++) {
        char out[OUT];
        casim_req rq = {(casim_op)op, NULL, CASIM_MODEL_STU, 23, "Y", CASIM_MODEL_STU, CASIM_RAD};
        CHECK(casim_run(&rq, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "op %d: a NULL eqn accepted", op);
    }
    {
        char out[OUT];
        CHECK(casim_simplify(NULL, CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_BAD_ARG, "casim_simplify(NULL)");
    }
    /* the old calls read 35 */
    {
        char out[OUT];
        CHECK(casim_simplify("2^3^2", CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK && strcmp(out, "64") == 0,
              "casim_simplify 2^3^2: \"%s\"", out);
    }
}

/* Seeded chains of powers with minus signs, numbers and variables: under each model, the value of
   simplify's and expand's result equals the core's value of the input read in that model, and the result
   reads alike under every model. */
static void chains(void)
{
    static const char *const AT[] = {"X", "Y", "2", "0.5", "1.5", "(X+1)", "SIN(X)", "3"};
    unsigned long long r = 31;
    int judged = 0, bad = 0;
    ce_angle(CASIM_RAD);
    ce_set('X', "1.3");
    ce_set('Y', "0.7");
    for (int n = 0; n < 2000 && bad < 5; n++) {
        char t[256] = "";
        int len = 0, k = 2 + (int)((r >> 33) % 3);
        for (int i = 0; i < k; i++) {
            r = r * 6364136223846793005ULL + 1442695040888963407ULL;
            len += snprintf(t + len, sizeof t - (size_t)len, "%s%s%s", i ? "^" : ((r >> 50) % 4 ? "" : "-"),
                            i && (r >> 52) % 4 == 0 ? "-" : "", AT[(r >> 33) % 8]);
        }
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        for (int m = 0; m < 3; m++) {
            for (int o = 0; o < 2; o++) {
                casim_op op = o ? CASIM_EXPAND : CASIM_SIMPLIFY;
                char out[OUT];
                abn want, got;
                ce_model = MODELS[m];
                if (ce_eval(t, &want) != ABN_OK) continue;
                if (run(op, t, MODELS[m], NULL, 0, out) != CASIM_OK) continue;
                judged++;
                ce_model = CASIM_MODEL_35;              /* the output reads alike under every model */
                if (!(ce_eval(out, &got) == ABN_OK && ce_close(&got, &want, -20)) && bad++ < 5)
                    CHECK(false, "%s (model %d) -> %s: %s, the core %s", t, MODELS[m], out,
                          ce_eval(out, &got) == ABN_OK ? ce_text(&got) : "error", ce_text(&want));
                round_trip(op, t, MODELS[m]);
            }
        }
    }
    ce_model = 35;
    CHECK(bad == 0 && judged > 3000, "%d chains judged, %d wrong", judged, bad);
}

/* Every visible example of units 001-003, under each model, round-trips (rule 2 over every case). */
static void examples(void)
{
    static const char *const FILES[] = {"work/001-examples.txt", "work/002-examples.txt", "work/003-examples.txt"};
    int n = 0;
    for (int f = 0; f < 3; f++) {
        FILE *fp = fopen(FILES[f], "r");
        char line[4096];
        CHECK(fp != NULL, "%s not found", FILES[f]);
        if (!fp) continue;
        while (fgets(line, sizeof line, fp)) {
            char *bar = strstr(line, " | "), *text, *end;
            if (!bar || line[0] == '#') continue;
            text = bar + 3;
            /* the field after the id may be the op (003); the equation is the next field then */
            if (strncmp(text, "expand | ", 9) == 0 || strncmp(text, "collect | ", 10) == 0 ||
                strncmp(text, "subst | ", 8) == 0)
                text = strstr(text, " | ") + 3;
            if ((end = strstr(text, " | ")) != NULL) *end = '\0';
            text[strcspn(text, "\r\n")] = '\0';
            for (int m = 0; m < 3; m++) {
                round_trip(CASIM_SIMPLIFY, text, MODELS[m]);
                round_trip(CASIM_EXPAND, text, MODELS[m]);
                round_trip(CASIM_DERIVE, text, MODELS[m]);
            }
            n++;
        }
        fclose(fp);
    }
    CHECK(n >= 60, "%d visible examples", n);
}

int main(void)
{
    ce_init();
    exact();
    chains();
    examples();
    return check_done();
}
