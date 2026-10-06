/* casim: unit CAS 001, the derivative of an equation (see casim.h).
 *
 * Three parts, all in the caller's arena:
 * - a parser that builds a tree from the core's equation grammar (core/eqn.h's grammar comment and
 *   core/eqn.c, read as the spec), failing where and as the core fails, so SYNTAX positions agree;
 * - the derivative, by the standard rules (the sum, product, quotient, power and chain rules and the
 *   table of elementary derivatives, e.g. Wikipedia "Differentiation rules", "Differentiation of
 *   trigonometric functions"), with the angle mode's factor on the trigonometric functions;
 * - a printer back to the core's grammar with only the parentheses its precedence needs.
 * The derivative shares the input's subtrees rather than copying them. Its constructors simplify
 * exactly as the unit's rule 5 asks and no further.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "abn.h"
#include "casim.h"

enum { N_NUM, N_VAR, N_PI, N_NEG, N_ADD, N_SUB, N_MUL, N_DIV, N_POW, N_FN, N_EQ };

/* The functions casim differentiates (and SGN, which only appears in a derivative). */
enum {
    F_SQ, F_SQRT, F_INV, F_LN, F_LOG, F_EXP, F_ALOG, F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN, F_SINH,
    F_COSH, F_TANH, F_ASINH, F_ACOSH, F_ATANH, F_ABS, F_SGN, F_REFUSED
};

static const char *const FNAME[] = {
    "SQ", "SQRT", "INV", "LN", "LOG", "EXP", "ALOG", "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN", "SINH",
    "COSH", "TANH", "ASINH", "ACOSH", "ATANH", "ABS", "SGN",
};

typedef struct node node;
struct node {
    uint8_t kind;
    uint8_t id;         /* N_FN: the function; N_VAR: the variable, 0-25 for A-Z, 26 for i */
    uint8_t exact;      /* N_NUM: u.v is the value the core reads (a fraction literal has none) */
    uint8_t len;        /* N_NUM from the text: its length in bytes; 0 for a computed number */
    uint16_t src;       /* N_NUM from the text: its byte offset */
    union {
        abn v;
        struct {
            node *a, *b;
        } k;
    } u;
};

/* Every name of the core's grammar (core/eqn.c's table, kinds as there: 1 and 2 take that many
   arguments, 0 none), so a name outside unit 001 is parsed as the core parses it, then refused. */
typedef struct {
    const char *name;
    uint8_t args;
    uint8_t fn;
} name_t;

