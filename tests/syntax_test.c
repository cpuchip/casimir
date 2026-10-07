/* The parser against the core, on random token strings (the oracle for CAS 002's iterative parser):
 * where the core's eqn_eval says SYNTAX, casim says SYNTAX at the same character offset; where the core
 * parses, casim does not say SYNTAX. The reference is the core's check-only pass (the one that reports
 * SYNTAX ERROR when an equation is entered): a fraction only its evaluating pass rejects (1.2.3E4, 1.2.)
 * is a literal to casim, printed back as typed, and the core reports it when it evaluates. Half the
 * strings are random tokens; half are well-formed expressions, some with one token inserted or one
 * character dropped, so errors deep inside a valid prefix are exercised too. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define CASES 20000
static unsigned char arena[65536];
static unsigned long long rng = 0xD1B54A32D192ED03ull;

static unsigned rnd(unsigned n)
{
    rng ^= rng << 13, rng ^= rng >> 7, rng ^= rng << 17;
    return (unsigned)(rng % n);
}

/* Tokens the grammar knows, and some it does not; weighted toward the ones that build expressions. */
static const char *const TOK[] = {
    "X", "Y", "Z", "A", "2", "3", "0.5", ".5", "1.2.3", "1E5", "2E-3", "1.2.3E4", "1Fh", "1100b", "1i2", "3θ1",
    "π", "i", "+", "-", "-", "×", "÷", "^", "^", "(", "(", ")", ")", "=", ",", "!", "[", "]", "SIN(", "SQRT(",
    "LN(", "ABS(", "IP(", "IDIV(", "RAND", "REGX", "n", "x̄", "→DEG(", "⒤", " ", "E", ".", "SIN", "ACOSH(",
};

static const char *const OPERAND[] = {"X", "Y", "2", "0.5", ".5", "1E5", "1.2.3", "π", "i", "1Fh", "1i2", "RAND"};
static const char *const BINOP[] = {"+", "-", "×", "÷", "^", "^-"};
static const char *const HEAD[] = {"SIN(", "SQRT(", "LN(", "-(", "(", "ABS(", "IP(", "--"};

static void cat(char *t, size_t cap, const char *k)
{
    if (strlen(t) + strlen(k) < cap) strcat(t, k);
}

/* A well-formed expression of about the given size (operands and operators alternating, heads opened
   and closed), often with one random token inserted or one character dropped. */
static void shaped(char *t, size_t cap, int size)
{
    int open = 0;
    for (int i = 0; i < size; i++) {
        while (rnd(3) == 0) {
            cat(t, cap, HEAD[rnd(sizeof HEAD / sizeof HEAD[0])]);
            open += t[strlen(t) - 1] == '(';
        }
        cat(t, cap, OPERAND[rnd(sizeof OPERAND / sizeof OPERAND[0])]);
        if (rnd(4) == 0) cat(t, cap, "!");
        while (open && rnd(2)) cat(t, cap, ")"), open--;
        if (i + 1 < size) cat(t, cap, BINOP[rnd(sizeof BINOP / sizeof BINOP[0])]);
    }
    while (open--) cat(t, cap, ")");
    if (rnd(4) == 0) {
        cat(t, cap, "=");
        cat(t, cap, OPERAND[rnd(sizeof OPERAND / sizeof OPERAND[0])]);
    }
    if (rnd(2)) {
        /* one mutation: a token inserted at a random byte offset (on a character boundary), or a
           character dropped */
        size_t n = strlen(t), at = rnd((unsigned)n + 1);
        while (at < n && ((unsigned char)t[at] & 0xC0) == 0x80) at++;
        if (rnd(2)) {
            const char *k = TOK[rnd(sizeof TOK / sizeof TOK[0])];
            size_t m = strlen(k);
            if (n + m < cap) {
                memmove(t + at + m, t + at, n - at + 1);
                memcpy(t + at, k, m);
            }
        } else if (at < n) {
            size_t m = 1;
            while (at + m < n && ((unsigned char)t[at + m] & 0xC0) == 0x80) m++;
            memmove(t + at, t + at + m, n - at - m + 1);
        }
    }
}

int main(void)
{
    int syntax = 0, parsed = 0;
    ce_init();
    ce_angle(CASIM_RAD);
    for (int c = 0; c < CASES; c++) {
        char t[200] = "", out[4096], want[16];
        int n = 1 + (int)rnd(12), errpos = -1;
        ab_val v;
        abn_status cs;
        casim_status st;
        if (c % 2) shaped(t, sizeof t, 1 + (int)rnd(6));
        else
            for (int i = 0; i < n; i++) cat(t, sizeof t, TOK[rnd(sizeof TOK / sizeof TOK[0])]);
        cs = eqn_eval(ce_calc, t, ce_model, false, true, &v, &errpos);
        st = casim_simplify(t, CASIM_RAD, out, sizeof out, arena, sizeof arena);
        if (cs == ABN_SYNTAX) {
            syntax++;
            snprintf(want, sizeof want, "%d", errpos);
            CHECK(st == CASIM_SYNTAX && strcmp(out, want) == 0, "\"%s\": casim %d \"%s\", the core SYNTAX at %s", t,
                  (int)st, st == CASIM_SYNTAX ? out : "", want);
        } else {
            parsed++;
            CHECK(st != CASIM_SYNTAX && st != CASIM_TOO_LONG && st != CASIM_NO_ROOM, "\"%s\": the core parses it, casim says %d \"%s\"", t,
                  (int)st, out);
        }
    }
    printf("syntax: %d SYNTAX, %d parsed\n", syntax, parsed);
    CHECK(syntax > CASES / 10 && parsed > CASES / 20, "the generator is lopsided: %d SYNTAX, %d parsed", syntax,
          parsed);
    return check_done();
}
