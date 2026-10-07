/* Casimir (casim): units CAS 001 (the derivative) and CAS 002 (simplify, to a canonical form).
 * docs/canonical-form.md is the contract this file implements; in brief:
 *
 * - parse: an iterative operator-precedence parser over the core's equation grammar (core/eqn.h's
 *   grammar comment, core/eqn.c read as the spec), failing where and as the core's check-only pass
 *   fails. Its stacks live at the top of the arena, so its C stack does not grow with nesting.
 * - canon: the raw tree to the canonical one, bottom-up; chains of one operator are walked in a loop.
 * - numbers: exact rationals p/q on the core's decimal128 (abn), reduced by Euclid's algorithm (Knuth,
 *   TAOCP vol. 2, 4.5.2); nothing through binary floating point.
 * - derive: the standard rules (Wikipedia "Differentiation rules", "Differentiation of trigonometric
 *   functions") on the canonical tree, built through the canonical constructors.
 * - print: back to the core's grammar, readable, and read back to the same canonical tree.
 * All memory is the caller's arena: 8-byte nodes with 16-bit references (offsets in 8-byte units).
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "abn.h"
#include "casim.h"

/* ---- nodes ---- */

enum {
    /* canonical (VAR and PI are raw too) */
    K_NUM, K_VAR, K_PI, K_FN, K_POW, K_MUL, K_ADD, K_EQ, K_LIT,
    K_ONAME,                    /* an opaque name (CAS 003): id its NAMES index, or (i) and (j) */
    K_OFN,                      /* an opaque function: id its NAMES index, or ! ; a, b its arguments */
    /* raw only */
    K_NEG, K_BADD, K_BSUB, K_BMUL, K_BDIV, K_BPOW, K_RFN, K_REQ, K_BAD,
    K_GROUP,                    /* a chain in written parentheses: a chain's walk stops there */
    K_ROFN,                     /* an opaque function as parsed */
    K_FWD                       /* a node moved by compact(): a is where it went */
};

/* The ids of the opaque kinds that are not NAMES entries. */
#define ID_FACT 255             /* postfix ! (an opaque function of one argument) */
#define ID_IND_I 254            /* the indirect (i) and (j) (opaque names) */
#define ID_IND_J 253
#define M_OPAQUE 128            /* an M_FN opener's fn at or past this: an opaque name, fn - M_OPAQUE */

/* The functions casim knows (SGN only appears in a derivative). */
enum {
    F_SQ, F_SQRT, F_INV, F_LN, F_LOG, F_EXP, F_ALOG, F_SIN, F_COS, F_TAN, F_ASIN, F_ACOS, F_ATAN, F_SINH,
    F_COSH, F_TANH, F_ASINH, F_ACOSH, F_ATANH, F_ABS, F_SGN, F_REFUSED
};

static const char *const FNAME[] = {
    "SQ", "SQRT", "INV", "LN", "LOG", "EXP", "ALOG", "SIN", "COS", "TAN", "ASIN", "ACOS", "ATAN", "SINH",
    "COSH", "TANH", "ASINH", "ACOSH", "ATANH", "ABS", "SGN",
};

typedef uint16_t ref;           /* an arena offset in 8-byte units; 0 is none */

typedef struct {
    uint8_t kind;
    uint8_t id;                 /* K_FN, K_RFN: the function; K_VAR: 0-25 for A-Z, 26 for i */
    uint16_t n;                 /* K_ADD, K_MUL: operands; K_LIT: the text's length */
    ref a, b;                   /* operands; K_ADD, K_MUL: a is the operand list; K_NUM: a is the rational;
                                   K_LIT: a is the text's byte offset */
} node;

typedef struct {
    int64_t p, q;               /* p/q: see "rationals, exactly" */
} rat;

/* Every name of the core's grammar (core/eqn.c's table; args as there: 1 and 2 take that many, 0 none),
   so a name outside casim's set is parsed as the core parses it, then refused. */
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
#define TIMES "\xc3\x97"
#define DIVIDE "\xc3\xb7"

/* The constants the rules use, one node each per call. */
enum { C_M1, C_0, C_1, C_2, C_10, C_180, C_200, C_HALF, C_MHALF, C_M2, SMALL };

typedef struct {
    const char *s;
    casim_status st;            /* the first error */
    int errpos;                 /* SYNTAX: its character offset */
    const char *bad;            /* the first construct refused, in the text */
    int badlen;
    unsigned char *mem;
    size_t cap, used, top;      /* nodes grow up from used; the parser's stacks down from top */
    int var, angle;
    int depth;                  /* the recursive walks' nesting, bounded by CASIM_DEPTH_MAX */
    bool opaque;                /* the names derive refuses are opaque (every operation but derive) */
    bool no_eq;                 /* an = is SYNTAX (subst's replacement is an expression) */
    bool has_impure;            /* RAND or SEED occurs: impure subtrees exist (CAS 003, #4264 point 2) */
    int rand_id, seed_id;       /* their NAMES indexes */
    const char *s2;             /* subst's replacement text (its literals have id 1) */
    int sub_var, sub_count;     /* subst: the variable replaced, and how often */
    ref sub_val;                /* its canonical replacement */
    ref small[SMALL], pi;
} ctx;

#define SPARE ((ref)1)          /* what an exhausted arena gives: a leaf that nothing writes */

static node *nd(const ctx *c, ref r)
{
    return (node *)(void *)(c->mem + (size_t)r * 8u);
}

static ref *list(const ctx *c, ref r)
{
    return (ref *)(void *)(c->mem + (size_t)r * 8u);
}

static rat *rt(const ctx *c, ref r)
{
    return (rat *)(void *)(c->mem + (size_t)r * 8u);
}

/* ---- the depth guard ---- */

/* Every structural walk (canonical form, derivative, comparison, printing) enters through enter(): past
   CASIM_DEPTH_MAX nested entries the call returns TOO_LONG. So the stack is bounded by construction, not
   by the inputs measured: the limit was set on the S3 so that the deepest case of every kind of nesting
   stays within 12 KB (unit CAS 002b; abacus's ruling, #4236). The sum, product and power constructors
   are not counted: they recurse only one level on their own, and their depth is the walks' (counting them
   too made a nest of sums cost two units a level, against one for a nest of powers, at the same stack). */
#ifndef CASIM_DEPTH_MAX
#define CASIM_DEPTH_MAX 48
#endif

static bool enter(ctx *c)
{
    if (c->depth >= CASIM_DEPTH_MAX) {
        if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;
        return false;
    }
    c->depth++;
    return true;
}

/* The guarded walks (each a wrapper of the body named with a trailing _). */
static int cmp(const ctx *c, ref a, ref b);
static ref canon(ctx *c, ref r);
static ref deriv(ctx *c, ref r);

/* ---- the arena ---- */

static ref grab(ctx *c, size_t bytes)
{
    size_t need = (bytes + 7u) & ~(size_t)7u;
    ref r;
    if (need == 0) need = 8;
    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_TOO_LONG;
    if (c->st != CASIM_OK) return 0;
    r = (ref)(c->used / 8u);
    memset(c->mem + c->used, 0, need);
    c->used += need;
    return r;
}

static ref mk(ctx *c, int kind, ref a, ref b)
{
    ref r = grab(c, sizeof(node));
    node *n;
    if (!r) return SPARE;
    n = nd(c, r);
    n->kind = (uint8_t)kind;
    n->a = a;
    n->b = b;
    return r;
}

static ref leaf(ctx *c, int kind, int id)
{
    ref r = mk(c, kind, 0, 0);
    if (r != SPARE) nd(c, r)->id = (uint8_t)id;
    return r;
}

/* ---- rationals, exactly ---- */

/* p/q with q > 0 and gcd(p, q) = 1, |p| and q below 2^62: 64-bit integers, every operation checked for
   overflow. A result that would not fit is not folded, and its operands stay as they are. (The core's
   decimal128 did this exactly too, at 15 to 45 times the S3's time: measured, CAS 002.) */
#define RAT_MAX ((int64_t)1 << 62)

static bool fits(int64_t v)
{
    return v > -RAT_MAX && v < RAT_MAX;
}

static bool mul64(int64_t *r, int64_t a, int64_t b)
{
    return !__builtin_mul_overflow(a, b, r) && fits(*r);
}

static bool add64(int64_t *r, int64_t a, int64_t b)
{
    return !__builtin_add_overflow(a, b, r) && fits(*r);
}