static const name_t NAMES[] = {
    {"LN", 1, F_LN}, {"LOG", 1, F_LOG}, {"EXP", 1, F_EXP}, {"ALOG", 1, F_ALOG}, {"SQ", 1, F_SQ},
    {"SQRT", 1, F_SQRT}, {"INV", 1, F_INV}, {"IP", 1, F_REFUSED}, {"FP", 1, F_REFUSED},
    {"RND", 1, F_REFUSED}, {"ABS", 1, F_ABS}, {"SGN", 1, F_REFUSED}, {"INTG", 1, F_REFUSED},
    {"IDIV", 2, F_REFUSED}, {"RMDR", 2, F_REFUSED}, {"SIN", 1, F_SIN}, {"COS", 1, F_COS}, {"TAN", 1, F_TAN},
    {"ASIN", 1, F_ASIN}, {"ACOS", 1, F_ACOS}, {"ATAN", 1, F_ATAN}, {"SINH", 1, F_SINH}, {"COSH", 1, F_COSH},
    {"TANH", 1, F_TANH}, {"ASINH", 1, F_ASINH}, {"ACOSH", 1, F_ACOSH}, {"ATANH", 1, F_ATANH},
    {"\xe2\x86\x92" "DEG", 1, F_REFUSED}, {"\xe2\x86\x92" "RAD", 1, F_REFUSED}, {"HMS\xe2\x86\x92", 1, F_REFUSED},
    {"\xe2\x86\x92" "HMS", 1, F_REFUSED}, {"%CHG", 2, F_REFUSED}, {"XROOT", 2, F_REFUSED},
    {"\xe2\x86\x92L", 1, F_REFUSED}, {"\xe2\x86\x92GAL", 1, F_REFUSED}, {"\xe2\x86\x92MILE", 1, F_REFUSED},
    {"\xe2\x86\x92KM", 1, F_REFUSED}, {"nCr", 2, F_REFUSED}, {"nPr", 2, F_REFUSED},
    {"\xe2\x86\x92KG", 1, F_REFUSED}, {"\xe2\x86\x92LB", 1, F_REFUSED}, {"\xe2\x86\x92\xc2\xb0" "C", 1, F_REFUSED},
    {"\xe2\x86\x92\xc2\xb0" "F", 1, F_REFUSED}, {"\xe2\x86\x92" "CM", 1, F_REFUSED},
    {"\xe2\x86\x92IN", 1, F_REFUSED}, {"SEED", 1, F_REFUSED}, {"ARG", 1, F_REFUSED}, {"RAND", 0, F_REFUSED},
    {"\xcf\x80", 0, F_REFUSED /* π: handled apart */}, {"sx", 0, F_REFUSED}, {"sy", 0, F_REFUSED},
    {"\xcf\x83x", 0, F_REFUSED}, {"\xcf\x83y", 0, F_REFUSED}, {"x\xcc\x84w", 0, F_REFUSED},
    {"x\xcc\x84", 0, F_REFUSED}, {"\xc8\xb3", 0, F_REFUSED}, {"x\xcc\x82", 1, F_REFUSED},
    {"\xc5\xb7", 1, F_REFUSED}, {"r", 0, F_REFUSED}, {"m", 0, F_REFUSED}, {"b", 0, F_REFUSED},
    {"n", 0, F_REFUSED}, {"\xce\xa3x\xc2\xb2", 0, F_REFUSED}, {"\xce\xa3y\xc2\xb2", 0, F_REFUSED},
    {"\xce\xa3xy", 0, F_REFUSED}, {"\xce\xa3x", 0, F_REFUSED}, {"\xce\xa3y", 0, F_REFUSED},
    {"REGX", 0, F_REFUSED}, {"REGY", 0, F_REFUSED}, {"REGZ", 0, F_REFUSED}, {"REGT", 0, F_REFUSED},
    {"AND", 2, F_REFUSED}, {"XOR", 2, F_REFUSED}, {"OR", 2, F_REFUSED}, {"NOT", 1, F_REFUSED},
    {"NAND", 2, F_REFUSED}, {"NOR", 2, F_REFUSED}, {"%", 2, F_REFUSED},
};
#define PI_UTF8 "\xcf\x80"
#define IND_I "\xe2\x92\xa4"    /* the indirect (i) and (j) glyphs, as core/eqn.c keeps them */
#define IND_J "\xe2\x92\xa5"

typedef struct {
    const char *s;
    int i;                      /* byte offset */
    casim_status st;            /* the first error */
    int errpos;                 /* SYNTAX: its character offset */
    const char *bad;            /* the first construct refused, in the text */
    int badlen;
    unsigned char *mem;
    size_t cap, used;
    node spare;                 /* what alloc gives once the arena is spent: a leaf (the number 0, not
                                   exact) that nothing writes, so a starved tree has no cycles */
    int var, angle;
    node *ints[6], *pi;         /* the derivative's constants, one node each (nodes never change) */
} ctx;

/* ---- the arena ---- */

static node *alloc(ctx *c, int kind)
{
    node *n;
    if (c->used + sizeof(node) > c->cap) {
        if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;
        return &c->spare;
    }
    n = (node *)(void *)(c->mem + c->used);
    c->used += sizeof(node);
    memset(n, 0, sizeof *n);
    n->kind = (uint8_t)kind;
    return n;
}

static node *mk2(ctx *c, int kind, node *a, node *b)
{
    node *n = alloc(c, kind);
    if (n == &c->spare) return n;
    n->u.k.a = a;
    n->u.k.b = b;
    return n;
}

static node *mk_fn(ctx *c, int fn, node *a)
{
    node *n = mk2(c, N_FN, a, NULL);
    if (n != &c->spare) n->id = (uint8_t)fn;
    return n;
}

static node *mk_abn(ctx *c, abn v)
{
    node *n = alloc(c, N_NUM);
    if (n == &c->spare) return n;
    n->exact = 1;
    n->u.v = v;
    return n;
}

/* The small whole numbers the rules use, each allocated once per call. */
static node *mk_int(ctx *c, int v)
{
    static const int16_t K[6] = {0, 1, 2, 10, 180, 200};
    for (int i = 0; i < 6; i++)
        if (K[i] == v) {
            if (!c->ints[i] || c->ints[i] == &c->spare) c->ints[i] = mk_abn(c, abn_from_int(v));
            return c->ints[i];
        }
    return mk_abn(c, abn_from_int(v));
}

static node *mk_pi(ctx *c)
{
    if (!c->pi || c->pi == &c->spare) c->pi = alloc(c, N_PI);
    return c->pi;
}

/* ---- the parser: core/eqn.c's grammar and its failure positions ---- */

