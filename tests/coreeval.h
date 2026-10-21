/* coreeval: the core as casim's judge. Text is evaluated by the core's own eqn_eval, in an angle
 * mode with variables assigned, and values are compared in the core's own decimal arithmetic. */
#ifndef COREEVAL_H
#define COREEVAL_H

#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "eqn.h"

static ab_calc *ce_calc;
static int ce_model = 35;   /* the model text is read in (unit 031: 33, 35, AB_MODEL_STU); 35 as before 031 */

static inline void ce_init(void)
{
    ce_calc = calloc(1, sizeof *ce_calc);
    if (!ce_calc) exit(2);
    ab_init(ce_calc);
}

static inline void ce_angle(int angle)
{
    ce_calc->angle = (ab_angle)angle;
}

/* A-Z, or i for the 33s's variable i (var[26], unit 025; ab_init leaves the core in 33s mode). */
static inline void ce_set(char var, const char *value)
{
    ab_val *v = &ce_calc->var[var == 'i' ? 26 : var - 'A'];
    memset(v, 0, sizeof *v);
    v->kind = AB_REAL;
    if (abn_from_text(&v->re, value) != ABN_OK) exit(3);
}

static inline void ce_set_abn(char var, abn value)
{
    ab_val *v = &ce_calc->var[var - 'A'];
    memset(v, 0, sizeof *v);
    v->kind = AB_REAL;
    v->re = value;
}

/* The value of text (an = gives left minus right, as the core's XEQ does); ABN_OK only for a real. */
static inline abn_status ce_eval(const char *text, abn *out)
{
    ab_val v;
    int errpos = 0;
    abn_status st = eqn_eval(ce_calc, text, ce_model, false, false, &v, &errpos);
    if (st != ABN_OK) return st;
    if (v.kind != AB_REAL) return ABN_DOMAIN;
    *out = v.re;
    return ABN_OK;
}

/* CAS 020: the value of text, real or complex (kind AB_REAL or AB_COMPLEX); and a variable set to one */
static inline abn_status ce_eval_val(const char *text, ab_val *out)
{
    int errpos = 0;
    abn_status st = eqn_eval(ce_calc, text, ce_model, false, false, out, &errpos);
    if (st != ABN_OK) return st;
    if (out->kind == AB_REAL) out->im = abn_zero();
    return out->kind == AB_REAL || out->kind == AB_COMPLEX ? ABN_OK : ABN_DOMAIN;
}

static inline void ce_set_val(char var, const ab_val *value)
{
    ce_calc->var[var - 'A'] = *value;
}

static inline abn ce_num(const char *text)
{
    abn a;
    if (abn_from_text(&a, text) != ABN_OK) exit(3);
    return a;
}

/* |got - want| <= 10^tol * |want|, or <= 10^tol when want is 0. CAS 008 rule 6: an exact 0 (got) also passes against
   a residue within 1E-30 of 0 (want), since the core's π has 34 digits (its SIN(π) is about -1.16E-34); abacus's
   judge uses the same rule. */
static inline bool ce_close(const abn *got, const abn *want, int tol)
{
    abn d, bound, scale;
    if (abn_is_zero(got)) {
        abn w = abn_abs(want), tiny;
        if (abn_pow10(&tiny, -30) == ABN_OK && abn_cmp(&w, &tiny) <= 0) return true;
    }
    if (abn_sub(&d, got, want) != ABN_OK) return false;
    d = abn_abs(&d);
    if (abn_pow10(&scale, tol) != ABN_OK) return false;
    if (abn_is_zero(want)) bound = scale;
    else {
        abn w = abn_abs(want);
        if (abn_mul(&bound, &w, &scale) != ABN_OK) return false;
    }
    return abn_cmp(&d, &bound) <= 0;
}

static inline const char *ce_text(const abn *a)
{
    static char buf[4][ABN_TEXT_MAX];
    static int k;
    k = (k + 1) % 4;
    abn_to_text(a, buf[k], ABN_TEXT_MAX);
    return buf[k];
}

#endif