/* gcd(|a|, |b|) by Euclid's algorithm. */
static int64_t gcd64(int64_t a, int64_t b)
{
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) {
        int64_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

/* p/q reduced, q made positive; false when q is 0 or a part does not fit. */
static bool rat_make(rat *r, int64_t p, int64_t q)
{
    int64_t g;
    if (q == 0 || !fits(p) || !fits(q)) return false;
    if (q < 0) p = -p, q = -q;
    g = p == 0 ? q : gcd64(p, q);
    r->p = p / g;
    r->q = q / g;
    return true;
}

static void rat_int(rat *r, int k)
{
    r->p = k;
    r->q = 1;
}

static bool rat_add(rat *r, const rat *x, const rat *y)
{
    int64_t g = gcd64(x->q, y->q), a, b, p, q;
    return mul64(&a, x->p, y->q / g) && mul64(&b, y->p, x->q / g) && add64(&p, a, b) &&
           mul64(&q, x->q / g, y->q) && rat_make(r, p, q);
}

static bool rat_mul(rat *r, const rat *x, const rat *y)
{
    int64_t g1, g2, p, q;
    if (x->p == 0 || y->p == 0) {
        rat_int(r, 0);
        return true;
    }
    g1 = gcd64(x->p, y->q);
    g2 = gcd64(y->p, x->q);
    return mul64(&p, x->p / g1, y->p / g2) && mul64(&q, x->q / g2, y->q / g1) && rat_make(r, p, q);
}

/* x^k for a whole k, |k| <= 4096, by squaring. */
static bool rat_pow(rat *r, const rat *x, long k)
{
    rat acc, base = *x;
    if (k < -4096 || k > 4096) return false;
    if (k < 0) {
        if (!rat_make(&base, x->q, x->p)) return false;
        k = -k;
    }
    rat_int(&acc, 1);
    while (k) {
        if ((k & 1) && !rat_mul(&acc, &acc, &base)) return false;
        k >>= 1;
        if (k && !rat_mul(&base, &base, &base)) return false;
    }
    *r = acc;
    return true;
}

static bool rat_is(const rat *x, int p, int q)
{
    return x->p == p && x->q == q;
}

/* x as a whole number k with |k| <= 4096. */
static bool rat_whole(const rat *x, long *k)
{
    if (x->q != 1 || x->p > 4096 || x->p < -4096) return false;
    *k = (long)x->p;
    return true;
}

/* a/b against c/d (b, d > 0), exactly, without a product that could overflow: the whole parts, then the
   remainders' reciprocals in reverse (the continued fractions' terms, one by one). */
static int frac_cmp(int64_t a, int64_t b, int64_t c, int64_t d)
{
    int sign = 1;
    if ((a < 0) != (c < 0)) return a < 0 ? -1 : 1;
    if (a < 0) a = -a, c = -c, sign = -1;
    for (;;) {
        int64_t qa = a / b, qc = c / d, ra = a % b, rc = c % d, t;
        if (qa != qc) return qa < qc ? -sign : sign;
        if (ra == 0 || rc == 0) return ra == rc ? 0 : ra == 0 ? -sign : sign;
        t = b, b = ra, a = t;           /* ra/b against rc/d is b/ra against d/rc, reversed */
        t = d, d = rc, c = t;
        sign = -sign;
    }
}

/* A total order: by value, ties (none: reduced forms are unique) on p then q. */
static int rat_cmp(const rat *x, const rat *y)
{
    int s;
    if (x->p == y->p && x->q == y->q) return 0;
    if ((s = frac_cmp(x->p, x->q, y->p, y->q)) != 0) return s;
    return x->p != y->p ? (x->p < y->p ? -1 : 1) : (x->q < y->q ? -1 : 1);
}

static const int64_t POW10[19] = {
    1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000, 10000000000LL,
    100000000000LL, 1000000000000LL, 10000000000000LL, 100000000000000LL, 1000000000000000LL,
    10000000000000000LL, 100000000000000000LL, 1000000000000000000LL,
};

/* The rational of a decimal value of the core (a literal as the core reads it), when it fits. */
static bool rat_from_abn(rat *r, const abn *x)
{
    bool neg;
    char d[ABN_DIGITS + 2];
    int lead, nd_, e;
    int64_t m = 0;
    abn_digits(x, &neg, d, &lead);
    nd_ = (int)strlen(d);
    if (nd_ == 0) {
        rat_int(r, 0);
        return true;
    }
    for (int i = 0; i < nd_; i++)          /* as many digits as fit: the rule decimal_of prints by */
        if (!mul64(&m, m, 10) || !add64(&m, m, d[i] - '0')) return false;
    if (neg) m = -m;
    e = lead - nd_ + 1;
    if (e >= 0) return e <= 18 && mul64(&m, m, POW10[e]) && rat_make(r, m, 1);
    return -e <= 18 && rat_make(r, m, POW10[-e]);
}

/* ---- numbers as nodes ---- */

static ref num(ctx *c, const rat *v)
{
    ref r = grab(c, sizeof(rat));
    if (!r) return SPARE;
    *rt(c, r) = *v;
    return mk(c, K_NUM, r, 0);
}

static ref small(ctx *c, int k)
{
    static const int16_t P[SMALL] = {-1, 0, 1, 2, 10, 180, 200, 1, -1, -2};
    static const uint8_t Q[SMALL] = {1, 1, 1, 1, 1, 1, 1, 2, 2, 1};
    if (!c->small[k] || c->small[k] == SPARE) {
        rat v;
        rat_make(&v, P[k], Q[k]);
        c->small[k] = num(c, &v);
    }
    return c->small[k];
}

static bool is_num(const ctx *c, ref r)
{
    return r != SPARE && nd(c, r)->kind == K_NUM;
}

static const rat *val(const ctx *c, ref r)
{
    return rt(c, nd(c, r)->a);
}

static bool is_k(const ctx *c, ref r, int p, int q)
{
    return is_num(c, r) && rat_is(val(c, r), p, q);
}

static bool num_negative(const ctx *c, ref r)
{
    return is_num(c, r) && val(c, r)->p < 0;
}

/* ---- the order (docs/canonical-form.md, "The order") ---- */

#define R_MUL 6                 /* a product's rank, which a monomial of several factors takes */

static int rank(const ctx *c, ref r)
{
    static const int8_t R[] = {[K_VAR] = 0, [K_PI] = 1, [K_ONAME] = 2, [K_FN] = 3, [K_OFN] = 4, [K_POW] = 5,
                               [K_MUL] = R_MUL, [K_ADD] = 7, [K_NUM] = 8, [K_LIT] = 9, [K_EQ] = 10};
    return r == SPARE ? 11 : R[nd(c, r)->kind];
}

static int cmp(const ctx *c, ref a, ref b);

static int cmp_lists(const ctx *c, const ref *x, int nx, const ref *y, int ny)
{
    for (int i = 0; i < nx && i < ny; i++) {
        int s = cmp(c, x[i], y[i]);
        if (s) return s;
    }
    return nx - ny;
}

/* A literal's text: the main text, or subst's replacement when its id is 1. */
static const char *lit_text(const ctx *c, const node *x)
{
    return (x->id ? c->s2 : c->s) + x->a;
}

static int cmp_(const ctx *c, ref a, ref b)
{
    const node *x = nd(c, a), *y = nd(c, b);
    int s;
    if (a == b) return 0;
    if ((s = rank(c, a) - rank(c, b)) != 0 || a == SPARE || b == SPARE) return s;
    switch (x->kind) {
    case K_VAR: return x->id - y->id;
    case K_PI: return 0;
    case K_NUM: return rat_cmp(val(c, a), val(c, b));
    case K_LIT: {
        int m = x->n < y->n ? x->n : y->n;
        if ((s = memcmp(lit_text(c, x), lit_text(c, y), (size_t)m)) != 0) return s;
        return x->n - y->n;
    }
    case K_FN:
        if (x->id != y->id) return x->id - y->id;
        return cmp(c, x->a, y->a);
    case K_ONAME: return x->id - y->id;
    case K_OFN:
        if (x->id != y->id) return x->id - y->id;
        if ((s = cmp(c, x->a, y->a)) != 0 || (!x->b && !y->b)) return s;
        if (!x->b || !y->b) return x->b ? 1 : -1;
        return cmp(c, x->b, y->b);
    case K_POW:
    case K_EQ:
        if ((s = cmp(c, x->a, y->a)) != 0) return s;
        return cmp(c, x->b, y->b);
    default: return cmp_lists(c, list(c, x->a), x->n, list(c, y->a), y->n);
    }
}

static int cmp(const ctx *c, ref a, ref b)
{
    int r;
    if (!enter((ctx *)c)) return 0;     /* the context is never a const object: only this walk's view */
    r = cmp_(c, a, b);
    ((ctx *)c)->depth--;
    return r;
}

/* A factor as base^exponent (X is X^1). */
static ref base_of(const ctx *c, ref f)
{
    return nd(c, f)->kind == K_POW ? nd(c, f)->a : f;
}

static ref exp_of(ctx *c, ref f)
{
    return nd(c, f)->kind == K_POW ? nd(c, f)->b : small(c, C_1);
}

static int by_factor(ctx *c, ref a, ref b)
{
    int s;
    /* numbers after every other factor, together: a power of a number (SQRT(1.5)) sorts by its base's
       value, and would otherwise fall between them (fold reads the numbers as one run; pair test) */
    if (is_num(c, a) != is_num(c, b)) return is_num(c, a) ? 1 : -1;
    s = cmp(c, base_of(c, a), base_of(c, b));
    return s ? s : cmp(c, exp_of(c, a), exp_of(c, b));
}

/* A product's operand list has a coefficient when its first operand is its only number. (Numbers that
   could not be folded together stay apart, after the other factors; each is then a factor.) */
static __attribute__((noinline)) bool has_coef(const ctx *c, const ref *k, int n)
{
    return is_num(c, k[0]) && (n == 1 || !is_num(c, k[1]));
}

/* A term's monomial: its factors without the coefficient (one factor: the term itself). */
static const ref *mono(const ctx *c, const ref *t, int *n)
{
    const node *x = nd(c, *t);
    if (x->kind == K_MUL) {
        const ref *k = list(c, x->a);
        int skip = has_coef(c, k, x->n);
        *n = x->n - skip;
        return k + skip;
    }
    *n = 1;
    return t;
}

static int by_mono(ctx *c, ref a, ref b)
{
    int na, nb, s;
    const ref *x = mono(c, &a, &na), *y = mono(c, &b, &nb);
    if (na == 1 && nb == 1) return cmp(c, x[0], y[0]);
    if (na == 1) return (s = rank(c, x[0]) - R_MUL) != 0 ? s : -1;
    if (nb == 1) return (s = R_MUL - rank(c, y[0])) != 0 ? s : 1;
    return cmp_lists(c, x, na, y, nb);
}

/* Insertion sort (stable): the lists are short. */
static void sort(ctx *c, ref *k, int n, int (*by)(ctx *, ref, ref))
{
    for (int i = 1; i < n; i++) {
        ref x = k[i];
        int j = i;
        while (j > 0 && by(c, k[j - 1], x) > 0) {
            k[j] = k[j - 1];
            j--;
        }
        k[j] = x;
    }
}

/* ---- impurity (CAS 003, #4264 point 2) ---- */

/* Whether r holds RAND or SEED: a draw, or a change to the draws. An impure subtree is never merged,
   cancelled, folded, dropped or duplicated, never distributed over or expanded, and impure operands keep
   their order (after the pure ones). Free when the input has neither. */
static bool impure(const ctx *c, ref r);

static bool impure_(const ctx *c, ref r)
{
    const node *x = nd(c, r);
    switch (x->kind) {
    case K_ONAME: return x->id == c->rand_id;
    case K_OFN: return x->id == c->seed_id || impure(c, x->a) || (x->b && impure(c, x->b));
    case K_FN: return impure(c, x->a);
    case K_POW:
    case K_EQ: return impure(c, x->a) || impure(c, x->b);
    case K_ADD:
    case K_MUL:
        for (int i = 0; i < x->n; i++)
            if (impure(c, list(c, x->a)[i])) return true;
        return false;
    default: return false;
    }
}

static bool impure(const ctx *c, ref r)
{
    bool v;
    if (!c->has_impure || r == 0 || r == SPARE) return false;
    if (!enter((ctx *)c)) return false;
    v = impure_(c, r);
    ((ctx *)c)->depth--;
    return v;
}

/* Moves t[0..m)'s impure operands, in order, to a new list (*l, *n); returns the pure ones' count. */
static int split_impure(ctx *c, ref *t, int m, ref *l, int *n)
{
    int j = 0;
    *n = 0;
    *l = 0;
    if (!c->has_impure) return m;
    for (int i = 0; i < m; i++)
        if (impure(c, t[i])) {
            if (!*l && !(*l = grab(c, (size_t)m * sizeof(ref)))) return m;
            list(c, *l)[(*n)++] = t[i];
        } else t[j++] = t[i];
    return j;
}

/* ---- the canonical constructors ---- */

static ref add(ctx *c, const ref *in, int n);
static ref mul(ctx *c, const ref *in, int n);
static ref power(ctx *c, ref b, ref e);

static ref add2(ctx *c, ref a, ref b)
{
    ref k[2] = {a, b};
    return add(c, k, 2);
}

static ref mul2(ctx *c, ref a, ref b)
{
    ref k[2] = {a, b};
    return mul(c, k, 2);
}

static ref mul3(ctx *c, ref a, ref b, ref d)
{
    ref k[3] = {a, b, d};
    return mul(c, k, 3);
}

static ref neg(ctx *c, ref a)
{
    return mul2(c, small(c, C_M1), a);
}

static ref flat_node(ctx *c, int kind, ref l, int n)
{
    ref r = mk(c, kind, l, 0);
    if (r != SPARE) nd(c, r)->n = (uint16_t)n;
    return r;
}

/* A sum or product node over t[0..k), which sits one slot into the list l: moved to the list's start (a
   list is referenced by its 8-byte unit) instead of copied to a new one. */
static ref in_place(ctx *c, int kind, ref l, ref *t, int k)
{
    memmove(list(c, l), t, (size_t)k * sizeof(ref));
    return flat_node(c, kind, l, k);
}

/* A product from a coefficient and factors already canonical, sorted and merged. */
static ref mul_node(ctx *c, ref co, const ref *f, int n)
{
    int one = is_k(c, co, 1, 1), m = n + !one;
    ref l;
    if (n == 0) return co;
    if (one && n == 1) return f[0];
    if (!(l = grab(c, (size_t)m * sizeof(ref)))) return SPARE;
    if (!one) list(c, l)[0] = co;
    memcpy(list(c, l) + !one, f, (size_t)n * sizeof(ref));
    return flat_node(c, K_MUL, l, m);
}

/* The operands of in[0..n), with sums (or products) opened, into a new list; returns its length. The list
   starts one slot in: list(c, *l)[0] is free, for a product's coefficient (see mul_place). */
static int flatten(ctx *c, const ref *in, int n, int kind, ref *l)
{
    int m = 0, k = 0;
    ref *t;
    for (int i = 0; i < n; i++) m += nd(c, in[i])->kind == kind ? nd(c, in[i])->n : 1;
    if (!(*l = grab(c, (size_t)(m + 2) * sizeof(ref)))) return -1;
    t = list(c, *l) + 1;
    for (int i = 0; i < n; i++) {
        const node *x = nd(c, in[i]);
        if (x->kind == kind) {
            memcpy(t + k, list(c, x->a), (size_t)x->n * sizeof(ref));
            k += x->n;
        } else t[k++] = in[i];
    }
    return m;
}

/* Sorts t[0..m) with by, then folds its numbers into *v with f, all of them or none. by puts the numbers
   together, in order of value (both orders rank a number after every kind but a literal), so this works
   in place. When any step would not fit, every number stays, in order, the identities (1 in a product, 0
   in a sum) dropped: the result is the same whatever the grouping that led to it, and simplifying it
   again changes nothing. Returns the new length. */
static int fold(ctx *c, ref *t, int m, rat *v, bool (*f)(rat *, const rat *, const rat *),
                int (*by)(ctx *, ref, ref))
{
    int first = 0, last, j;
    rat acc = *v;
    bool ok = true;
    sort(c, t, m, by);
    while (first < m && !is_num(c, t[first])) first++;
    last = first;
    while (last < m && is_num(c, t[last])) last++;
    for (int i = first; i < last && ok; i++) {
        rat r;
        if ((ok = f(&r, &acc, val(c, t[i])))) acc = r;
    }
    j = first;
    if (ok) *v = acc;
    else if (f == rat_add) {
        for (int i = first; i < last; i++)
            if (!is_k(c, t[i], 0, 1)) t[j++] = t[i];
    } else {
        /* a product's numbers apart: one sign rule, so X-4E18×4E18 and -4E18×4E18+X agree: their absolute
           values in order, the 1s dropped, the product's sign on the first (second review) */
        int neg_ = 0;
        for (int i = first; i < last; i++) {
            rat a = *val(c, t[i]);
            if (a.p < 0) neg_ ^= 1, a.p = -a.p;
            if (!rat_is(&a, 1, 1)) t[j++] = a.p == val(c, t[i])->p ? t[i] : num(c, &a);
        }
        sort(c, t + first, j - first, by);
        if (neg_ && j > first) {
            rat a = *val(c, t[first]);
            a.p = -a.p;
            t[first] = num(c, &a);
        }
    }
    for (int i = last; i < m; i++) t[j++] = t[i];         /* literals, after the numbers */
    return j;
}

/* A sum (docs: flattened, sorted by monomial, like terms collected, the constant last). */
static ref add(ctx *c, const ref *in, int n)
{
    ref l, *t, il;
    rat sum;
    int m, k = 0, ni;
    if (c->st != CASIM_OK) return SPARE;
    if ((m = flatten(c, in, n, K_ADD, &l)) < 0) return SPARE;
    t = list(c, l) + 1;
    m = split_impure(c, t, m, &il, &ni);
    rat_int(&sum, 0);
    m = fold(c, t, m, &sum, rat_add, by_mono);
    for (int i = 0; i < m;) {
        int j = i + 1;
        rat s2;
        bool ok = true, nums = false;
        const ref *f;
        int nf;
        if (!is_num(c, t[i]))
            while (j < m && !is_num(c, t[j]) && by_mono(c, t[i], t[j]) == 0) j++;
        if (j - i == 1) {
            t[k++] = t[i++];
            continue;
        }
        /* a run of like terms: their coefficients added, one term built (all of them, or the run is left
           as it is when the sum would not fit); one node for the run, not one for each pair */
        rat_int(&s2, 0);
        for (int q = i; q < j && ok; q++) {
            const node *x = nd(c, t[q]);
            rat cq;
            rat_int(&cq, 1);
            if (x->kind == K_MUL && has_coef(c, list(c, x->a), x->n)) cq = *val(c, list(c, x->a)[0]);
            ok = rat_add(&s2, &s2, &cq);
        }
        if (!ok) {
            while (i < j) t[k++] = t[i++];
            continue;
        }
        f = mono(c, &t[i], &nf);
        i = j;
        if (rat_is(&s2, 0, 1)) continue;
        for (int q = 0; q < nf; q++) nums |= is_num(c, f[q]);
        if (!nums) t[k++] = mul_node(c, num(c, &s2), f, nf);
        else {
            /* numbers kept apart in the monomial: the coefficient joins them (second review) */
            ref ml = grab(c, (size_t)(nf + 1) * sizeof(ref));
            if (!ml) return SPARE;
            list(c, ml)[0] = num(c, &s2);
            memcpy(list(c, ml) + 1, f, (size_t)nf * sizeof(ref));
            t[k++] = mul(c, list(c, ml), nf + 1);
        }
    }
    if (!rat_is(&sum, 0, 1)) t[k++] = num(c, &sum);
    for (int i = 0; i < ni; i++) t[k++] = list(c, il)[i];   /* impure terms, in order, last */
    if (k == 0) return small(c, C_0);
    if (k == 1) return t[0];
    list(c, l)[0] = 0;
    return in_place(c, K_ADD, l, t, k);
}

/* A sum's content: its first term's coefficient (the constant is last, so the first term has a
   monomial). */
static rat content(ctx *c, ref s)
{
    ref t0 = list(c, nd(c, s)->a)[0];
    rat k;
    rat_int(&k, 1);
    if (nd(c, t0)->kind == K_MUL && has_coef(c, list(c, nd(c, t0)->a), nd(c, t0)->n))
        k = *val(c, list(c, nd(c, t0)->a)[0]);
    return k;
}

/* The sum s with every term divided by k (k not 0); 0 when a quotient would not fit. The terms keep their
   order: their monomials do not change. */
static ref sum_div(ctx *c, ref s, const rat *k)
{
    int n = nd(c, s)->n;
    rat inv, q;
    ref l;
    if (!rat_make(&inv, k->q, k->p) || !(l = grab(c, (size_t)n * sizeof(ref)))) return 0;
    for (int i = 0; i < n; i++) {
        ref t = list(c, nd(c, s)->a)[i], r;
        if (is_num(c, t)) {
            if (!rat_mul(&q, val(c, t), &inv)) return 0;
            r = num(c, &q);
        } else {
            int nf;
            const ref *f = mono(c, &t, &nf);
            rat ct;
            rat_int(&ct, 1);
            if (nd(c, t)->kind == K_MUL && has_coef(c, list(c, nd(c, t)->a), nd(c, t)->n))
                ct = *val(c, list(c, nd(c, t)->a)[0]);
            if (!rat_mul(&q, &ct, &inv)) return 0;
            r = mul_node(c, num(c, &q), f, nf);
        }
        list(c, l)[i] = r;
    }
    return flat_node(c, K_ADD, l, n);
}

/* A product (docs: flattened, coefficient first, factors sorted by base, like factors combined; a sum
   that is a factor is made primitive, its first coefficient 1, the content moved to the coefficient; a
   number times a single sum distributed). */
static ref mul(ctx *c, const ref *in, int n)
{
    ref l, *t, co, reuse = 0, il = 0;
    rat cv;
    int m = 0, ni = 0;
    if (c->st != CASIM_OK) return SPARE;
    rat_int(&cv, 1);
    for (int i = 0; i < n; i++) {
        /* the coefficient's node, when an operand already holds that number (no new 40 bytes) */
        const node *x = nd(c, in[i]);
        if (is_num(c, in[i])) reuse = in[i];
        else if (x->kind == K_MUL && is_num(c, list(c, x->a)[0])) reuse = list(c, x->a)[0];
    }
    for (int pass = 0; pass < 64; pass++) {
        bool again = false;
        int k = 0;
        if ((m = flatten(c, in, n, K_MUL, &l)) < 0) return SPARE;
        t = list(c, l) + 1;
        if (pass == 0) m = split_impure(c, t, m, &il, &ni);     /* impure factors: apart, in order */
        if (!rat_is(&cv, 1, 1)) t[m++] = num(c, &cv), rat_int(&cv, 1);    /* the last pass's coefficient */
        for (int i = 0; i < m; i++)
            if (is_k(c, t[i], 0, 1))                                  /* 0×u is 0: but a draw stays */
                return ni ? mul_node(c, small(c, C_0), list(c, il), ni) : small(c, C_0);
        m = fold(c, t, m, &cv, rat_mul, by_factor);
        if (rat_is(&cv, 0, 1)) return ni ? mul_node(c, small(c, C_0), list(c, il), ni) : small(c, C_0);
        for (int i = 0; i < m; i++) {
            /* a sum as a factor: primitive, so 2×(X+1) and (2×X+2) are one product */
            rat k = nd(c, t[i])->kind == K_ADD ? content(c, t[i]) : cv, ck;
            ref p;
            if (nd(c, t[i])->kind != K_ADD || rat_is(&k, 1, 1) || !rat_mul(&ck, &cv, &k)) continue;
            if ((p = sum_div(c, t[i], &k)) != 0) t[i] = p, cv = ck;
        }
        sort(c, t, m, by_factor);
        for (int i = 0; i < m;) {
            int j = i + 1;
            if (!is_num(c, t[i]))
                while (j < m && !is_num(c, t[j]) && cmp(c, base_of(c, t[i]), base_of(c, t[j])) == 0) j++;
            if (j - i == 1) {
                t[k++] = t[i++];
                continue;
            }
            {
                /* a run of like factors: one base, all the exponents added at once */
                ref el = grab(c, (size_t)(j - i) * sizeof(ref)), f;
                if (!el) return SPARE;
                for (int q = i; q < j; q++) list(c, el)[q - i] = exp_of(c, t[q]);
                f = power(c, base_of(c, t[i]), add(c, list(c, el), j - i));
                i = j;
                again = true;       /* a merge can make a number, a product, a power of another base, a
                                       sum not yet primitive: each needs the steps again */
                if (is_k(c, f, 1, 1)) continue;
                t[k++] = f;
            }
        }
        m = k;
        if (!again) break;
        /* a merge: flatten, fold, make primitive, sort and merge once more (the list has room for the
           coefficient: a merge freed a place) */
        in = t;
        n = m;
    }
    co = rat_is(&cv, 1, 1) ? small(c, C_1) : reuse == 0 || rat_cmp(&cv, val(c, reuse)) != 0 ? num(c, &cv) : reuse;
    if (ni) {
        /* the impure factors, in order, after the pure ones */
        ref fl = grab(c, (size_t)(m + ni) * sizeof(ref));
        if (!fl) return SPARE;
        memcpy(list(c, fl), t, (size_t)m * sizeof(ref));
        memcpy(list(c, fl) + m, list(c, il), (size_t)ni * sizeof(ref));
        return mul_node(c, co, list(c, fl), m + ni);
    }
    if (m == 1 && !rat_is(&cv, 1, 1) && nd(c, t[0])->kind == K_ADD) {
        /* a number times one sum: distributed */
        const node *s = nd(c, t[0]);
        int ns = s->n;
        ref dl = grab(c, (size_t)ns * sizeof(ref));
        if (!dl) return SPARE;
        for (int i = 0; i < ns; i++) {
            ref d = mul2(c, co, list(c, nd(c, t[0])->a)[i]);
            list(c, dl)[i] = d;
        }
        return add(c, list(c, dl), ns);
    }
    if (m == 0 || (m == 1 && is_k(c, co, 1, 1))) return mul_node(c, co, t, m);
    if (is_k(c, co, 1, 1)) return in_place(c, K_MUL, l, t, m);
    list(c, l)[0] = co;                 /* the coefficient, in the slot before the factors */
    return flat_node(c, K_MUL, l, m + 1);
}

/* u^e (docs: e not 0 or 1; a number to a whole power folded; (u^a)^n and (u×v)^n for a whole n). */
static ref power(ctx *c, ref b, ref e)
{
    long k_ = 0;
    bool w;
    if (c->st != CASIM_OK) return SPARE;
    if (c->has_impure && impure(c, b)) return is_k(c, e, 1, 1) ? b : mk(c, K_POW, b, e);    /* as written */
    w = is_num(c, e) && rat_whole(val(c, e), &k_);
    if (is_k(c, e, 0, 1)) return small(c, C_1);
    if (is_k(c, e, 1, 1) || is_k(c, b, 1, 1)) return b;
    if (is_num(c, b)) {
        const rat *v = val(c, b);
        rat p;
        if (rat_is(v, 0, 1) && is_num(c, e) && val(c, e)->p > 0) return b;     /* 0^e, e > 0 */
        if (w && !(rat_is(v, 0, 1) && k_ < 0) && rat_pow(&p, v, k_)) return num(c, &p);
    }
    if (w && nd(c, b)->kind == K_POW) return power(c, nd(c, b)->a, mul2(c, nd(c, b)->b, e));
    if (w && nd(c, b)->kind == K_ADD) {
        /* (k×s)^n = k^n × s^n, s primitive */
        rat k = content(c, b), kn;
        ref p;
        if (!rat_is(&k, 1, 1) && rat_pow(&kn, &k, k_) && (p = sum_div(c, b, &k)) != 0)
            return mul2(c, num(c, &kn), power(c, p, e));
    }
    if (w && nd(c, b)->kind == K_MUL) {
        int nb = nd(c, b)->n;
        ref l = grab(c, (size_t)nb * sizeof(ref));
        if (!l) return SPARE;
        for (int i = 0; i < nb; i++) {
            ref f = power(c, list(c, nd(c, b)->a)[i], e);
            list(c, l)[i] = f;
        }
        return mul(c, list(c, l), nb);
    }
    return mk(c, K_POW, b, e);
}

static ref fn(ctx *c, int f, ref a)
{
    const node *x = nd(c, a);
    ref r;
    if (c->st != CASIM_OK) return SPARE;
    switch (f) {
    case F_SQ: return power(c, a, small(c, C_2));
    case F_INV: return power(c, a, small(c, C_M1));
    case F_SQRT: return power(c, a, small(c, C_HALF));
    case F_LN: if (x->kind == K_FN && x->id == F_EXP) return x->a; break;
    case F_EXP: if (x->kind == K_FN && x->id == F_LN) return x->a; break;
    case F_LOG: if (x->kind == K_FN && x->id == F_ALOG) return x->a; break;
    case F_ALOG: if (x->kind == K_FN && x->id == F_LOG) return x->a; break;
    case F_ABS:
        if (is_num(c, a)) {
            rat v = *val(c, a);
            v.p = v.p < 0 ? -v.p : v.p;
            return num(c, &v);
        }
        break;
    }
    r = mk(c, K_FN, a, 0);
    if (r != SPARE) nd(c, r)->id = (uint8_t)f;
    return r;
}

/* ---- the parser: core/eqn.c's grammar and its failure positions, without recursion ---- */

static void fail(ctx *c, int at)
{
    if (c->st == CASIM_OK) {
        int n = 0;
        for (int i = 0; i < at && c->s[i]; i++)
            if (((unsigned char)c->s[i] & 0xC0) != 0x80) n++;
        c->st = CASIM_SYNTAX;
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

/* The parser's operators: binary by level (+ - 1, × ÷ 2, ^ 4), the two unary minuses (before a power,
   level 3: -X^2 is -(X^2); after ^, level 5: X^-Y^2 is (X^-Y)^2), =, and the openers. */
enum { O_ADD, O_SUB, O_MUL, O_DIV, O_NEG, O_POW, O_PNEG, O_EQ, M_PAREN, M_FN, M_VEC };

static const uint8_t LEVEL[] = {1, 1, 2, 2, 3, 4, 5, 0};

typedef struct {
    uint8_t op, fn, args, need;
} opent;

typedef struct {
    ref *val;
    opent *ops;
    int nv, no;
} pstack;

static void reduce(ctx *c, pstack *p)
{
    static const uint8_t KIND[] = {K_BADD, K_BSUB, K_BMUL, K_BDIV, K_NEG, K_BPOW, K_NEG, K_REQ};
    opent o = p->ops[--p->no];
    if (o.op == O_NEG || o.op == O_PNEG) {
        if (p->nv >= 1) p->val[p->nv - 1] = mk(c, K_NEG, p->val[p->nv - 1], 0);
        return;
    }
    if (p->nv < 2) return;
    p->nv--;
    p->val[p->nv - 1] = mk(c, KIND[o.op], p->val[p->nv - 1], p->val[p->nv]);
}

/* Applies the operators that bind at least as tightly as level, down to the nearest opener. */
static void reduce_to(ctx *c, pstack *p, int level)
{
    while (p->no > 0 && p->ops[p->no - 1].op < M_PAREN && LEVEL[p->ops[p->no - 1].op] >= level) reduce(c, p);
}

static bool any_opener(const pstack *p)
{
    for (int i = 0; i < p->no; i++)
        if (p->ops[i].op >= M_PAREN) return true;
    return false;
}

/* An operand where one is expected: a minus, a number, an opener, a name, a variable. Returns the bytes
   consumed and sets *done when an operand is complete; -1 on a syntax error. */
static int operand(ctx *c, pstack *p, int i, bool parg, bool *done)
{
    const char *s = c->s, *t = s + i;
    int n, points, digits;
    *done = true;
    if (*t == '-') {
        p->ops[p->no++] = (opent){parg ? O_PNEG : O_NEG, 0, 0, 0};
        *done = false;
        return 1;
    }
    if ((n = based_len(t)) > 0) {
        refuse(c, t, n);
        p->val[p->nv++] = leaf(c, K_BAD, 0);
        return n;
    }
    if ((n = decimal_len(t, &points, &digits)) > 0) {
        int j = i + n;
        if (!digits || points > 2) {
            fail(c, j);
            return -1;
        }
        if (s[j] == 'i' || strncmp(s + j, "\xce\xb8", 2) == 0) {
            /* a complex number, x i y or r θ a: parsed as the core parses it, then refused */
            int n2;
            j += s[j] == 'i' ? 1 : 2;
            if (s[j] == '-') j++;
            n2 = decimal_len(s + j, &points, &digits);
            if (n2 == 0 || !digits || points > 1) {
                fail(c, j);
                return -1;
            }
            j += n2;
            refuse(c, t, j - i);
            p->val[p->nv++] = leaf(c, K_BAD, 0);
        } else {
            ref r = mk(c, K_LIT, (ref)i, 0);
            if (r != SPARE) {
                nd(c, r)->n = (uint16_t)n;
                nd(c, r)->id = c->s2 && c->s == c->s2;      /* subst's replacement: its own text */
            }
            p->val[p->nv++] = r;
        }
        return j - i;
    }
    if (*t == '(' || *t == '[') {
        if (*t == '[') refuse(c, t, 1);
        p->ops[p->no++] = (opent){*t == '(' ? M_PAREN : M_VEC, 0, 1, 3};
        *done = false;
        return 1;
    }
    {
        /* a name: the longest that fits; a function name needs its "(" */
        const name_t *best = NULL;
        size_t bl = 0;
        for (size_t k = 0; k < sizeof NAMES / sizeof NAMES[0]; k++) {
            size_t m;
            if (NAMES[k].name[0] != *t) continue;          /* most never share the first byte */
            m = strlen(NAMES[k].name);
            if (m > bl && strncmp(t, NAMES[k].name, m) == 0 && (NAMES[k].args == 0 || t[m] == '(')) {
                best = &NAMES[k];
                bl = m;
            }
        }
        if (best && best->args == 0) {
            int id = (int)(best - NAMES);
            if (strcmp(best->name, PI_UTF8) == 0) p->val[p->nv++] = leaf(c, K_PI, 0);
            else if (c->opaque) {
                /* an opaque name (CAS 003): held at one value for the evaluation, but RAND draws */
                p->val[p->nv++] = leaf(c, K_ONAME, id);
                if (id == c->rand_id) c->has_impure = true;
            } else {
                refuse(c, t, (int)bl);
                p->val[p->nv++] = leaf(c, K_BAD, 0);
            }
            return (int)bl;
        }
        if (best) {
            int fn_ = best->fn;
            if (fn_ == F_REFUSED && c->opaque) {
                fn_ = M_OPAQUE + (int)(best - NAMES);       /* an opaque function (CAS 003) */
                if (best - NAMES == c->seed_id) c->has_impure = true;
            } else if (fn_ == F_REFUSED) refuse(c, t, (int)bl);
            p->ops[p->no++] = (opent){M_FN, (uint8_t)fn_, 1, best->args};
            *done = false;
            return (int)bl + 1;     /* and its "(" */
        }
    }
    if (strncmp(t, IND_I, 3) == 0 || strncmp(t, IND_J, 3) == 0) {
        if (c->opaque) p->val[p->nv++] = leaf(c, K_ONAME, t[2] == IND_I[2] ? ID_IND_I : ID_IND_J);
        else {
            refuse(c, t, 3);
            p->val[p->nv++] = leaf(c, K_BAD, 0);
        }
        return 3;
    }
    if ((*t >= 'A' && *t <= 'Z') || *t == 'i') {
        /* A-Z, and i: the 33s's variable i (33s p.13-23, core unit 025); the 35s rejects a lone i before
           casim could see it */
        p->val[p->nv++] = leaf(c, K_VAR, *t == 'i' ? 26 : *t - 'A');
        return 1;
    }
    fail(c, i);
    return -1;
}

/* A number as written: a literal, minus signs before one, or a group of numbers. */
static bool numeric_leaf(const ctx *c, ref r)
{
    while (nd(c, r)->kind == K_NEG) r = nd(c, r)->a;
    return nd(c, r)->kind == K_LIT || nd(c, r)->kind == K_GROUP;
}

/* A chain of + - × ÷ whose operands are all numbers (walked down its left side in a loop). */
static bool numeric_chain(const ctx *c, ref r)
{
    int k = nd(c, r)->kind;
    if (k != K_BADD && k != K_BSUB && k != K_BMUL && k != K_BDIV) return false;
    while ((k = nd(c, r)->kind) == K_BADD || k == K_BSUB || k == K_BMUL || k == K_BDIV) {
        if (!numeric_leaf(c, nd(c, r)->b)) return false;
        r = nd(c, r)->a;
    }
    return numeric_leaf(c, r);
}

/* A closer: ")", "," or "]". Returns false on a syntax error. */
static bool closer(ctx *c, pstack *p, int *i)
{
    char t = c->s[*i];
    opent *o;
    reduce_to(c, p, 1);
    o = p->no > 0 ? &p->ops[p->no - 1] : NULL;
    if (o && t == ')' && o->op == M_PAREN) {
        /* (p÷q)×… is the one number p/q times …, not p×(1/q)×…: the parentheses stay around a chain of
           numbers. (Only numbers: a group of factors canonicalized apart cannot merge with a power of the
           same product, (SQRT(2×Z)×SQRT(2×Z))÷SQRT(2×Z) against the same unparenthesized; second review.) */
        ref v = p->nv > 0 ? p->val[p->nv - 1] : 0;
        if (v && numeric_chain(c, v)) p->val[p->nv - 1] = mk(c, K_GROUP, v, 0);
        p->no--;
        (*i)++;
        return true;
    }
    if (o && t == ')' && o->op == M_FN && o->args == o->need) {
        p->no--;
        (*i)++;
        if (o->fn >= M_OPAQUE) {
            /* an opaque function: one argument or two */
            ref a = p->val[p->nv - o->need], b = o->need == 2 ? p->val[p->nv - 1] : 0, r;
            p->nv -= o->need - 1;
            r = mk(c, K_ROFN, a, b);
            if (r != SPARE) nd(c, r)->id = (uint8_t)(o->fn - M_OPAQUE);
            p->val[p->nv - 1] = r;
            return true;
        }
        if (o->need == 2) p->nv--;
        if (o->need == 2 || o->fn == F_REFUSED) p->val[p->nv - 1] = leaf(c, K_BAD, 0);
        else {
            ref r = mk(c, K_RFN, p->val[p->nv - 1], 0);
            if (r != SPARE) nd(c, r)->id = o->fn;
            p->val[p->nv - 1] = r;
        }
        return true;
    }
    if (o && t == ',' && o->op == M_FN && o->args < o->need) {
        o->args++;
        (*i)++;
        return true;
    }
    if (o && t == ',' && o->op == M_VEC) {
        (*i)++;
        if (o->args == 3) {
            fail(c, *i);
            return false;
        }
        o->args++;
        return true;
    }
    if (o && t == ']' && o->op == M_VEC) {
        p->nv -= o->args - 1;
        p->val[p->nv - 1] = leaf(c, K_BAD, 0);
        p->no--;
        (*i)++;
        return true;
    }
    fail(c, *i);
    return false;
}

static ref parse(ctx *c)
{
    const char *s = c->s;
    size_t len = strlen(s), need = ((len + 2) * (sizeof(ref) + sizeof(opent)) + 7u) & ~(size_t)7u;
    pstack p = {0};
    int i = 0;
    bool want = true, parg = false, eq = false;
    ref out = SPARE;
    if (len > 65535u || c->top < c->used + need) {     /* a literal's offset and length are 16 bits */
        c->st = CASIM_TOO_LONG;
        return SPARE;
    }
    c->top -= need;                     /* the stacks, at the top of the arena */
    p.val = (ref *)(void *)(c->mem + c->top);
    p.ops = (opent *)(void *)(c->mem + c->top + (len + 2) * sizeof(ref));
    while (c->st == CASIM_OK) {
        const char *t = s + i;
        int op = -1;
        if (want) {
            bool done;
            int n = operand(c, &p, i, parg, &done);
            if (n < 0) break;
            i += n;
            if (done) want = false;
            else if (s[i - 1] != '-') parg = false;
            continue;
        }
        if (*t == '!') {
            if (c->opaque && p.nv > 0) {
                ref r = mk(c, K_ROFN, p.val[p.nv - 1], 0);    /* postfix !: an opaque function */
                if (r != SPARE) nd(c, r)->id = ID_FACT;
                p.val[p.nv - 1] = r;
            } else refuse(c, t, 1);
            i++;
            continue;
        }
        if (*t == '^') op = O_POW;
        else if (strncmp(t, TIMES, 2) == 0) op = O_MUL;
        else if (strncmp(t, DIVIDE, 2) == 0) op = O_DIV;
        else if (*t == '+') op = O_ADD;
        else if (*t == '-') op = O_SUB;
        else if (*t == '=' && !eq && !any_opener(&p) && !c->no_eq) op = O_EQ, eq = true;
        if (op >= 0) {
            reduce_to(c, &p, op == O_EQ ? 1 : LEVEL[op]);
            p.ops[p.no++] = (opent){(uint8_t)op, 0, 0, 0};
            i += op == O_MUL || op == O_DIV ? 2 : 1;
            want = true;
            parg = op == O_POW;
            continue;
        }
        if (*t == ')' || *t == ',' || *t == ']') {
            if (!closer(c, &p, &i)) break;
            if (*t != ')' && *t != ']') want = true, parg = false;
            continue;
        }
        if (*t == '\0' && !any_opener(&p)) {
            while (p.no > 0) reduce(c, &p);
            if (c->st == CASIM_OK && p.nv == 1) out = p.val[0];
            break;
        }
        fail(c, i);
    }
    c->top += need;
    if (c->st == CASIM_OK && c->bad) c->st = CASIM_NOT_DIFFERENTIABLE;
    return out;
}

/* ---- raw to canonical ---- */

/* The rational of the value the core reads for a literal: a decimal, or a fraction a.b.c as (a×c+b)/c;
   false (the literal is kept as typed) when it does not fit. Kept out of line: its buffer would otherwise
   sit in the recursion. */
static __attribute__((noinline)) bool literal(rat *v, const char *s, int n)
{
    char buf[200];
    int points = 0, k = 0;
    for (int i = 0; i < n; i++) points += s[i] == '.';
    if (n + 2 >= (int)sizeof buf) return false;
    if (points < 2) {
        abn x;
        if (s[0] == '.') buf[k++] = '0';
        memcpy(buf + k, s, (size_t)n);
        buf[k + n] = '\0';
        return abn_from_text(&x, buf) == ABN_OK && rat_from_abn(v, &x);
    }
    {
        /* a.b.c: three digit strings, no exponent, a nonzero denominator (else the core's evaluating pass
           rejects it, and casim keeps the text) */
        int64_t part[3], ac, top;
        int at = 0;
        for (int j = 0; j < 3; j++) {
            int64_t m = 0;
            int digits = 0;
            while (at < n && s[at] != '.') {
                if (!digit(s[at]) || ++digits > 18) return false;
                m = m * 10 + (s[at++] - '0');
            }
            at++;
            part[j] = m;
        }
        return part[2] != 0 && mul64(&ac, part[0], part[2]) && add64(&top, ac, part[1]) && rat_make(v, top, part[2]);
    }
}

static ref canon(ctx *c, ref r);

/* A chain of + and - (or × and ÷), walked down its left side in a loop. */
static ref chain(ctx *c, ref r, bool sum)
{
    int m = 1, k1 = sum ? K_BADD : K_BMUL, k2 = sum ? K_BSUB : K_BDIV;
    ref l, x = r;
    while (nd(c, x)->kind == k1 || nd(c, x)->kind == k2) m++, x = nd(c, x)->a;
    if (!(l = grab(c, (size_t)m * sizeof(ref)))) return SPARE;
    x = r;
    for (int k = m - 1; k > 0; k--) {
        ref v = canon(c, nd(c, x)->b);
        if (nd(c, x)->kind == k2) v = sum ? neg(c, v) : power(c, v, small(c, C_M1));
        list(c, l)[k] = v;
        x = nd(c, x)->a;
    }
    {
        ref v = canon(c, x);
        list(c, l)[0] = v;
    }
    return sum ? add(c, list(c, l), m) : mul(c, list(c, l), m);
}

/* The rarer cases, out of line: their locals stay out of every recursive frame (S3 stack, CAS 003). */
static __attribute__((noinline)) ref canon_ofn(ctx *c, ref r)
{
    ref a = canon(c, nd(c, r)->a), b = nd(c, r)->b ? canon(c, nd(c, r)->b) : 0, o = mk(c, K_OFN, a, b);
    if (o != SPARE) nd(c, o)->id = nd(c, r)->id;
    return o;
}

static __attribute__((noinline)) ref canon_var(ctx *c, ref r)
{
    if (c->sub_val && nd(c, r)->id == c->sub_var) {
        c->sub_count++;
        return c->sub_val;                      /* subst: the replacement, whole */
    }
    return r;
}

static ref canon_(ctx *c, ref r)
{
    const node *x = nd(c, r);
    if (c->st != CASIM_OK) return SPARE;
    switch (x->kind) {
    case K_LIT: {
        rat v;
        if (literal(&v, lit_text(c, x), x->n)) return num(c, &v);
        return r;               /* kept as typed */
    }
    case K_NEG: {
        int k = 0;
        ref y = r;
        while (nd(c, y)->kind == K_NEG) k++, y = nd(c, y)->a;
        y = canon(c, y);
        return k % 2 ? neg(c, y) : y;
    }
    case K_BADD:
    case K_BSUB: return chain(c, r, true);
    case K_BMUL:
    case K_BDIV: return chain(c, r, false);
    case K_BPOW: {
        /* ^ is left to right: walk the left side in a loop, then apply the exponents innermost first */
        int m = 0;
        ref y = r, b, l;
        while (nd(c, y)->kind == K_BPOW) m++, y = nd(c, y)->a;
        if (!(l = grab(c, (size_t)m * sizeof(ref)))) return SPARE;
        y = r;
        for (int k = m - 1; k >= 0; k--) list(c, l)[k] = nd(c, y)->b, y = nd(c, y)->a;
        b = canon(c, y);
        for (int k = 0; k < m; k++) b = power(c, b, canon(c, list(c, l)[k]));
        return b;
    }
    case K_GROUP: return canon(c, x->a);
    case K_ROFN: return canon_ofn(c, r);
    case K_VAR: return canon_var(c, r);
    case K_RFN: return fn(c, x->id, canon(c, x->a));
    case K_REQ: {
        ref a = canon(c, x->a), b = canon(c, nd(c, r)->b);
        return mk(c, K_EQ, a, b);
    }
    default: return r;          /* a variable or π */
    }
}

static ref canon(ctx *c, ref r)
{
    ref v;
    if (!enter(c)) return SPARE;
    v = canon_(c, r);
    c->depth--;
    return v;
}

/* ---- the derivative, on the canonical tree ---- */

/* The angle mode's factor: the trigonometric functions read degrees or grads, so d/du SIN(u) carries
   π/180 (π/200); the inverses return them, so they carry 180/π. */
static ref angle_factor(ctx *c, bool inverse)
{
    ref d, pi;
    if (c->angle == CASIM_RAD) return small(c, C_1);
    d = small(c, c->angle == CASIM_DEG ? C_180 : C_200);
    if (!c->pi || c->pi == SPARE) c->pi = leaf(c, K_PI, 0);
    pi = c->pi;
    return inverse ? mul2(c, d, power(c, pi, small(c, C_M1))) : mul2(c, pi, power(c, d, small(c, C_M1)));
}

/* (1 - u^2), (1 + u^2), (u^2 + 1), (u^2 - 1): the inverse functions' radicands. */
static ref one_pm_sq(ctx *c, ref u, int sign_one, int sign_sq)
{
    ref sq = power(c, u, small(c, C_2));
    return add2(c, small(c, sign_one > 0 ? C_1 : C_M1), sign_sq > 0 ? sq : neg(c, sq));
}

/* d/dx f(u) = f'(u)·u' for the function node r. */
static ref deriv_fn(ctx *c, ref r, ref du)
{
    ref u = nd(c, r)->a, m1 = small(c, C_M1);
    switch (nd(c, r)->id) {
    case F_LN: return mul2(c, du, power(c, u, m1));
    case F_LOG: return mul3(c, du, power(c, u, m1), power(c, fn(c, F_LN, small(c, C_10)), m1));
    case F_EXP: return mul2(c, r, du);
    case F_ALOG: return mul3(c, r, fn(c, F_LN, small(c, C_10)), du);
    case F_SIN: return mul3(c, fn(c, F_COS, u), du, angle_factor(c, false));
    case F_COS: return neg(c, mul3(c, fn(c, F_SIN, u), du, angle_factor(c, false)));
    case F_TAN: return mul3(c, du, power(c, fn(c, F_COS, u), small(c, C_M2)), angle_factor(c, false));
    case F_ASIN: return mul3(c, du, power(c, one_pm_sq(c, u, 1, -1), small(c, C_MHALF)), angle_factor(c, true));
    case F_ACOS:
        return neg(c, mul3(c, du, power(c, one_pm_sq(c, u, 1, -1), small(c, C_MHALF)), angle_factor(c, true)));
    case F_ATAN: return mul3(c, du, power(c, one_pm_sq(c, u, 1, 1), m1), angle_factor(c, true));
    case F_SINH: return mul2(c, fn(c, F_COSH, u), du);
    case F_COSH: return mul2(c, fn(c, F_SINH, u), du);
    case F_TANH: return mul2(c, du, power(c, fn(c, F_COSH, u), small(c, C_M2)));
    case F_ASINH: return mul2(c, du, power(c, one_pm_sq(c, u, 1, 1), small(c, C_MHALF)));
    case F_ACOSH: return mul2(c, du, power(c, one_pm_sq(c, u, -1, 1), small(c, C_MHALF)));
    case F_ATANH: return mul2(c, du, power(c, one_pm_sq(c, u, 1, -1), m1));
    case F_ABS: return mul2(c, fn(c, F_SGN, u), du);      /* right except at u = 0 */
    }
    return small(c, C_0);       /* not reached: the parser refuses every other function */
}

static ref deriv_(ctx *c, ref r)
{
    const node *x = nd(c, r);
    if (c->st != CASIM_OK) return SPARE;
    switch (x->kind) {
    case K_VAR: return small(c, x->id == c->var ? C_1 : C_0);
    case K_ADD: {
        int n = x->n;
        ref l = grab(c, (size_t)n * sizeof(ref));
        if (!l) return SPARE;
        for (int i = 0; i < n; i++) {
            ref d = deriv(c, list(c, nd(c, r)->a)[i]);
            list(c, l)[i] = d;
        }
        return add(c, list(c, l), n);
    }
    case K_MUL: {
        /* (f1·f2·…)' = Σ f1·…·fi'·…: the product with one factor replaced by its derivative, each */
        int n = x->n, k = 0;
        ref terms = grab(c, (size_t)n * sizeof(ref)), f = grab(c, (size_t)n * sizeof(ref));
        if (!terms || !f) return SPARE;
        for (int i = 0; i < n; i++) {
            ref d = deriv(c, list(c, nd(c, r)->a)[i]);
            if (is_k(c, d, 0, 1)) continue;
            memcpy(list(c, f), list(c, nd(c, r)->a), (size_t)n * sizeof(ref));
            list(c, f)[i] = d;
            d = mul(c, list(c, f), n);
            list(c, terms)[k++] = d;
        }
        return add(c, list(c, terms), k);
    }
    case K_POW: {
        ref u = x->a, e = x->b, du = deriv(c, u), de = deriv(c, e);
        bool cu = is_k(c, du, 0, 1), ce = is_k(c, de, 0, 1);
        if (cu && ce) return small(c, C_0);
        if (ce) return mul3(c, e, power(c, u, add2(c, e, small(c, C_M1))), du);     /* e·u^(e-1)·u' */
        if (is_k(c, u, 0, 1)) return small(c, C_0);                            /* 0^v: 0 where defined */
        if (cu) return mul3(c, r, fn(c, F_LN, u), de);                          /* u^e·ln u·e' */
        return mul2(c, r, add2(c, mul2(c, de, fn(c, F_LN, u)), mul3(c, e, du, power(c, u, small(c, C_M1)))));
    }
    case K_FN: {
        ref du = deriv(c, x->a);
        if (is_k(c, du, 0, 1)) return du;
        return deriv_fn(c, r, du);
    }
    case K_EQ: {
        ref a = deriv(c, x->a), b = deriv(c, nd(c, r)->b);
        return mk(c, K_EQ, a, b);
    }
    default: return small(c, C_0);      /* numbers, π, literals */
    }
}

static ref deriv(ctx *c, ref r)
{
    ref v;
    if (!enter(c)) return SPARE;
    v = deriv_(c, r);
    c->depth--;
    return v;
}

/* ---- the printer ---- */

typedef struct {
    char *o;
    size_t cap, n;
    bool full;
} out_t;

static void put(out_t *w, const char *s, size_t len)
{
    if (!w || w->full) return;
    if (w->n + len + 1 > w->cap) {
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

/* A number in the forms core/eqn.c reads: plain where it is short, else d.dddEn. Kept out of line, so its
   buffers are not in every frame of the recursive printer. */
static __attribute__((noinline)) void put_num(out_t *w, const abn *v)
{
    bool neg_;
    char d[ABN_DIGITS + 2], buf[ABN_DIGITS + 48];
    int lead, nd_, k = 0;
    if (!w) return;
    abn_digits(v, &neg_, d, &lead);
    nd_ = (int)strlen(d);
    if (nd_ == 0) {
        puts_(w, "0");
        return;
    }
    if (neg_) buf[k++] = '-';
    if (lead >= 0 && lead < ABN_DIGITS) {
        for (int i = 0; i <= lead; i++) buf[k++] = i < nd_ ? d[i] : '0';
        if (nd_ > lead + 1) {
            buf[k++] = '.';
            for (int i = lead + 1; i < nd_; i++) buf[k++] = d[i];
        }
    } else if (lead < 0 && lead >= -6) {
        buf[k++] = '0';
        buf[k++] = '.';
        for (int i = -1; i > lead; i--) buf[k++] = '0';
        for (int i = 0; i < nd_; i++) buf[k++] = d[i];
    } else {
        int e = lead < 0 ? -lead : lead, p = 0;
        char ex[8];
        buf[k++] = d[0];
        if (nd_ > 1) {
            buf[k++] = '.';
            for (int i = 1; i < nd_; i++) buf[k++] = d[i];
        }
        buf[k++] = 'E';
        if (lead < 0) buf[k++] = '-';
        do ex[p++] = (char)('0' + e % 10); while ((e /= 10) && p < 8);
        while (p) buf[k++] = ex[--p];
    }
    put(w, buf, (size_t)k);
}

/* v's digits, a minus sign first when negative; returns the length. */
static int i64_text(char *buf, int64_t v)
{
    char d[20];
    int n = 0, k = 0;
    uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;
    if (v < 0) buf[k++] = '-';
    do d[n++] = (char)('0' + u % 10u); while ((u /= 10u) && n < 20);
    while (n) buf[k++] = d[--n];
    buf[k] = '\0';
    return k;
}

static void put_int(out_t *w, int64_t v)
{
    char buf[24];
    put(w, buf, (size_t)i64_text(buf, v));
}

/* p/q as a decimal, when it terminates (q has no prime but 2 and 5) and its digits fit. */
static bool decimal_of(const rat *v, abn *d)
{
    char buf[48];
    int64_t q = v->q, m;
    int twos = 0, fives = 0, k, n;
    while (q % 2 == 0) q /= 2, twos++;
    while (q % 5 == 0) q /= 5, fives++;
    if (q != 1) return false;
    k = twos > fives ? twos : fives;
    if (k > 18 || !mul64(&m, v->p, POW10[k] / v->q)) return false;
    n = i64_text(buf, m);
    if (k) {
        buf[n++] = 'E';
        buf[n++] = '-';
        n += i64_text(buf + n, k);
    }
    buf[n] = '\0';
    return abn_from_text(d, buf) == ABN_OK;
}

/* |v| as a decimal, or p÷q; returns the level it prints at (5, or 2 for a quotient). */
static __attribute__((noinline)) int put_rat_abs(out_t *w, const rat *v)
{
    abn d;
    rat a = {v->p < 0 ? -v->p : v->p, v->q};
    if (decimal_of(&a, &d)) {
        put_num(w, &d);
        return 5;
    }
    put_int(w, a.p);
    puts_(w, DIVIDE);
    put_int(w, a.q);
    return 2;
}

static __attribute__((noinline)) void put_signed(const ctx *c, out_t *w, ref t, bool flip);

/* The recursive printer's frames hold only references: every number (a coefficient, an exponent) is read
   in an out-of-line leaf below, which returns before the printer goes deeper. On the S3 a 192-byte power
   chain took 25 KB of stack with the numbers in the frames (measured, CAS 002). */

static int level(ctx *c, ref r);
static void print(ctx *c, out_t *w, ref r);

static void print_in(ctx *c, out_t *w, ref r, bool paren)
{
    if (paren) puts_(w, "(");
    print(c, w, r);
    if (paren) puts_(w, ")");
}

/* The number x is p/q in absolute value. */
static bool abs_is(const ctx *c, ref x, int p, int q)
{
    const rat *v = val(c, x);
    return (v->p == p || v->p == -p) && v->q == q;
}

/* A factor goes under the ÷ when its exponent is a number below 0, unless its base is 0: 0^(-2) is
   undefined and stays as it is (as 1÷0^2 it would read back as 1÷0). */
static bool below(const ctx *c, ref f)
{
    return nd(c, f)->kind == K_POW && num_negative(c, nd(c, f)->b) && !is_k(c, nd(c, f)->a, 0, 1);
}

/* A coefficient's parts: whether it prints in the numerator (a decimal other than 1, or the p of a p÷q
   other than 1) and whether it has a denominator (the q of a p÷q). */
static __attribute__((noinline)) void coef_parts(const ctx *c, ref f0, bool *top, bool *bottom)
{
    rat co;
    abn d;
    *top = *bottom = false;
    if (!f0) return;
    co = *val(c, f0);
    co.p = co.p < 0 ? -co.p : co.p;
    if (decimal_of(&co, &d)) *top = !rat_is(&co, 1, 1);
    else *top = co.p != 1, *bottom = co.q != 1;
}

/* The coefficient's sign and numerator (top) or its ÷ denominator; returns whether it printed an item. */
static __attribute__((noinline)) int put_coef(const ctx *c, out_t *w, ref f0, bool flip, bool top)
{
    rat co;
    abn d;
    bool neg_;
    rat_int(&co, 1);
    if (f0) co = *val(c, f0);
    neg_ = (co.p < 0) != flip;
    co.p = co.p < 0 ? -co.p : co.p;
    if (top && neg_) puts_(w, "-");
    if (decimal_of(&co, &d)) {
        if (!top || rat_is(&co, 1, 1)) return 0;
        put_num(w, &d);
        return 1;
    }
    if (top) {
        if (co.p == 1) return 0;
        put_int(w, co.p);
        return 1;
    }
    if (co.q != 1) {
        puts_(w, DIVIDE);
        put_int(w, co.q);
    }
    return 0;
}

/* A number exponent, its absolute value: a decimal, or (p÷q). */
static __attribute__((noinline)) void put_exp(const ctx *c, out_t *w, ref x)
{
    rat e = *val(c, x);
    abn d;
    e.p = e.p < 0 ? -e.p : e.p;
    if (decimal_of(&e, &d)) put_num(w, &d);
    else {
        puts_(w, "(");
        put_rat_abs(w, &e);
        puts_(w, ")");
    }
}

/* A power with a symbolic exponent, and the left chain under it, ((b^x)^y)^z, as b^x^y^z, in a loop: the
   chain's depth does not reach the stack. */
static void put_chain(ctx *c, out_t *w, ref f)
{
    int k = 0;
    ref b = f;
    while (nd(c, b)->kind == K_POW && !is_num(c, nd(c, b)->b)) k++, b = nd(c, b)->a;
    print_in(c, w, b, level(c, b) < 4);
    for (int i = k - 1; i >= 0; i--) {
        ref y = f, x;
        for (int j = 0; j < i; j++) y = nd(c, y)->a;
        x = nd(c, y)->b;
        puts_(w, "^");
        print_in(c, w, x, level(c, x) < 5);
    }
}

/* One factor, its exponent's sign dropped (a denominator's): the base for |e| = 1 (in () below level min),
   SQRT(base) for 1/2, base^|e| otherwise. */
static void put_factor(ctx *c, out_t *w, ref f, int min)
{
    ref b, x;
    if (is_num(c, f)) {
        /* a number kept apart (it would not fold): whole, so it reads back as the one number */
        bool paren = num_negative(c, f) || put_rat_abs(NULL, val(c, f)) < 5;
        if (paren) puts_(w, "(");
        put_signed(c, w, f, false);
        if (paren) puts_(w, ")");
        return;
    }
    if (nd(c, f)->kind != K_POW) {
        print_in(c, w, f, level(c, f) < min);
        return;
    }
    b = nd(c, f)->a;
    x = nd(c, f)->b;
    if (!is_num(c, x)) put_chain(c, w, f);
    else if (num_negative(c, x) && !below(c, f)) {
        /* 0^(-e), in a numerator, as written: 1÷0, SQRT(0) or 0 would each read back as 0 */
        print_in(c, w, b, level(c, b) < 4);
        puts_(w, "^(-");
        put_exp(c, w, x);
        puts_(w, ")");
    } else if (abs_is(c, x, 1, 1)) print_in(c, w, b, level(c, b) < min);
    else if (abs_is(c, x, 1, 2)) {
        puts_(w, "SQRT(");
        print(c, w, b);
        puts_(w, ")");
    } else {
        print_in(c, w, b, level(c, b) < 4);
        puts_(w, "^");
        put_exp(c, w, x);
    }
}

/* The level a factor prints at in a numerator (min 2), from its own kind and its base's: no deeper. */
static int factor_level(ctx *c, ref f)
{
    ref x;
    int l;
    if (is_num(c, f)) return 5;
    if (nd(c, f)->kind != K_POW) return (l = level(c, f)) < 2 ? 5 : l;
    x = nd(c, f)->b;
    if (!is_num(c, x)) return 4;
    return abs_is(c, x, 1, 2) ? 5 : 4;
}

/* A product with draws: its factors in their order, × or ÷ as each comes, so the draws happen in the
   order they were written (1÷RAND×RAND is not RAND÷RAND; review). Out of line: off print's frame. */
static __attribute__((noinline)) void print_drawn(ctx *c, out_t *w, const ref *f, int n, int start, bool flip)
{
    int items = put_coef(c, w, start ? f[0] : 0, flip, true);
    put_coef(c, w, start ? f[0] : 0, flip, false);
    for (int i = start; i < n; i++) {
        bool d = below(c, f[i]);
        if (d && !items) puts_(w, "1");
        if (items || d) puts_(w, d ? DIVIDE : TIMES);
        put_factor(c, w, f[i], d ? 3 : 2);
        items++;
    }
    if (!items) puts_(w, "1");
}

/* A product (or one power): its sign, the numerator's items joined by ×, then ÷ each denominator item
   (X÷(Y+1): ÷'s right side above level 2); flip prints the negated product. */
static void print_product(ctx *c, out_t *w, ref r, bool flip)
{
    const node *x = nd(c, r);
    const ref *f = x->kind == K_MUL ? list(c, x->a) : &r;
    int n = x->kind == K_MUL ? x->n : 1, start = has_coef(c, f, n), items;
    if (c->has_impure && impure(c, r)) {
        print_drawn(c, w, f, n, start, flip);
        return;
    }
    items = put_coef(c, w, start ? f[0] : 0, flip, true);
    for (int i = start; i < n; i++) {
        if (below(c, f[i])) continue;
        if (items++) puts_(w, TIMES);
        put_factor(c, w, f[i], 2);
    }
    if (!items) puts_(w, "1");
    put_coef(c, w, start ? f[0] : 0, flip, false);
    for (int i = start; i < n; i++) {
        if (!below(c, f[i])) continue;
        puts_(w, DIVIDE);
        put_factor(c, w, f[i], 3);
    }
}

/* The level a product prints at, counted, not printed. */
static int product_level(ctx *c, ref r)
{
    const node *x = nd(c, r);
    const ref *f = x->kind == K_MUL ? list(c, x->a) : &r;
    int n = x->kind == K_MUL ? x->n : 1, start = has_coef(c, f, n), items, dens, one = 5;
    bool top, bottom, sign = start && num_negative(c, f[0]);
    coef_parts(c, start ? f[0] : 0, &top, &bottom);
    items = top, dens = bottom;
    for (int i = start; i < n; i++) {
        if (below(c, f[i])) dens++;
        else items++, one = factor_level(c, f[i]);
    }
    if (dens || items > 1) return 2;
    if (items == 0) one = 5;
    return sign && one > 3 ? 3 : one;
}

static bool negative(ctx *c, ref t)
{
    if (is_num(c, t)) return num_negative(c, t);
    return nd(c, t)->kind == K_MUL && has_coef(c, list(c, nd(c, t)->a), nd(c, t)->n) &&
           num_negative(c, list(c, nd(c, t)->a)[0]);
}

static __attribute__((noinline)) void put_signed(const ctx *c, out_t *w, ref t, bool flip)
{
    if (num_negative(c, t) != flip) puts_(w, "-");
    put_rat_abs(w, val(c, t));
}

static void print_term(ctx *c, out_t *w, ref t, bool flip)
{
    if (is_num(c, t)) put_signed(c, w, t, flip);
    else if (nd(c, t)->kind == K_MUL || nd(c, t)->kind == K_POW) print_product(c, w, t, flip);
    else print(c, w, t);
}

static int level(ctx *c, ref r)
{
    if (r == SPARE) return 5;
    switch (nd(c, r)->kind) {
    case K_ADD: return 1;
    case K_MUL:
    case K_POW: return product_level(c, r);
    case K_NUM: {
        int l = put_rat_abs(NULL, val(c, r));
        return num_negative(c, r) && l > 3 ? 3 : l;
    }
    case K_EQ: return 0;
    default: return 5;
    }
}

/* An opaque name or function, out of line (its locals stay out of print_'s recursive frame). */
static __attribute__((noinline)) void print_opaque(ctx *c, out_t *w, ref r)
{
    const node *x = nd(c, r);
    if (x->kind == K_ONAME) {
        puts_(w, x->id == ID_IND_I ? IND_I : x->id == ID_IND_J ? IND_J : NAMES[x->id].name);
        return;
    }
    if (x->id == ID_FACT) {
        print_in(c, w, x->a, level(c, x->a) < 5);
        puts_(w, "!");
        return;
    }
    puts_(w, NAMES[x->id].name);
    puts_(w, "(");
    print(c, w, x->a);
    if (nd(c, r)->b) {
        puts_(w, ",");
        print(c, w, nd(c, r)->b);
    }
    puts_(w, ")");
}

static void print_(ctx *c, out_t *w, ref r)
{
    const node *x = nd(c, r);
    if (w->full || r == SPARE) return;
    switch (x->kind) {
    case K_NUM: put_signed(c, w, r, false); return;
    case K_LIT: put(w, lit_text(c, x), x->n); return;
    case K_VAR:
        if (x->id == 26) puts_(w, "i");
        else {
            char ch = (char)('A' + x->id);
            put(w, &ch, 1);
        }
        return;
    case K_PI: puts_(w, PI_UTF8); return;
    case K_ONAME:
    case K_OFN: print_opaque(c, w, r); return;
    case K_FN:
        puts_(w, FNAME[x->id]);
        print_in(c, w, x->a, true);
        return;
    case K_EQ:
        print(c, w, x->a);
        puts_(w, "=");
        print(c, w, nd(c, r)->b);
        return;
    case K_ADD:
        for (int i = 0; i < x->n; i++) {
            ref t = list(c, nd(c, r)->a)[i];
            bool m = i > 0 && negative(c, t);
            if (i > 0) puts_(w, m ? "-" : "+");
            print_term(c, w, t, m);
        }
        return;
    case K_MUL:
    case K_POW: print_product(c, w, r, false); return;
    }
}

static void print(ctx *c, out_t *w, ref r)
{
    if (!enter(c)) return;              /* stopped: c->st is TOO_LONG, which finish() reports */
    print_(c, w, r);
    c->depth--;
}

/* ---- the API ---- */

static casim_status finish(ctx *c, ref n, char *out, size_t cap)
{
    out_t w = {out, cap, 0, false};
    if (cap == 0) return CASIM_TOO_LONG;
    out[0] = '\0';
    if (c->st == CASIM_OK && (n == SPARE || n == 0)) c->st = CASIM_TOO_LONG;    /* never an empty OK */
    if (c->st == CASIM_OK) {
        print(c, &w, n);                /* the depth guard may stop it: c->st is then TOO_LONG */
        if (w.full) c->st = CASIM_TOO_LONG;
    }
    if (c->st == CASIM_SYNTAX) {
        char d[12];
        int k = 0, e = c->errpos;
        do d[k++] = (char)('0' + e % 10); while ((e /= 10) && k < 11);
        while (k) put(&w, &d[--k], 1);
    } else if (c->st == CASIM_NOT_DIFFERENTIABLE) put(&w, c->bad, (size_t)c->badlen);
    if (w.full || c->st == CASIM_TOO_LONG) out[0] = '\0';
    return c->st;
}

static void start(ctx *c, const char *eqn, int angle, void *arena, size_t arena_cap)
{
    uintptr_t p = (uintptr_t)arena, q = (p + 7u) & ~(uintptr_t)7u;
    size_t cap = arena_cap > q - p ? arena_cap - (q - p) : 0;
    memset(c, 0, sizeof *c);
    c->s = eqn;
    c->angle = angle;
    c->mem = (unsigned char *)arena + (q - p);
    if (cap > 65535u * 8u) cap = 65535u * 8u;
    c->cap = c->top = cap & ~(size_t)7u;
    /* unit 0 is "none"; unit 1 is the spare leaf */
    if (c->cap < 16) {
        c->st = CASIM_TOO_LONG;
        return;
    }
    memset(c->mem, 0, 16);
    nd(c, SPARE)->kind = K_PI;
    c->used = 16;
    c->rand_id = c->seed_id = -1;
    for (int i = 0; i < (int)(sizeof NAMES / sizeof NAMES[0]); i++) {
        if (strcmp(NAMES[i].name, "RAND") == 0) c->rand_id = i;
        if (strcmp(NAMES[i].name, "SEED") == 0) c->seed_id = i;
    }
}

casim_status casim_derive(const char *eqn, int var, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap)
{
    ctx c;
    ref n = SPARE;
    if (cap) out[0] = '\0';
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.var = var;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = canon(&c, n);
    if (c.st == CASIM_OK) n = deriv(&c, n);
    return finish(&c, n, out, cap);
}

casim_status casim_simplify(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    ctx c;
    ref n = SPARE;
    if (cap) out[0] = '\0';
    if (angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.opaque = true;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = canon(&c, n);
    return finish(&c, n, out, cap);
}

/* ---- expand and collect (unit CAS 003) ---- */

#define EXPAND_TERMS 64     /* more distinct terms than this cannot print in 191 bytes: stop before building */

static bool contains(const ctx *c, ref r, int var);

static bool contains_(const ctx *c, ref r, int var)
{
    const node *x = nd(c, r);
    switch (x->kind) {
    case K_VAR: return x->id == var;
    case K_FN: return contains(c, x->a, var);
    case K_OFN: return contains(c, x->a, var) || (x->b && contains(c, x->b, var));
    case K_POW:
    case K_EQ: return contains(c, x->a, var) || contains(c, x->b, var);
    case K_ADD:
    case K_MUL:
        for (int i = 0; i < x->n; i++)
            if (contains(c, list(c, x->a)[i], var)) return true;
        return false;
    default: return false;
    }
}

/* Whether var occurs in r (a guarded walk). */
static bool contains(const ctx *c, ref r, int var)
{
    bool v;
    if (r == 0 || r == SPARE) return false;
    if (!enter((ctx *)c)) return false;
    v = contains_(c, r, var);
    ((ctx *)c)->depth--;
    return v;
}

/* Whether expand opens r (a sum): every sum, or with collect's variable, a sum that holds it. */
static bool opens(ctx *c, ref r)
{
    return nd(c, r)->kind == K_ADD && !impure(c, r) && (c->sub_var < 0 || contains(c, r, c->sub_var));
}

/* A sum's terms as expand multiplies them: each term, or with collect's variable, the terms that hold it
   and one block of the rest (so (X+Y+1)^2 is X^2+2×X×(Y+1)+(Y+1)^2, the coefficients not expanded). */
static int blocks(ctx *c, ref s, ref *out)
{
    const node *x = nd(c, s);
    int n = 0, nr = 0;
    ref rest = 0;
    if (c->sub_var < 0) {                   /* expand: every term (no list for the rest) */
        memcpy(out, list(c, x->a), (size_t)x->n * sizeof(ref));
        return x->n;
    }
    if (!(rest = grab(c, (size_t)x->n * sizeof(ref)))) return 0;
    for (int i = 0; i < x->n; i++) {
        ref t = list(c, nd(c, s)->a)[i];
        if (c->sub_var < 0 || contains(c, t, c->sub_var)) out[n++] = t;
        else list(c, rest)[nr++] = t;
    }
    if (nr) out[n++] = add(c, list(c, rest), nr);
    return n;
}

static bool too_many(ctx *c, long terms)
{
    if (terms <= EXPAND_TERMS) return false;
    if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;
    return true;
}

/* C(n, k) exactly, or -1 past 2^62. */
static int64_t binom(int n, int k)
{
    int64_t r = 1;
    if (k < 0 || k > n) return 0;
    if (k > n - k) k = n - k;
    for (int i = 0; i < k; i++) {
        int64_t t, g;
        if (!mul64(&t, r, n - i)) {
            /* divide first where it divides: r × (n-i) / (i+1) */
            g = gcd64(r, i + 1);
            if (!mul64(&t, r / g, (n - i) / ((i + 1) / g)) || (n - i) % ((i + 1) / g)) return -1;
            r = t;
            continue;
        }
        r = t / (i + 1);
    }
    return r;
}

/* (t1+…+tm)^n by the multinomial theorem: a term for each k1+…+km = n, its coefficient n!/(k1!…km!)
   (a product of binomials), its factors t_i^k_i. The compositions are counted first and walked in a loop. */
static __attribute__((noinline)) ref multinomial(ctx *c, ref s, int n)
{
    ref tl = grab(c, EXPAND_TERMS * sizeof(ref)), kl = grab(c, EXPAND_TERMS * sizeof(int)), terms, *t;
    int m, *k, nt = 0;
    int64_t count;
    if (!tl || !kl || too_many(c, nd(c, s)->n)) return SPARE;
    t = list(c, tl);                    /* in the arena: off the stack (review: 608 bytes of frame) */
    k = (int *)(void *)list(c, kl);
    m = blocks(c, s, t);
    count = binom(n + m - 1, m - 1);
    if (m == 0 || count < 0 || too_many(c, count)) {
        if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;      /* a count past 2^62 too (review) */
        return SPARE;
    }
    if (!(terms = grab(c, (size_t)count * sizeof(ref)))) return SPARE;
    for (int i = 0; i < m; i++) k[i] = 0;
    k[0] = n;
    for (;;) {
        /* the term for k[0..m) */
        ref f = grab(c, (size_t)(m + 1) * sizeof(ref));
        int left = n, nf = 0;
        rat co;
        rat_int(&co, 1);
        if (!f) return SPARE;
        for (int i = 0; i < m; i++) {
            int64_t b = binom(left, k[i]);
            rat br;
            if (b < 0 || !rat_make(&br, b, 1) || !rat_mul(&co, &co, &br)) {
                if (c->st == CASIM_OK) c->st = CASIM_TOO_LONG;
                return SPARE;
            }
            left -= k[i];
            if (k[i]) {
                rat e;
                rat_int(&e, k[i]);
                list(c, f)[nf++] = power(c, t[i], num(c, &e));
            }
        }
        list(c, f)[nf++] = num(c, &co);
        list(c, terms)[nt++] = mul(c, list(c, f), nf);
        /* the next composition: move one from the last nonzero place but the final one onward */
        {
            int j = m - 2;
            while (j >= 0 && k[j] == 0) j--;
            if (j < 0) break;
            k[j]--;
            k[j + 1] = k[m - 1] + 1;
            if (j + 1 != m - 1) k[m - 1] = 0;
        }
        if (c->st != CASIM_OK || nt >= count) break;
    }
    return add(c, list(c, terms), nt);
}

static ref expand(ctx *c, ref r);

/* A product's expansion: its factors expanded, then every opened sum multiplied out (none when a factor is
   impure: no distribution over a draw, none of a draw over a sum). */
static __attribute__((noinline)) ref expand_mul(ctx *c, ref r)
{
    int n = nd(c, r)->n, nterms = 1, nk = 0;
    ref fl = grab(c, (size_t)n * sizeof(ref)), keep, acc;
    bool pure = true;
    if (!fl) return SPARE;
    for (int i = 0; i < n; i++) {
        ref e = expand(c, list(c, nd(c, r)->a)[i]);
        list(c, fl)[i] = e;
        if (impure(c, e)) pure = false;
    }
    if (!pure) return mul(c, list(c, fl), n);
    {
        /* the expanded factors, products opened (an expanded power can be a coefficient times a power) */
        ref ff;
        int m = flatten(c, list(c, fl), n, K_MUL, &ff);
        if (m < 0) return SPARE;
        fl = ff;
        memmove(list(c, fl), list(c, fl) + 1, (size_t)m * sizeof(ref));
        n = m;
    }
    if (c->sub_var < 0) {
        /* the denominator: every factor with a negative whole exponent, joined and expanded as one (a÷(b+c)
           stays a quotient, its divisor expanded: X÷((Y+1)×(Y-1)) is X÷(Y^2-1)) */
        int nd_ = 0, sums = 0;
        ref dl = 0;
        for (int i = 0; i < n; i++) {
            ref f = list(c, fl)[i];
            if (below(c, f)) {
                if (!dl && !(dl = grab(c, (size_t)n * sizeof(ref)))) return SPARE;
                rat e = *val(c, nd(c, f)->b);
                e.p = -e.p;
                sums += nd(c, nd(c, f)->a)->kind == K_ADD;
                list(c, dl)[nd_++] = power(c, nd(c, f)->a, num(c, &e));
            }
        }
        if (sums && !(nd_ == 1 && n == 1)) {
            ref den = power(c, expand(c, mul(c, list(c, dl), nd_)), small(c, C_M1));
            int k = 0;
            for (int i = 0; i < n; i++)
                if (!below(c, list(c, fl)[i])) list(c, fl)[k++] = list(c, fl)[i];
            list(c, fl)[k++] = den;
            n = k;
        }
    }
    /* the factors that stay, and the sums opened */
    if (!(keep = grab(c, (size_t)n * sizeof(ref)))) return SPARE;
    for (int i = 0; i < n; i++)
        if (!opens(c, list(c, fl)[i])) list(c, keep)[nk++] = list(c, fl)[i];
    acc = mul(c, list(c, keep), nk);
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref sum = list(c, fl)[i], bl, *b, nl;
        int nb, na, k = 0;
        const ref *at;
        ref one = acc;
        if (!opens(c, sum)) continue;
        if (too_many(c, nd(c, sum)->n) || !(bl = grab(c, EXPAND_TERMS * sizeof(ref)))) return SPARE;
        b = list(c, bl);
        nb = blocks(c, sum, b);
        na = nd(c, acc)->kind == K_ADD ? nd(c, acc)->n : 1;
        nterms = na * nb;
        if (too_many(c, nterms)) return SPARE;
        if (!(nl = grab(c, (size_t)nterms * sizeof(ref)))) return SPARE;
        at = nd(c, acc)->kind == K_ADD ? list(c, nd(c, acc)->a) : &one;
        for (int p = 0; p < na; p++)
            for (int q = 0; q < nb; q++) {
                ref t = mul2(c, at[p], b[q]);
                list(c, nl)[k++] = t;
            }
        acc = add(c, list(c, nl), k);
    }
    return acc;
}

static ref expand_(ctx *c, ref r)
{
    const node *x = nd(c, r);
    switch (x->kind) {
    case K_ADD: {
        int n = x->n;
        ref l = grab(c, (size_t)n * sizeof(ref));
        if (!l) return SPARE;
        for (int i = 0; i < n; i++) {
            ref e = expand(c, list(c, nd(c, r)->a)[i]);
            list(c, l)[i] = e;
        }
        return add(c, list(c, l), n);
    }
    case K_MUL: return expand_mul(c, r);
    case K_POW: {
        ref b = expand(c, x->a), e = nd(c, r)->b, p;
        long k = 0;
        if (is_num(c, e) && rat_whole(val(c, e), &k) && k >= 2 && opens(c, b)) return multinomial(c, b, (int)k);
        if (k <= -1 && c->sub_var < 0 && is_num(c, e) && rat_whole(val(c, e), &k) && opens(c, b)) {
            /* a sum to a negative whole power: the inverse of its positive power, expanded */
            ref pos = k == -1 ? b : multinomial(c, b, (int)-k);
            return power(c, pos, small(c, C_M1));
        }
        p = power(c, b, e);
        /* a whole power of a product distributes: its factors may be sums to open */
        if (nd(c, p)->kind == K_MUL && p != r) return expand(c, p);
        return p;
    }
    case K_FN:
        if (c->sub_var >= 0) return r;          /* collect: a function's argument is the remainder's */
        return fn(c, x->id, expand(c, x->a));
    case K_OFN: {
        ref a, b, o;
        if (c->sub_var >= 0) return r;
        a = expand(c, x->a);
        b = nd(c, r)->b ? expand(c, nd(c, r)->b) : 0;
        o = mk(c, K_OFN, a, b);
        if (o != SPARE) nd(c, o)->id = nd(c, r)->id;
        return o;
    }
    case K_EQ: {
        ref a = expand(c, x->a), b = expand(c, nd(c, r)->b);
        return mk(c, K_EQ, a, b);
    }
    default: return r;
    }
}

/* ---- reclaiming a step's garbage ---- */

/* Copies the part of v allocated since mark to the arena's end, its references already pointing where the
   block will be once it is slid down to mark (by `shift` units); nodes older than mark stay where they are.
   A copied node is left as K_FWD pointing at its copy, so a shared node is copied once. */
static ref copy_up(ctx *c, ref v, ref mark, ref shift)
{
    node *x;
    ref r;
    if (v == 0 || v == SPARE || v < mark || c->st != CASIM_OK) return v;
    x = nd(c, v);
    if (x->kind == K_FWD) return x->a;
    if (!enter(c)) return SPARE;
    if ((r = grab(c, sizeof(node))) == 0) {
        c->depth--;
        return SPARE;
    }
    *nd(c, r) = *nd(c, v);
    switch (nd(c, v)->kind) {
    case K_NUM: {
        ref q = grab(c, sizeof(rat));
        if (!q) break;
        *rt(c, q) = *rt(c, nd(c, v)->a);
        nd(c, r)->a = nd(c, v)->a < mark ? nd(c, v)->a : (ref)(q - shift);
        break;
    }
    case K_FN:
    case K_OFN:
    case K_POW:
    case K_EQ: {
        ref a = nd(c, v)->a, b = nd(c, v)->b, na, nb;
        if (nd(c, v)->kind != K_FN) b = nd(c, v)->b;
        nd(c, v)->kind = K_FWD;             /* forward before the children: a cycle cannot arise, but a
                                               shared child below must not see a half-copied parent */
        nd(c, v)->a = (ref)(r - shift);
        na = copy_up(c, a, mark, shift);
        nb = b ? copy_up(c, b, mark, shift) : 0;
        nd(c, r)->a = na;
        nd(c, r)->b = nb;
        c->depth--;
        return (ref)(r - shift);
    }
    case K_ADD:
    case K_MUL: {
        int n = nd(c, v)->n;
        ref l = nd(c, v)->a, nl = l < mark ? l : grab(c, (size_t)n * sizeof(ref));
        if (!nl) break;
        nd(c, v)->kind = K_FWD;
        nd(c, v)->a = (ref)(r - shift);
        if (l >= mark) {
            for (int i = 0; i < n; i++) {
                ref k = copy_up(c, list(c, l)[i], mark, shift);
                list(c, nl)[i] = k;
            }
            nd(c, r)->a = (ref)(nl - shift);
        }
        c->depth--;
        return (ref)(r - shift);
    }
    default: break;                         /* leaves: VAR, PI, ONAME, LIT (a is a text offset) */
    }
    nd(c, v)->kind = K_FWD;
    nd(c, v)->a = (ref)(r - shift);
    c->depth--;
    return (ref)(r - shift);
}

/* Keeps v and frees everything else allocated since mark (bytes): v's new part is copied up, the caches
   are moved or dropped, and the copy is slid down to mark. Returns v where it now is. */
static ref compact(ctx *c, size_t mark, ref v)
{
    size_t start = c->used, size;
    ref m = (ref)(mark / 8u), shift, nv;
    if (c->st != CASIM_OK || mark >= start || v == 0 || v == SPARE) return v;
    shift = (ref)((start - mark) / 8u);
    nv = copy_up(c, v, m, shift);
    if (c->st != CASIM_OK) return SPARE;
    /* the caches: a moved constant keeps its place, a dropped one is made again when needed */
    for (int k = 0; k < SMALL; k++)
        if (c->small[k] >= m) c->small[k] = nd(c, c->small[k])->kind == K_FWD ? nd(c, c->small[k])->a : 0;
    if (c->pi >= m) c->pi = nd(c, c->pi)->kind == K_FWD ? nd(c, c->pi)->a : 0;
    size = c->used - start;
    memmove(c->mem + mark, c->mem + start, size);
    c->used = mark + size;
    return nv;
}

#define COMPACT_AT 512          /* bytes of garbage a step must leave before it is worth reclaiming */

/* r expanded (a guarded walk); with c->sub_var set (collect), only where that variable is. Each step's
   garbage is reclaimed: an expansion's intermediate products and sums would otherwise fill the arena. */
static ref expand(ctx *c, ref r)
{
    ref v;
    size_t mark = c->used;
    if (c->st != CASIM_OK) return SPARE;
    if (!enter(c)) return SPARE;
    v = expand_(c, r);
    if (c->depth > 1 && c->used - mark > COMPACT_AT) v = compact(c, mark, v);    /* not the last step: no gain */
    c->depth--;
    return v;
}

/* expand until nothing changes: a product made while distributing can merge into a sum or a new
   denominator (SQRT(X+1)×SQRT(X+1) is X+1), which the next round opens (review: 7-19 in 5,000 were not
   fixed points). Each round's garbage is reclaimed; a few rounds always settle. */
static ref expand_fully(ctx *c, ref n)
{
    size_t mark = c->used;
    for (int round = 0; round < 8 && c->st == CASIM_OK; round++) {
        ref m = expand(c, n);
        if (c->st != CASIM_OK) return SPARE;
        if (cmp(c, m, n) == 0) return m;
        n = compact(c, mark, m);
    }
    return n;
}

/* A term as c×var^k: its k and its coefficient; false when var is in it other than as a whole power. */
static bool as_power(ctx *c, ref t, int var, long *k, ref *co)
{
    const node *x = nd(c, t);
    if (!contains(c, t, var)) {
        *k = 0, *co = t;
        return true;
    }
    if (x->kind == K_VAR) {
        *k = 1, *co = small(c, C_1);
        return true;
    }
    if (x->kind == K_POW && nd(c, x->a)->kind == K_VAR && is_num(c, x->b) && rat_whole(val(c, x->b), k)) {
        *co = small(c, C_1);
        return true;
    }
    if (x->kind == K_MUL) {
        int n = x->n, at = -1, nr = 0;
        ref rest;
        for (int i = 0; i < n; i++)
            if (contains(c, list(c, x->a)[i], var)) {
                if (at >= 0) return false;
                at = i;
            }
        if (!as_power(c, list(c, x->a)[at], var, k, co) || !is_k(c, *co, 1, 1)) return false;
        if (!(rest = grab(c, (size_t)n * sizeof(ref)))) return false;
        for (int i = 0; i < n; i++)
            if (i != at) list(c, rest)[nr++] = list(c, nd(c, t)->a)[i];
        *co = mul(c, list(c, rest), nr);
        return true;
    }
    return false;
}

typedef struct {
    long k;
    ref c;
} cterm;

/* var^k (k not 0): X, X^2, X^(-2). */
static void put_power(out_t *w, int var, long k)
{
    char ch = (char)('A' + var), buf[24];
    put(w, &ch, 1);
    if (k == 1) return;
    puts_(w, k < 0 ? "^(-" : "^");
    put(w, buf, (size_t)i64_text(buf, k < 0 ? -k : k));
    if (k < 0) puts_(w, ")");
}

/* One c×var^k term of the collected form (#4264 point 5): first is whether it leads the output. */
static void put_cterm(ctx *c, out_t *w, const cterm *t, int var, bool first)
{
    bool sum = nd(c, t->c)->kind == K_ADD, neg_ = !sum && negative(c, t->c);
    if (!first || neg_) puts_(w, neg_ ? "-" : "+");      /* no leading + */
    if (sum) {
        puts_(w, "(");
        print(c, w, t->c);
        puts_(w, ")");
    } else if (t->k != 0 && (is_k(c, t->c, 1, 1) || is_k(c, t->c, -1, 1))) {
        put_power(w, var, t->k);
        return;
    } else print_term(c, w, t->c, neg_);
    if (t->k != 0) {
        puts_(w, TIMES);
        put_power(w, var, t->k);
    }
}

/* The collected form of one side: Σ c_k × var^k in descending k, then the remainder after the lowest k. */
static void put_collected(ctx *c, out_t *w, ref r, int var)
{
    const node *x = nd(c, r);
    int n = x->kind == K_ADD ? x->n : 1, nc = 0, nr = 0;
    ref one = r, cl = grab(c, (size_t)n * sizeof(cterm) + 8), rl = grab(c, (size_t)n * sizeof(ref));
    cterm *ct;
    const ref *terms = x->kind == K_ADD ? list(c, x->a) : &one;
    if (!cl || !rl || c->st != CASIM_OK) return;
    ct = (cterm *)(void *)list(c, cl);
    for (int i = 0; i < n; i++) {
        long k;
        ref co;
        if (!impure(c, terms[i]) && as_power(c, terms[i], var, &k, &co)) {     /* a draw is never grouped */
            int j = 0;
            while (j < nc && ct[j].k != k) j++;
            if (j == nc) ct[nc++] = (cterm){k, co};
            else ct[j].c = add2(c, ct[j].c, co);
        } else list(c, rl)[nr++] = terms[i];
    }
    /* descending k (insertion sort: few terms) */
    for (int i = 1; i < nc; i++) {
        cterm v = ct[i];
        int j = i;
        while (j > 0 && ct[j - 1].k < v.k) ct[j] = ct[j - 1], j--;
        ct[j] = v;
    }
    {
        bool first = true;
        for (int i = 0; i < nc && c->st == CASIM_OK; i++) {
            if (is_k(c, ct[i].c, 0, 1)) continue;
            put_cterm(c, w, &ct[i], var, first);
            first = false;
        }
        if (nr) {
            ref rem = add(c, list(c, rl), nr);
            const node *y = nd(c, rem);
            if (!first && !(y->kind == K_ADD ? negative(c, list(c, y->a)[0]) : negative(c, rem))) puts_(w, "+");
            print(c, w, rem);
            first = false;
        }
        if (first) puts_(w, "0");
    }
}

/* ---- the API: CAS 003 ---- */

#define SLOT 192                /* an equation slot with its NUL: expand, collect and subst fit it or stop */

casim_status casim_expand(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    ctx c;
    ref n = SPARE;
    if (cap) out[0] = '\0';
    if (angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.opaque = true;
    c.sub_var = -1;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = compact(&c, 16, canon(&c, n));     /* the raw tree is garbage now */
    if (c.st == CASIM_OK) n = expand_fully(&c, n);
    return finish(&c, n, out, cap < SLOT ? cap : SLOT);
}

casim_status casim_collect(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    ctx c;
    ref n = SPARE;
    out_t w = {out, cap < SLOT ? cap : SLOT, 0, false};
    if (cap) out[0] = '\0';
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.opaque = true;
    c.sub_var = var;                    /* expand only where var is */
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = compact(&c, 16, canon(&c, n));     /* the raw tree is garbage now */
    if (c.st == CASIM_OK) n = expand_fully(&c, n);
    if (c.st != CASIM_OK || cap == 0) return finish(&c, n, out, cap < SLOT ? cap : SLOT);
    if (nd(&c, n)->kind == K_EQ) {
        put_collected(&c, &w, nd(&c, n)->a, var);
        puts_(&w, "=");
        put_collected(&c, &w, nd(&c, n)->b, var);
    } else put_collected(&c, &w, n, var);
    if (w.full && c.st == CASIM_OK) c.st = CASIM_TOO_LONG;
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, cap < SLOT ? cap : SLOT);
    return CASIM_OK;
}

casim_status casim_subst(const char *eqn, int var, const char *repl, int angle, char *out, size_t cap, void *arena,
                         size_t arena_cap)
{
    ctx c;
    ref n = SPARE, rr;
    int chars = 0;
    if (cap) out[0] = '\0';
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD || !repl) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.opaque = true;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st != CASIM_OK) return finish(&c, n, out, cap < SLOT ? cap : SLOT);
    /* the replacement: a second text, its literals marked (id 1), its SYNTAX offset counted as if the call
       were eqn + "|" + repl (#4264 point 6) */
    for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    c.s2 = repl;
    c.s = repl;
    c.no_eq = true;                     /* a replacement is an expression, not an equation (review) */
    rr = parse(&c);
    c.s = eqn;
    if (c.st == CASIM_SYNTAX) c.errpos += chars + 1;
    if (c.st != CASIM_OK) return finish(&c, rr, out, cap < SLOT ? cap : SLOT);
    rr = canon(&c, rr);
    c.sub_var = var;
    c.sub_val = rr;
    n = canon(&c, n);
    if (c.st == CASIM_OK && c.sub_count > 1 && impure(&c, rr)) {
        /* a draw copied: refused (#4264 point 2) */
        c.st = CASIM_NOT_DIFFERENTIABLE;
        c.bad = strstr(repl, "SEED(") ? "SEED" : "RAND";
        c.badlen = 4;
    }
    return finish(&c, n, out, cap < SLOT ? cap : SLOT);
}