static void fail(ctx *c, casim_status st)
{
    if (c->st == CASIM_OK) {
        int n = 0;
        for (int i = 0; i < c->i && c->s[i]; i++)
            if (((unsigned char)c->s[i] & 0xC0) != 0x80) n++;
        c->st = st;
        c->errpos = n;
    }
}

/* Refusals wait until the whole text has parsed: a syntax error anywhere outranks them. */
static void refuse(ctx *c, const char *at, int len)
{
    if (!c->bad) {
        c->bad = at;
        c->badlen = len;
    }
}

static bool at(ctx *c, const char *tok)
{
    size_t n = strlen(tok);
    if (strncmp(c->s + c->i, tok, n) == 0) {
        c->i += (int)n;
        return true;
    }
    return false;
}

static bool digit(char ch)
{
    return ch >= '0' && ch <= '9';
}

/* A number marked with a base: digits of that base, then b, o, d or h (core/eqn.c based_len). */
static int based_len(const char *s)
{
    int n = 0, b;
    while (digit(s[n]) || (s[n] >= 'A' && s[n] <= 'F')) n++;
    if (n == 0) return 0;
    b = s[n] == 'b' ? 2 : s[n] == 'o' ? 8 : s[n] == 'd' ? 10 : s[n] == 'h' ? 16 : 0;
    if (!b) return 0;
    for (int i = 0; i < n; i++)
        if ((digit(s[i]) ? s[i] - '0' : s[i] - 'A' + 10) >= b) return 0;
    return n + 1;
}

/* The digits, points and exponent of a decimal number (core/eqn.c decimal_len). */
static int decimal_len(const char *s, int *points, int *digits)
{
    int n = 0;
    *points = *digits = 0;
    while (s[n] == '.' || digit(s[n])) {
        if (s[n] == '.') (*points)++;
        else (*digits)++;
        n++;
    }
    if ((*digits || *points) && s[n] == 'E' && (s[n + 1] == '-' ? digit(s[n + 2]) : digit(s[n + 1]))) {
        n++;
        if (s[n] == '-') n++;
        while (digit(s[n])) n++;
    }
    return n;
}

static void parse_add(ctx *c, node **v);

/* The value the core reads for the n-byte decimal at s. Kept out of line: its buffer would otherwise
   sit in every frame of the recursive parser (S3 stack, measured). */
static __attribute__((noinline)) bool literal_value(abn *v, const char *s, int n)
{
    char buf[200];
    int k = 0;
    if (s[0] == '.') buf[k++] = '0';
    if (n + k >= (int)sizeof buf) return false;
    memcpy(buf + k, s, (size_t)n);
    buf[k + n] = '\0';
    return abn_from_text(v, buf) == ABN_OK;
}

/* A number as typed. A plain decimal keeps its text and the value the core reads; a fraction a.b.c
   keeps only its text; based and complex numbers are refused. */
static bool parse_number(ctx *c, node **v)
{
    const char *s = c->s + c->i;
    int n, points, digits;
    if ((n = based_len(s)) > 0) {
        refuse(c, s, n);
        c->i += n;
        *v = mk_int(c, 0);
        return true;
    }
    n = decimal_len(s, &points, &digits);
    if (n == 0) return false;
    if (!digits || points > 2) {
        c->i += n;
        fail(c, CASIM_SYNTAX);
        return true;
    }
    if (points == 2) {
        /* the core reads a fraction with an exponent, or without a denominator, as SYNTAX at its start */
        int d = 0, part = 0;
        for (int k = 0; k < n; k++) {
            if (s[k] == 'E') d = -1000;
            if (s[k] == '.') part++;
            else if (part == 2) d++;
        }
        if (d <= 0) {
            fail(c, CASIM_SYNTAX);
            c->i += n;
            return true;
        }
    }
    *v = alloc(c, N_NUM);
    if (*v != &c->spare) {
        (*v)->src = (uint16_t)c->i;
        (*v)->len = (uint8_t)n;
    }
    if (points < 2 && *v != &c->spare) (*v)->exact = literal_value(&(*v)->u.v, s, n);
    c->i += n;
    if (at(c, "i") || at(c, "\xce\xb8")) {
        int n2;
        at(c, "-");
        n2 = decimal_len(c->s + c->i, &points, &digits);
        if (n2 == 0 || !digits || points > 1) {
            fail(c, CASIM_SYNTAX);
            return true;
        }
        c->i += n2;
        refuse(c, s, (int)(c->s + c->i - s));
    }
    return true;
}

static void parse_prim(ctx *c, node **v)
{
    const char *s = c->s + c->i;
    *v = &c->spare;
    if (c->st != CASIM_OK) return;
    if (parse_number(c, v)) return;
    if (at(c, "(")) {
        parse_add(c, v);
        if (!at(c, ")")) fail(c, CASIM_SYNTAX);
        return;
    }
    if (at(c, "[")) {
        node *e;
        int k = 0;
        refuse(c, s, 1);
        do {
            if (k == 3) {
                fail(c, CASIM_SYNTAX);
                return;
            }
            k++;
            parse_add(c, &e);
        } while (c->st == CASIM_OK && at(c, ","));
        if (c->st == CASIM_OK && !at(c, "]")) fail(c, CASIM_SYNTAX);
        return;
    }
    {
        /* a name: the longest that fits; a function name needs its "(" */
        const name_t *best = NULL;
        size_t bl = 0;
        for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
            size_t n = strlen(NAMES[k].name);
            if (n > bl && strncmp(s, NAMES[k].name, n) == 0 && (NAMES[k].args == 0 || s[n] == '(')) {
                best = &NAMES[k];
                bl = n;
            }
        }
        if (best) {
            node *a, *b;
            c->i += (int)bl;
            if (best->args == 0) {
                if (strcmp(best->name, PI_UTF8) == 0) *v = alloc(c, N_PI);
                else refuse(c, s, (int)bl);
                return;
            }
            c->i++;                     /* the "(" */
            parse_add(c, &a);
            if (best->args == 2) {
                if (!at(c, ",")) {
                    fail(c, CASIM_SYNTAX);
                    return;
                }
                parse_add(c, &b);
            }
            if (!at(c, ")")) {
                fail(c, CASIM_SYNTAX);
                return;
            }
            if (best->fn == F_REFUSED) refuse(c, s, (int)bl);
            else *v = mk_fn(c, best->fn, a);
            return;
        }
    }
    if (at(c, IND_I) || at(c, IND_J)) {
        refuse(c, s, 3);
        return;
    }
    if ((*s >= 'A' && *s <= 'Z') || *s == 'i') {
        /* A-Z, and i: the 33s's variable i (33s p.13-23, core unit 025); the 35s rejects a lone i
           before casim could see it */
        c->i++;
        *v = alloc(c, N_VAR);
        if (*v != &c->spare) (*v)->id = (uint8_t)(*s == 'i' ? 26 : *s - 'A');
        return;
    }
    fail(c, CASIM_SYNTAX);
}

static void parse_post(ctx *c, node **v)
{
    parse_prim(c, v);
    while (c->st == CASIM_OK && c->s[c->i] == '!') {
        refuse(c, c->s + c->i, 1);
        c->i++;
    }
}

static void parse_parg(ctx *c, node **v)
{
    if (at(c, "-")) {
        node *a;
        parse_parg(c, &a);
        *v = mk2(c, N_NEG, a, NULL);
        return;
    }
    parse_post(c, v);
}

static void parse_pow(ctx *c, node **v)
{
    parse_post(c, v);
    while (c->st == CASIM_OK && at(c, "^")) {
        node *e;
        parse_parg(c, &e);
        *v = mk2(c, N_POW, *v, e);
    }
}

static void parse_neg(ctx *c, node **v)
{
    if (at(c, "-")) {
        node *a;
        parse_neg(c, &a);
        *v = mk2(c, N_NEG, a, NULL);
        return;
    }
    parse_pow(c, v);
}

static void parse_mul(ctx *c, node **v)
{
    parse_neg(c, v);
    while (c->st == CASIM_OK) {
        int kind;
        node *r;
        if (at(c, "\xc3\x97")) kind = N_MUL;
        else if (at(c, "\xc3\xb7")) kind = N_DIV;
        else break;
        parse_neg(c, &r);
        *v = mk2(c, kind, *v, r);
    }
}

static void parse_add(ctx *c, node **v)
{
    parse_mul(c, v);
    while (c->st == CASIM_OK) {
        int kind;
        node *r;
        if (at(c, "+")) kind = N_ADD;
        else if (at(c, "-")) kind = N_SUB;
        else break;
        parse_mul(c, &r);
        *v = mk2(c, kind, *v, r);
    }
}

static node *parse(ctx *c)
{
    node *l, *r;
    parse_add(c, &l);
    if (c->st == CASIM_OK && at(c, "=")) {
        parse_add(c, &r);
        l = mk2(c, N_EQ, l, r);
    }
    if (c->st == CASIM_OK && c->s[c->i] != '\0') fail(c, CASIM_SYNTAX);
    if (c->st == CASIM_OK && c->bad) c->st = CASIM_NOT_DIFFERENTIABLE;
    return l;
}

/* ---- constants, folded only when the result is exact (unit CAS 001 rule 5) ---- */

/* The value the core reads for n, if n is a number (or the negation of one) known exactly. */
static bool num_of(const node *n, abn *v)
{
    if (n->kind == N_NUM && n->exact) {
        *v = n->u.v;
        return true;
    }
    if (n->kind == N_NEG && num_of(n->u.k.a, v)) {
        *v = abn_neg(v);
        return true;
    }
    return false;
}

static bool is_int(const node *n, int k)
{
    abn v, w = abn_from_int(k);
    return num_of(n, &v) && abn_cmp(&v, &w) == 0;
}

/* The exponents of a's leading and last significant digits. */
static void span(const abn *a, int *lead, int *last)
{
    bool neg;
    char d[ABN_DIGITS + 2];
    abn_digits(a, &neg, d, lead);
    *last = *lead - (int)strlen(d) + 1;
}

/* a + b exactly, when the sum fits in the engine's digits (its span, carry included). */
static bool add_exact(abn *r, const abn *a, const abn *b)
{
    int la, ta, lb, tb;
    if (abn_is_zero(a)) return *r = *b, true;
    if (abn_is_zero(b)) return *r = *a, true;
    span(a, &la, &ta);
    span(b, &lb, &tb);
    if ((la > lb ? la : lb) + 1 - (ta < tb ? ta : tb) + 1 > ABN_DIGITS) return false;
    return abn_add(r, a, b) == ABN_OK;
}

/* a / b exactly: the quotient times b gives a back with no rounding. */
static bool div_exact(abn *r, const abn *a, const abn *b)
{
    abn q, back;
    if (abn_is_zero(b) || abn_div(&q, a, b) != ABN_OK) return false;
    if (abn_mul_exact(&back, &q, b) != ABN_OK || abn_cmp(&back, a) != 0) return false;
    *r = q;
    return true;
}

/* a ^ k for a small whole k, exactly. */
static bool pow_exact(abn *r, const abn *a, const abn *k)
{
    abn acc = abn_from_int(1), lim = abn_from_int(64), one = abn_from_int(1), i = abn_zero();
    if (!abn_is_integer(k) || abn_is_negative(k) || abn_cmp(k, &lim) > 0) return false;
    while (abn_cmp(&i, k) < 0)
        if (abn_mul_exact(&acc, &acc, a) != ABN_OK || abn_add(&i, &i, &one) != ABN_OK) return false;
    *r = acc;
    return true;
}

/* ---- the derivative's constructors: rule 5's simplifications, and three more (u-u, 0÷u, d 0^v)
   that keep a derivative defined wherever its function is ---- */

/* The same expression, node for node. */
static bool same(const ctx *c, const node *a, const node *b)
{
    if (a == b) return true;
    if (a->kind != b->kind || a->id != b->id) return false;
    switch (a->kind) {
    case N_NUM:
        if (a->exact != b->exact) return false;
        if (a->exact) return abn_cmp(&a->u.v, &b->u.v) == 0;
        return a->len == b->len && memcmp(c->s + a->src, c->s + b->src, a->len) == 0;   /* a fraction's text */
    case N_VAR:
    case N_PI: return true;
    case N_NEG:
    case N_FN: return same(c, a->u.k.a, b->u.k.a);
    default: return same(c, a->u.k.a, b->u.k.a) && same(c, a->u.k.b, b->u.k.b);
    }
}

static node *s_neg(ctx *c, node *a)
{
    abn v;
    if (a->kind == N_NEG) return a->u.k.a;                    /* -(-u) is u */
    if (num_of(a, &v)) return mk_abn(c, abn_neg(&v));
    return mk2(c, N_NEG, a, NULL);
}

static node *s_add(ctx *c, node *a, node *b)
{
    abn x, y, r;
    if (is_int(a, 0)) return b;
    if (is_int(b, 0)) return a;
    if (num_of(a, &x) && num_of(b, &y) && add_exact(&r, &x, &y)) return mk_abn(c, r);
    return mk2(c, N_ADD, a, b);
}

static node *s_sub(ctx *c, node *a, node *b)
{
    abn x, y, r;
    if (is_int(b, 0)) return a;
    if (is_int(a, 0)) return s_neg(c, b);                     /* 0-u is -u */
    if (same(c, a, b)) return mk_int(c, 0);                      /* u-u is 0 (ours, beyond rule 5) */
    if (num_of(a, &x) && num_of(b, &y)) {
        y = abn_neg(&y);
        if (add_exact(&r, &x, &y)) return mk_abn(c, r);
    }
    return mk2(c, N_SUB, a, b);
}

static node *s_mul(ctx *c, node *a, node *b)
{
    abn x, y, r;
    if (is_int(a, 0) || is_int(b, 0)) return mk_int(c, 0);    /* 0×u and u×0 are 0 */
    if (is_int(a, 1)) return b;
    if (is_int(b, 1)) return a;
    if (num_of(a, &x) && num_of(b, &y) && abn_mul_exact(&r, &x, &y) == ABN_OK) return mk_abn(c, r);
    return mk2(c, N_MUL, a, b);
}

static node *s_div(ctx *c, node *a, node *b)
{
    abn x, y, r;
    if (is_int(b, 1)) return a;
    if (is_int(a, 0)) return a;                               /* 0÷u is 0 (ours, beyond rule 5) */
    if (num_of(a, &x) && num_of(b, &y) && div_exact(&r, &x, &y)) return mk_abn(c, r);
    return mk2(c, N_DIV, a, b);
}

static node *s_pow(ctx *c, node *a, node *b)
{
    abn x, y, r;
    if (is_int(b, 1)) return a;
    if (is_int(b, 0)) return mk_int(c, 1);
    if (num_of(a, &x) && num_of(b, &y) && pow_exact(&r, &x, &y)) return mk_abn(c, r);
    return mk2(c, N_POW, a, b);
}

/* ---- the derivative ---- */

/* A derivative in the angle mode: the trigonometric functions read their argument in degrees or
   grads, so d/du SIN(u) carries π/180 (π/200); the inverses return them, so they carry 180/π. */
static node *trig(ctx *c, node *e)
{
    if (c->angle == CASIM_RAD) return e;
    return s_div(c, s_mul(c, e, mk_pi(c)), mk_int(c, c->angle == CASIM_DEG ? 180 : 200));
}

static node *itrig(ctx *c, node *e)
{
    if (c->angle == CASIM_RAD) return e;
    return s_div(c, s_mul(c, e, mk_int(c, c->angle == CASIM_DEG ? 180 : 200)), mk_pi(c));
}

static node *fn(ctx *c, int f, node *a)
{
    return mk_fn(c, f, a);
}

static node *deriv(ctx *c, node *n);

/* d/dx f(u) for f(u) = n, by the chain rule: f'(u)·u'. */
static node *deriv_fn(ctx *c, node *n)
{
    node *u = n->u.k.a, *du = deriv(c, u), *one = NULL;
    if (is_int(du, 0)) return du;
    switch (n->id) {
    case F_SQ: return s_mul(c, s_mul(c, mk_int(c, 2), u), du);
    case F_SQRT: return s_div(c, du, s_mul(c, mk_int(c, 2), n));
    case F_INV: return s_neg(c, s_div(c, du, fn(c, F_SQ, u)));
    case F_LN: return s_div(c, du, u);
    case F_LOG: return s_div(c, du, s_mul(c, u, fn(c, F_LN, mk_int(c, 10))));
    case F_EXP: return s_mul(c, n, du);
    case F_ALOG: return s_mul(c, s_mul(c, n, fn(c, F_LN, mk_int(c, 10))), du);
    case F_SIN: return trig(c, s_mul(c, fn(c, F_COS, u), du));
    case F_COS: return trig(c, s_mul(c, s_neg(c, fn(c, F_SIN, u)), du));
    case F_TAN: return trig(c, s_div(c, du, fn(c, F_SQ, fn(c, F_COS, u))));
    case F_ASIN: one = mk_int(c, 1); return itrig(c, s_div(c, du, fn(c, F_SQRT, s_sub(c, one, fn(c, F_SQ, u)))));
    case F_ACOS:
        one = mk_int(c, 1);
        return itrig(c, s_neg(c, s_div(c, du, fn(c, F_SQRT, s_sub(c, one, fn(c, F_SQ, u))))));
    case F_ATAN: return itrig(c, s_div(c, du, s_add(c, mk_int(c, 1), fn(c, F_SQ, u))));
    case F_SINH: return s_mul(c, fn(c, F_COSH, u), du);
    case F_COSH: return s_mul(c, fn(c, F_SINH, u), du);
    case F_TANH: return s_div(c, du, fn(c, F_SQ, fn(c, F_COSH, u)));
    case F_ASINH: return s_div(c, du, fn(c, F_SQRT, s_add(c, fn(c, F_SQ, u), mk_int(c, 1))));
    case F_ACOSH: return s_div(c, du, fn(c, F_SQRT, s_sub(c, fn(c, F_SQ, u), mk_int(c, 1))));
    case F_ATANH: return s_div(c, du, s_sub(c, mk_int(c, 1), fn(c, F_SQ, u)));
    case F_ABS: return s_mul(c, fn(c, F_SGN, u), du);        /* right except at u = 0 */
    }
    return mk_int(c, 0);    /* not reached: the parser refuses every other function */
}

static node *deriv(ctx *c, node *n)
{
    node *a = n->u.k.a, *b = n->u.k.b, *da, *db;
    if (c->st != CASIM_OK) return &c->spare;
    switch (n->kind) {
    case N_NUM:
    case N_PI: return mk_int(c, 0);
    case N_VAR: return mk_int(c, n->id == c->var);
    case N_NEG: return s_neg(c, deriv(c, a));
    case N_FN: return deriv_fn(c, n);
    case N_EQ: return mk2(c, N_EQ, deriv(c, a), deriv(c, b));
    }
    da = deriv(c, a);
    db = deriv(c, b);
    switch (n->kind) {
    case N_ADD: return s_add(c, da, db);
    case N_SUB: return s_sub(c, da, db);
    case N_MUL: return s_add(c, s_mul(c, da, b), s_mul(c, a, db));         /* (uv)' = u'v + uv' */
    case N_DIV:                                                            /* (u/v)' = (u'v - uv')/v² */
        if (is_int(db, 0)) return s_div(c, da, b);
        return s_div(c, s_sub(c, s_mul(c, da, b), s_mul(c, a, db)), fn(c, F_SQ, b));
    default:                                                               /* N_POW, u^v */
        if (is_int(db, 0)) {
            /* v constant: v·u^(v-1)·u' */
            if (is_int(da, 0)) return da;
            return s_mul(c, s_mul(c, b, s_pow(c, a, s_sub(c, b, mk_int(c, 1)))), da);
        }
        if (is_int(a, 0)) return mk_int(c, 0);         /* 0^v is 0 wherever it is defined (v > 0) */
        if (is_int(da, 0)) return s_mul(c, s_mul(c, n, fn(c, F_LN, a)), db);   /* u constant: u^v·ln u·v' */
        /* u^v·(v'·ln u + v·u'/u) */
        return s_mul(c, n, s_add(c, s_mul(c, db, fn(c, F_LN, a)), s_div(c, s_mul(c, b, da), a)));
    }
}

/* ---- the printer ---- */

typedef struct {
    char *o;
    size_t cap, n;
    bool full;
} out_t;

static void put(out_t *w, const char *s, size_t len)
{
    if (w->full || w->n + len + 1 > w->cap) {
        w->full = true;
        return;
    }
    memcpy(w->o + w->n, s, len);
    w->n += len;
    w->o[w->n] = '\0';
}

static void puts_(out_t *w, const char *s)
{
    put(w, s, strlen(s));
}

static bool neg_num(const node *n)
{
    return n->kind == N_NUM && n->len == 0 && abn_is_negative(&n->u.v);
}

/* The grammar level a node prints at: 1 + -, 2 × ÷, 3 unary minus, 4 ^, 5 a primary. */
static int level(const node *n)
{
    switch (n->kind) {
    case N_ADD: case N_SUB: return 1;
    case N_MUL: case N_DIV: return 2;
    case N_NEG: return 3;
    case N_POW: return 4;
    case N_NUM: return neg_num(n) ? 3 : 5;
    default: return 5;
    }
}

/* A computed number: plain where it is short, else d.dddEn, in the forms core/eqn.c reads. Kept out of
   line, so its buffers are not in every frame of the recursive printer. */
static __attribute__((noinline)) void put_num(out_t *w, const abn *v)
{
    bool neg;
    char d[ABN_DIGITS + 2], buf[ABN_DIGITS + 48];
    int lead, nd, k = 0;
    abn_digits(v, &neg, d, &lead);
    nd = (int)strlen(d);
    if (nd == 0) {
        puts_(w, "0");
        return;
    }
    if (neg) buf[k++] = '-';
    if (lead >= 0 && lead < ABN_DIGITS) {
        for (int i = 0; i <= lead; i++) buf[k++] = i < nd ? d[i] : '0';
        if (nd > lead + 1) {
            buf[k++] = '.';
            for (int i = lead + 1; i < nd; i++) buf[k++] = d[i];
        }
    } else if (lead < 0 && lead >= -6) {
        buf[k++] = '0';
        buf[k++] = '.';
        for (int i = -1; i > lead; i--) buf[k++] = '0';
        for (int i = 0; i < nd; i++) buf[k++] = d[i];
    } else {
        int e = lead < 0 ? -lead : lead, p = 0;
        char ex[8];
        buf[k++] = d[0];
        if (nd > 1) {
            buf[k++] = '.';
            for (int i = 1; i < nd; i++) buf[k++] = d[i];
        }
        buf[k++] = 'E';
        if (lead < 0) buf[k++] = '-';
        do ex[p++] = (char)('0' + e % 10); while ((e /= 10) && p < 8);
        while (p) buf[k++] = ex[--p];
    }
    put(w, buf, (size_t)k);
}

static void print(ctx *c, out_t *w, const node *n);

static void print_in(ctx *c, out_t *w, const node *n, bool paren)
{
    if (paren) puts_(w, "(");
    print(c, w, n);
    if (paren) puts_(w, ")");
}

/* An exponent prints bare when parg reads it whole: a primary, or minus signs before one. */
static bool parg_ok(const node *n)
{
    if (n->kind == N_NEG) return level(n->u.k.a) < 3 || parg_ok(n->u.k.a);   /* -(…) reads as - prim */
    return level(n) == 5 || neg_num(n);
}

static void print(ctx *c, out_t *w, const node *n)
{
    static const char *const OPS[] = {[N_ADD] = "+", [N_SUB] = "-", [N_MUL] = "\xc3\x97",
                                      [N_DIV] = "\xc3\xb7", [N_POW] = "^", [N_EQ] = "="};
    const node *a = n->u.k.a, *b = n->u.k.b;
    if (w->full) return;
    switch (n->kind) {
    case N_NUM:
        if (n->len) put(w, c->s + n->src, n->len);
        else put_num(w, &n->u.v);
        return;
    case N_VAR:
        if (n->id == 26) puts_(w, "i");
        else {
            char ch = (char)('A' + n->id);
            put(w, &ch, 1);
        }
        return;
    case N_PI: puts_(w, PI_UTF8); return;
    case N_NEG:
        puts_(w, "-");
        print_in(c, w, a, level(a) < 3);
        return;
    case N_FN:
        puts_(w, FNAME[n->id]);
        print_in(c, w, a, true);
        return;
    case N_EQ:
        print(c, w, a);
        puts_(w, "=");
        print(c, w, b);
        return;
    case N_ADD:
    case N_SUB:
    case N_MUL:
    case N_DIV: {
        int lv = level(n);
        print_in(c, w, a, level(a) < lv);
        puts_(w, OPS[n->kind]);
        print_in(c, w, b, level(b) <= lv);     /* left to right: an equal level on the right needs () */
        return;
    }
    default:                                   /* N_POW: post { "^" parg }, left to right */
        print_in(c, w, a, level(a) < 4);
        puts_(w, "^");
        print_in(c, w, b, !parg_ok(b));
        return;
    }
}

/* ---- the API ---- */

static casim_status finish(ctx *c, const node *n, char *out, size_t cap)
{
    out_t w = {out, cap, 0, false};
    if (cap == 0) return CASIM_TOO_LONG;
    out[0] = '\0';
    if (c->st == CASIM_OK) {
        print(c, &w, n);
        if (w.full) c->st = CASIM_TOO_LONG;
    }
    if (c->st == CASIM_SYNTAX) {
        char num[12];
        int k = 0, e = c->errpos;
        do num[k++] = (char)('0' + e % 10); while ((e /= 10) && k < 11);
        while (k) {
            char ch = num[--k];
            put(&w, &ch, 1);
        }
    } else if (c->st == CASIM_NOT_DIFFERENTIABLE) put(&w, c->bad, (size_t)c->badlen);
    if (w.full) out[0] = '\0';
    return c->st;
}

static void start(ctx *c, const char *eqn, void *arena, size_t arena_cap)
{
    uintptr_t p = (uintptr_t)arena, q = (p + 7u) & ~(uintptr_t)7u;
    memset(c, 0, sizeof *c);
    c->s = eqn;
    c->mem = (unsigned char *)arena + (q - p);
    c->cap = arena_cap > q - p ? arena_cap - (q - p) : 0;
}

casim_status casim_derive(const char *eqn, int var, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap)
{
    ctx c;
    node *n;
    if (cap) out[0] = '\0';
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, arena, arena_cap);
    c.var = var;
    c.angle = angle;
    n = parse(&c);
    if (c.st == CASIM_OK) n = deriv(&c, n);
    return finish(&c, n, out, cap);
}

casim_status casim_reprint(const char *eqn, char *out, size_t cap, void *arena, size_t arena_cap)
{
    ctx c;
    node *n;
    if (cap) out[0] = '\0';
    start(&c, eqn, arena, arena_cap);
    n = parse(&c);
    return finish(&c, n, out, cap);
}

casim_status casim_simplify(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    (void)eqn, (void)angle, (void)arena, (void)arena_cap;     /* a stub until unit CAS 002 lands */
    if (cap) out[0] = '\0';
    return CASIM_TOO_LONG;
}
