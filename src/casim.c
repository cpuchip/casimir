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
    bool rtl;                   /* the text is read as STU's (CAS 004, unit 031): ^ right to left, and a
                                   minus starting an exponent covers the chain after it */
    bool has_impure;            /* RAND or SEED occurs: impure subtrees exist (CAS 003, #4264 point 2) */
    bool starved;               /* the arena ran out (grab): integrate's walks keep NO_ROOM for it (review) */
    bool apart;                 /* something was left apart past 2^62 (numbers, or a run of like terms):
                                   until then no sum can merge once divided, and no term holds numbers */
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
   CASIM_DEPTH_MAX nested entries the call returns NO_ROOM (TOO_LONG before CAS 004). So the stack is bounded by construction, not
   by the inputs measured: the limit was set on the S3 so that the deepest case of every kind of nesting
   stays within 12 KB (unit CAS 002b; abacus's ruling, #4236). The sum, product and power constructors
   are not counted: they recurse only one level on their own, and their depth is the walks' (counting them
   too made a nest of sums cost two units a level, against one for a nest of powers, at the same stack). */
#ifndef CASIM_DEPTH_MAX
#define CASIM_DEPTH_MAX 46              /* 47 until CAS 010's build left 240 B of the 12,288 (abacus #5634) */
#endif

static bool enter(ctx *c)
{
    if (c->depth >= CASIM_DEPTH_MAX) {
        if (c->st == CASIM_OK) c->st = CASIM_NO_ROOM;          /* the stack's bound (CAS 004: not TOO_LONG) */
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
    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_NO_ROOM, c->starved = true;   /* the arena */
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
    int64_t g, a, b, p, q;
    if (x->q == 1 && y->q == 1) {
        /* whole numbers: no division (a 64-bit division is a library call on the S3) */
        if (!add64(&p, x->p, y->p) || !fits(p)) return false;
        r->p = p, r->q = 1;
        return true;
    }
    g = gcd64(x->q, y->q);
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
    if (x->q == y->q) return x->p < y->p ? -1 : 1;     /* one denominator: by numerator, no division */
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

/* A term's value in a sum: a number's own, else its coefficient (1 when it has none). */
static rat term_coef(ctx *c, ref t)
{
    const node *x = nd(c, t);
    rat k;
    if (is_num(c, t)) return *val(c, t);
    rat_int(&k, 1);
    if (x->kind == K_MUL && has_coef(c, list(c, x->a), x->n)) k = *val(c, list(c, x->a)[0]);
    return k;
}

static int by_coef(ctx *c, ref a, ref b)
{
    rat x = term_coef(c, a), y = term_coef(c, b);
    return rat_cmp(&x, &y);
}

/* t[0..n), in order of value, added into *acc, one denominator at a time (the smallest first): within
   each, from the low end while its sum is above 0 and from the high end otherwise. So no partial sum of a
   denominator's terms is further from 0 than its largest term or its total, and terms that cancel exactly
   meet before anything else does (2E18÷7−2E18÷7+3E18). The order comes from the values alone: whether it
   fits does not depend on how the sum was written (3E18×X+3E18×X−3E18×X; third review, and its delta). */
static bool add_by_den(ctx *c, const ref *t, int n, rat *acc)
{
    int64_t q = 0;
    for (;;) {
        int64_t g = 0;
        int lo = 0, hi = n - 1;
        rat s;
        for (int i = 0; i < n; i++) {
            int64_t d = term_coef(c, t[i]).q;
            if (d > q && (g == 0 || d < g)) g = d;
        }
        if (g == 0) return true;
        q = g;
        rat_int(&s, 0);
        while (lo <= hi) {
            rat v;
            if (term_coef(c, t[lo]).q != q) lo++;
            else if (term_coef(c, t[hi]).q != q) hi--;
            else if (v = term_coef(c, t[s.p > 0 ? lo++ : hi--]), !rat_add(&s, &s, &v)) return false;
        }
        if (!rat_add(acc, acc, &s)) return false;
    }
}

/* t[0..n), in order of value, added into *acc: by denominator (add_by_den); when the denominators' totals
   do not fit together, all the terms at once, from the low end while the sum is above 0 and from the high
   end otherwise (2E18+1E18-4500000000000000001÷2: the third pass). Both orders come from the values alone,
   so whether a sum folds never depends on how it was written. */
static bool add_sorted(ctx *c, const ref *t, int n, rat *acc)
{
    rat a0 = *acc;
    int lo = 0, hi = n - 1;
    if (add_by_den(c, t, n, acc)) return true;
    *acc = a0;
    while (lo <= hi) {
        rat v = term_coef(c, t[acc->p > 0 ? lo++ : hi--]);
        if (!rat_add(acc, acc, &v)) return false;
    }
    return true;
}

/* Sorts t[0..m) with by, then folds its numbers into *v with f, all of them or none. by puts the numbers
   together, in order of value (both orders rank a number after every kind but a literal), so this works
   in place. When any step would not fit, every number stays, in order, the identities (1 in a product, 0
   in a sum) dropped: the result is deterministic, and simplifying it again changes nothing (two
   groupings of the same mathematics may still differ past 2^62: docs). Returns the new length. */
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
    if (f == rat_add) ok = add_sorted(c, t + first, last - first, &acc);
    else
        for (int i = first; i < last && ok; i++) {
            rat r;
            if ((ok = f(&r, &acc, val(c, t[i])))) acc = r;
        }
    j = first;
    if (!ok) c->apart = true;
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

/* A coefficient k (not 0) times a monomial's factors f: one product node, or through mul() when numbers
   were kept apart in the monomial, so the coefficient joins them (second review). */
static ref term_of(ctx *c, const rat *k, const ref *f, int nf)
{
    ref ml;
    bool nums = false;
    for (int q = 0; q < nf; q++) nums |= is_num(c, f[q]);
    if (!nums) return mul_node(c, num(c, k), f, nf);
    if (!(ml = grab(c, (size_t)(nf + 1) * sizeof(ref)))) return SPARE;
    list(c, ml)[0] = num(c, k);
    memcpy(list(c, ml) + 1, f, (size_t)nf * sizeof(ref));
    return mul(c, list(c, ml), nf + 1);
}

/* A sum (docs: flattened, sorted by monomial, like terms collected, the constant last). */
static ref add(ctx *c, const ref *in, int n)
{
    ref l, *t, il;
    rat sum;
    int m, k = 0, ni;
    bool resort = false;
    if (c->st != CASIM_OK) return SPARE;
    if ((m = flatten(c, in, n, K_ADD, &l)) < 0) return SPARE;
    t = list(c, l) + 1;
    m = split_impure(c, t, m, &il, &ni);
    rat_int(&sum, 0);
again:
    m = fold(c, t, m, &sum, rat_add, by_mono);
    for (int i = 0; i < m;) {
        int j = i + 1;
        rat s2;
        bool ok = true;
        const ref *f;
        int nf;
        if (!is_num(c, t[i]))
            while (j < m && !is_num(c, t[j]) && by_mono(c, t[i], t[j]) == 0) j++;
        if (j - i == 1) {
            t[k++] = t[i++];
            continue;
        }
        /* a run of like terms: their coefficients added, one term built (all of them, or the run is left
           in order of value when the sum would not fit); one node for the run, not one for each pair */
        rat_int(&s2, 0);
        sort(c, t + i, j - i, by_coef);
        ok = add_sorted(c, t + i, j - i, &s2);
        if (!ok) {
            c->apart = true;
            while (i < j) t[k++] = t[i++];
            continue;
        }
        f = mono(c, &t[i], &nf);
        i = j;
        if (rat_is(&s2, 0, 1)) continue;
        for (int q = 0; q < nf; q++) resort |= is_num(c, f[q]);
        t[k++] = term_of(c, &s2, f, nf);
    }
    if (resort) {
        /* a run whose numbers stay apart: its coefficient joined them, so its monomial changed and it
           sorts elsewhere, maybe beside a like term (third pass). Again: each pass merged a run, so fewer
           terms each time. */
        m = k, k = 0, resort = false;
        goto again;
    }
    if (!rat_is(&sum, 0, 1)) t[k++] = num(c, &sum);
    for (int i = 0; i < ni; i++) t[k++] = list(c, il)[i];   /* impure terms, in order, last */
    if (k == 0) return small(c, C_0);
    if (k == 1) return t[0];
    list(c, l)[0] = 0;
    return in_place(c, K_ADD, l, t, k);
}

/* A sum's content: its first term's coefficient (the constant is last, so the first term has a
   monomial). When any term holds numbers kept apart (past 2^62), only a sign: dividing such a term by more
   and multiplying it back would not meet again, and each pass would add numbers (delta review); by −1 it
   meets exactly, so −(S)÷Z reads back as written (third pass). The sign must flip exactly when the sum is
   negated, or each pass divides by −1 again: it is the sign of the largest coefficient among the first
   plain terms (no numbers apart; their monomial). Negating a term with numbers apart moves its sign onto
   them and so moves the term in the order, and like terms left apart are in order of value, which negating
   reverses; but the plain monomials keep their order, and two like terms of one size and opposite signs
   would have cancelled. With no plain term, 1. */
static rat content(ctx *c, ref s)
{
    ref t0 = list(c, nd(c, s)->a)[0];
    rat k;
    rat_int(&k, 1);
    for (int i = 0; c->apart && i < nd(c, s)->n; i++) {
        int nf;
        ref t = list(c, nd(c, s)->a)[i];
        const ref *f = is_num(c, t) ? NULL : mono(c, &t, &nf);
        for (int q = 0; f && q < nf; q++)
            if (is_num(c, f[q])) {
                ref first = 0;
                rat big;
                rat_int(&big, 0);
                for (int j = 0; j < nd(c, s)->n; j++) {
                    ref u = list(c, nd(c, s)->a)[j];
                    const ref *g = is_num(c, u) ? NULL : mono(c, &u, &nf);
                    bool plain = g != NULL;
                    rat v, a;
                    for (int r = 0; g && r < nf; r++) plain &= !is_num(c, g[r]);
                    if (!plain || (first && by_mono(c, first, u) != 0)) continue;
                    first = u;
                    v = term_coef(c, u), a = v;
                    a.p = a.p < 0 ? -a.p : a.p;
                    if (frac_cmp(a.p, a.q, big.p < 0 ? -big.p : big.p, big.q) > 0) big = v;
                }
                if (big.p < 0) rat_int(&k, -1);
                return k;
            }
    }
    if (nd(c, t0)->kind == K_MUL && has_coef(c, list(c, nd(c, t0)->a), nd(c, t0)->n))
        k = *val(c, list(c, nd(c, t0)->a)[0]);
    return k;
}

/* The sum s with every term divided by k (k not 0); 0 when a quotient would not fit. Built through add():
   a run of like terms left apart because its sum did not fit may fit once divided, and a negative k
   reverses the order of value (delta review), so the result may merge, or not be a sum at all. */
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
            r = term_of(c, &q, f, nf);              /* numbers apart in f: the sign joins them */
        }
        list(c, l)[i] = r;
    }
    return c->apart ? add(c, list(c, l), n) : flat_node(c, K_ADD, l, n);   /* nothing apart: nothing merges */
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
            if ((p = sum_div(c, t[i], &k)) == 0) continue;
            if (p == SPARE) return SPARE;
            t[i] = p, cv = ck;
            if (nd(c, p)->kind != K_ADD || (k = content(c, p), !rat_is(&k, 1, 1))) again = true;   /* it merged */
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
/* CAS 008 rule 1: the integer q-th root of n >= 0, when exact. No root >= 2 of an n below 2^62 has q > 62 (rule 8:
   the guard keeps an STU exponent chain's huge q from a long loop; stamp_test is its regression). */
static bool int_root(int64_t n, long q, int64_t *r)
{
    int64_t lo = 2, hi = (int64_t)1 << 31;
    if (n < 0) return false;
    if (n < 2 || q == 1) return *r = n, true;
    if (q > 62) return false;
    /* bisection on t in [2, 2^31] (t^q <= n < 2^62, q >= 2), t^q overflow-checked and cut off past n: integers only,
       so no libm pow is linked (abacus #5589: it cost the firmware 3.9 KB) */
    while (lo <= hi) {
        int64_t t = lo + (hi - lo) / 2, v = 1;
        bool over = false;
        for (long i = 0; i < q && !over; i++) over = __builtin_mul_overflow(v, t, &v) || v > n;
        if (!over && v == n) return *r = t, true;
        if (over) hi = t - 1;
        else lo = t + 1;
    }
    return false;
}

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
        if (rat_is(v, 0, 1) && is_num(c, e) && val(c, e)->p < 0) {
            c->st = CASIM_UNDEFINED;            /* CAS 008 rule 5: an exact 0 in a denominator has no value */
            return SPARE;
        }
        if (w && rat_pow(&p, v, k_)) return num(c, &p);
        if (!w && is_num(c, e) && v->p > 0) {
            /* CAS 008 rule 1: a perfect rational power, (p/q)^(m/n) with p and q exact n-th powers */
            const rat *x = val(c, e);
            int64_t rp, rq;
            rat root;
            if (int_root(v->p, x->q, &rp) && int_root(v->q, x->q, &rq) && rat_make(&root, rp, rq) && rat_pow(&p, &root, x->p))
                return num(c, &p);
        }
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

/* ---- CAS 009: a constant's value ----
   The value of a tree that holds no letter, by the core's own arithmetic (abn), in the angle mode: for the bounds of
   a definite integral, the interval check, and ABS and SGN of a constant. CV_UNDEF: no value (a 0 divided, LN(0));
   CV_NOTREAL: outside the real domain (SQRT(-1), LN(-1)); CV_NO: not a constant, or a function it can't evaluate. */

enum { CV_OK, CV_NO, CV_UNDEF, CV_NOTREAL };

static abn abn_pi(void)
{
    abn y = abn_zero(), m = abn_from_int(-1), p;
    abn_atan2(&p, &y, &m);
    return p;
}

static int cv_status(abn_status st)
{
    return st == ABN_OK ? CV_OK : st == ABN_DIV_ZERO || st == ABN_RANGE ? CV_UNDEF : CV_NOTREAL;
}

/* an int64 as abn, in two parts of 10^9 (|v| < 2^62) */
static bool i64_abn(int64_t v, abn *r)
{
    abn hi = abn_from_int((int32_t)(v / 1000000000)), lo = abn_from_int((int32_t)(v % 1000000000)), g = abn_from_int(1000000000);
    return abn_mul(r, &hi, &g) == ABN_OK && abn_add(r, r, &lo) == ABN_OK;
}

static bool rat_abn(const rat *q, abn *v)
{
    abn p, d;
    return i64_abn(q->p, &p) && i64_abn(q->q, &d) && abn_div(v, &p, &d) == ABN_OK;
}

/* the angle mode's factor to radians (to_rad) or from them */
static abn mode_scale(int angle, bool to_rad)
{
    abn pi = abn_pi(), d = abn_from_int(angle == CASIM_DEG ? 180 : 200), r;
    if (angle == CASIM_RAD) return abn_from_int(1);
    abn_div(&r, to_rad ? &pi : &d, to_rad ? &d : &pi);
    return r;
}

static int const_val(ctx *c, ref r, abn *v);

/* v^k for a whole k, by squaring (abn_mul, abn_div): exact as the core's own arithmetic, any sign of v */
static int abn_ipow(abn *v, long k)
{
    abn b = *v, acc = abn_from_int(1);
    unsigned long m = k < 0 ? -(unsigned long)k : (unsigned long)k;
    if (abn_is_zero(v)) return k < 0 ? CV_UNDEF : (*v = k == 0 ? acc : abn_zero(), CV_OK);
    for (; m; m >>= 1) {
        if (m & 1 && abn_mul(&acc, &acc, &b) != ABN_OK) return CV_UNDEF;
        if (m > 1 && abn_mul(&b, &b, &b) != ABN_OK) return CV_UNDEF;
    }
    if (k < 0) {
        abn one = abn_from_int(1);
        return cv_status(abn_div(v, &one, &acc));
    }
    return *v = acc, CV_OK;
}

/* v^e without abn_pow (abacus #5589: libbid's pow and its tables cost Rev A 38 KB, and only this called it): a whole
   e exactly; p/2 through abn_sqrt; any other e, for v > 0, as exp(e ln v). A negative v under a non-whole e isn't
   real; 0 under a negative e has no value. */
static int const_pow(ctx *c, ref e, abn *v)
{
    abn t, l;
    int st;
    long k;
    if (is_num(c, e) && rat_whole(val(c, e), &k)) return abn_ipow(v, k);
    if (is_num(c, e) && val(c, e)->q == 2) {
        if (abn_is_negative(v)) return CV_NOTREAL;
        if (abn_is_zero(v)) return val(c, e)->p < 0 ? CV_UNDEF : CV_OK;
        if ((st = cv_status(abn_sqrt(v, v))) != CV_OK) return st;
        return abn_ipow(v, (long)val(c, e)->p);
    }
    if ((st = const_val(c, e, &t)) != CV_OK) return st;
    if (abn_is_negative(v)) return CV_NOTREAL;
    if (abn_is_zero(v)) return abn_is_negative(&t) ? CV_UNDEF : abn_is_zero(&t) ? (*v = abn_from_int(1), CV_OK) : CV_OK;
    if ((st = cv_status(abn_ln(&l, v))) != CV_OK || (st = cv_status(abn_mul(&l, &l, &t))) != CV_OK) return st;
    return cv_status(abn_exp(v, &l));
}

static __attribute__((noinline)) int const_val(ctx *c, ref r, abn *v)
{
    const node *x;
    abn t, k;
    int st;
    if (!enter(c)) return CV_NO;
    x = nd(c, r);
    st = CV_NO;
    switch (x->kind) {
    case K_NUM: st = rat_abn(val(c, r), v) ? CV_OK : CV_NO; break;
    case K_PI: *v = abn_pi(), st = CV_OK; break;
    case K_ADD:
    case K_MUL:
        *v = abn_from_int(x->kind == K_MUL);
        st = CV_OK;
        for (int i = 0; i < x->n && st == CV_OK; i++)
            if ((st = const_val(c, list(c, x->a)[i], &t)) == CV_OK)
                st = cv_status(x->kind == K_MUL ? abn_mul(v, v, &t) : abn_add(v, v, &t));
        break;
    case K_POW:
        if ((st = const_val(c, x->a, v)) != CV_OK) break;
        st = const_pow(c, nd(c, r)->b, v);
        break;
    case K_FN: {
        int f = x->id;
        if ((st = const_val(c, x->a, &t)) != CV_OK) break;
        if ((f == F_LN || f == F_LOG) && abn_is_zero(&t)) {
            st = CV_UNDEF;
            break;
        }
        if (f == F_SIN || f == F_COS || f == F_TAN) k = mode_scale(c->angle, true), abn_mul(&t, &t, &k);
        switch (f) {
        case F_LN: st = cv_status(abn_ln(v, &t)); break;
        case F_LOG: st = cv_status(abn_log10(v, &t)); break;
        case F_EXP: st = cv_status(abn_exp(v, &t)); break;
        case F_ALOG: st = cv_status(abn_exp10(v, &t)); break;
        case F_SIN: st = cv_status(abn_sin(v, &t)); break;
        case F_COS: st = cv_status(abn_cos(v, &t)); break;
        case F_TAN: st = cv_status(abn_tan(v, &t)); break;
        case F_ASIN: st = cv_status(abn_asin(v, &t)); break;
        case F_ACOS: st = cv_status(abn_acos(v, &t)); break;
        case F_ATAN: st = cv_status(abn_atan(v, &t)); break;
        case F_SINH: st = cv_status(abn_sinh(v, &t)); break;
        case F_COSH: st = cv_status(abn_cosh(v, &t)); break;
        case F_TANH: st = cv_status(abn_tanh(v, &t)); break;
        case F_ASINH: st = cv_status(abn_asinh(v, &t)); break;
        case F_ACOSH: st = cv_status(abn_acosh(v, &t)); break;
        case F_ATANH: st = cv_status(abn_atanh(v, &t)); break;
        case F_ABS: *v = abn_abs(&t), st = CV_OK; break;
        case F_SGN: *v = abn_from_int(abn_is_zero(&t) ? 0 : abn_is_negative(&t) ? -1 : 1), st = CV_OK; break;
        default: st = CV_NO; break;
        }
        if (st == CV_OK && (f == F_ASIN || f == F_ACOS || f == F_ATAN))
            k = mode_scale(c->angle, false), st = cv_status(abn_mul(v, v, &k));
        break;
    }
    default: break;                     /* a letter, an opaque name: not a constant */
    }
    c->depth--;
    return st;
}

/* the sign of a constant, when the evaluator decides it beyond 1E-20 (CAS 009 rule 5); 0 when it can't */
static int const_sign(ctx *c, ref a)
{
    abn v, tiny, w;
    if (const_val(c, a, &v) != CV_OK || abn_pow10(&tiny, -20) != ABN_OK) return 0;
    w = abn_abs(&v);
    if (abn_cmp(&w, &tiny) <= 0) return 0;
    return abn_is_negative(&v) ? -1 : 1;
}

/* ---- CAS 008: exact values ----
   One table, sin(nπ/12), serves SIN, COS (n + 6) and, apart, TAN, in all three angle modes: a constant argument
   is first turned into a rational multiple of π. Only the multiples of π/6 and π/4 are exact; each value is
   s × p/q × SQRT(k). */

typedef struct {
    signed char s, p, q, k;             /* s × p/q × SQRT(k); p = 0 is the value 0 */
} xval;

/* sin(nπ/12), cos(nπ/12) (cos: n + 6) and tan(nπ/12); false where not exact (an odd multiple of π/12, TAN's poles) */
static bool table_val(int f, long n, xval *v)
{
    static const signed char SINM[7][3] = {{0, 1, 1}, {0, 0, 0}, {1, 2, 1}, {1, 2, 2}, {1, 2, 3}, {0, 0, 0}, {1, 1, 1}};
    static const signed char TANM[12][4] = {{1, 0, 1, 1}, {0, 0, 0, 0}, {1, 1, 3, 3}, {1, 1, 1, 1}, {1, 1, 1, 3},
                                            {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {-1, 1, 1, 3}, {-1, 1, 1, 1},
                                            {-1, 1, 3, 3}, {0, 0, 0, 0}};
    int m;
    if (f == F_TAN) {
        m = (int)(((n % 12) + 12) % 12);
        if (TANM[m][2] == 0) return false;
        *v = (xval){TANM[m][0], TANM[m][1], TANM[m][2], TANM[m][3]};
        return true;
    }
    if (f == F_COS) n += 6;
    m = (int)(((n % 24) + 24) % 24);
    v->s = (signed char)(m >= 12 ? -1 : 1);
    m %= 12;
    if (m > 6) m = 12 - m;
    if (SINM[m][1] == 0) return false;
    v->p = SINM[m][0], v->q = SINM[m][1], v->k = SINM[m][2];
    return true;
}

static ref xval_tree(ctx *c, const xval *v)
{
    rat r;
    ref t;
    rat_make(&r, v->s * v->p, v->q);
    t = num(c, &r);
    if (v->k > 1 && v->p != 0) {
        rat kv;
        rat_int(&kv, v->k);
        t = mul2(c, t, power(c, num(c, &kv), small(c, C_HALF)));
    }
    return t;
}

/* a constant angle as n, a multiple of π/12 (rule 2): RAD 0, π or q×π; DEG d (d/180 π); GRAD d (d/200 π) */
static bool angle_n(ctx *c, ref a, long *n)
{
    const node *x = nd(c, a);
    rat r, t, twelve;
    if (c->angle != CASIM_RAD) {
        rat d;
        if (!is_num(c, a)) return false;
        rat_make(&d, 1, c->angle == CASIM_DEG ? 180 : 200);
        if (!rat_mul(&r, val(c, a), &d)) return false;
    } else if (is_num(c, a) && rat_is(val(c, a), 0, 1)) rat_int(&r, 0);
    else if (x->kind == K_PI) rat_int(&r, 1);
    else if (x->kind == K_MUL && x->n == 2 && is_num(c, list(c, x->a)[0]) && nd(c, list(c, x->a)[1])->kind == K_PI)
        r = *val(c, list(c, x->a)[0]);
    else return false;
    rat_int(&twelve, 12);
    return rat_mul(&t, &r, &twelve) && rat_whole(&t, n);
}

/* a value the table prints, read back from its tree (rule 4): p/q, ±p/q × SQRT(k), SQRT(k) */
static bool tree_xval(ctx *c, ref a, xval *v)
{
    const node *x = nd(c, a);
    rat co;
    long kk;
    int k = 1;
    rat_int(&co, 1);
    if (is_num(c, a)) co = *val(c, a);
    else {
        ref rt_ = a;
        if (x->kind == K_MUL && x->n == 2 && is_num(c, list(c, x->a)[0])) co = *val(c, list(c, x->a)[0]), rt_ = list(c, x->a)[1];
        x = nd(c, rt_);
        if (x->kind != K_POW || !is_num(c, x->a) || !is_k(c, x->b, 1, 2)) return false;
        if (!rat_whole(val(c, x->a), &kk) || (kk != 2 && kk != 3)) return false;
        k = (int)kk;
    }
    if (co.p > 127 || co.p < -127 || co.q > 127) return false;
    v->s = (signed char)(co.p < 0 ? -1 : 1), v->p = (signed char)(co.p < 0 ? -co.p : co.p), v->q = (signed char)co.q;
    v->k = (signed char)k;
    return true;
}

/* ASIN, ACOS, ATAN of a table value: the principal angle in the mode (ASIN, ATAN in [-π/2, π/2], ACOS in [0, π]) */
static ref exact_inverse(ctx *c, int f, ref a)
{
    xval v, t;
    int g = f == F_ASIN ? F_SIN : f == F_ACOS ? F_COS : F_TAN, lo = f == F_ACOS ? 0 : -6, hi = f == F_ACOS ? 12 : 6;
    if (!tree_xval(c, a, &v)) return 0;
    for (int n = lo; n <= hi; n++) {
        rat r;
        if (!table_val(g, n, &t)) continue;
        if (t.p == 0 ? v.p != 0 : (t.s != v.s || t.p * v.q != v.p * t.q || t.k != v.k)) continue;
        if (c->angle == CASIM_DEG) return rat_int(&r, 15 * n), num(c, &r);
        if (c->angle == CASIM_GRAD) return rat_make(&r, 50 * n, 3), num(c, &r);
        if (n == 0) return small(c, C_0);
        rat_make(&r, n, 12);
        return mul2(c, num(c, &r), leaf(c, K_PI, 0));
    }
    return 0;
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
    case F_LN:
        if (x->kind == K_FN && x->id == F_EXP) return x->a;
        if (is_k(c, a, 1, 1)) return small(c, C_0);                 /* CAS 008 */
        break;
    case F_EXP:
        if (x->kind == K_FN && x->id == F_LN) return x->a;
        if (is_k(c, a, 0, 1)) return small(c, C_1);                 /* CAS 008 */
        if (x->kind == K_MUL && x->n == 2 && is_num(c, list(c, x->a)[0])) {
            /* EXP(k×LN(a)) = a^k, a a positive number (exact when power() makes it so) */
            ref u = list(c, x->a)[1];
            if (nd(c, u)->kind == K_FN && nd(c, u)->id == F_LN && is_num(c, nd(c, u)->a) && val(c, nd(c, u)->a)->p > 0)
                return power(c, nd(c, u)->a, list(c, x->a)[0]);
        }
        break;
    case F_SIN:
    case F_COS:
    case F_TAN: {
        long n;
        xval v;
        if (angle_n(c, a, &n) && table_val(f, n, &v)) return xval_tree(c, &v);
        break;
    }
    case F_ASIN:
    case F_ACOS:
    case F_ATAN:
        if ((r = exact_inverse(c, f, a)) != 0) return r;
        break;
    case F_LOG: if (x->kind == K_FN && x->id == F_ALOG) return x->a; break;
    case F_ALOG: if (x->kind == K_FN && x->id == F_LOG) return x->a; break;
    case F_ABS:
        if (is_num(c, a)) {
            rat v = *val(c, a);
            v.p = v.p < 0 ? -v.p : v.p;
            return num(c, &v);
        } else {
            int sg = const_sign(c, a);              /* CAS 009 rule 5: ABS of a constant whose sign is decided */
            if (sg) return sg > 0 ? a : neg(c, a);
        }
        break;
    case F_SGN: {
        int sg = const_sign(c, a);
        if (sg) return small(c, sg > 0 ? C_1 : C_M1);
        break;
    }
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

/* An operator's level in the text's model: in STU (unit 031) a minus starting an exponent ranks as any
   unary minus, below ^, so 2^-3^2 is 2^-(3^2), where the 33s and 35s bind it to the next operand. */
static int level_of(const ctx *c, int op)
{
    return c->rtl && op == O_PNEG ? LEVEL[O_NEG] : LEVEL[op];
}

/* Applies the operators that bind at least as tightly as level, down to the nearest opener. */
static void reduce_to(ctx *c, pstack *p, int level)
{
    while (p->no > 0 && p->ops[p->no - 1].op < M_PAREN && level_of(c, p->ops[p->no - 1].op) >= level) reduce(c, p);
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
        c->st = CASIM_NO_ROOM;
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
            /* left to right: apply what binds as tightly first; STU's ^ is right to left, so only what binds
               more tightly (2^3^2 is 2^(3^2)) */
            reduce_to(c, &p, op == O_EQ ? 1 : LEVEL[op] + (c->rtl && op == O_POW));
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

/* A power with a symbolic exponent, and the left chain under it, ((b^x)^y)^z, printed with its parentheses
   (decision 52: never a bare b^x^y^z, which a mode grouping powers right to left would read otherwise), in a
   loop: the chain's depth does not reach the stack. */
static void put_chain(ctx *c, out_t *w, ref f)
{
    int k = 0;
    ref b = f;
    while (nd(c, b)->kind == K_POW && !is_num(c, nd(c, b)->b)) k++, b = nd(c, b)->a;
    for (int i = 1; i < k; i++) puts_(w, "(");             /* decision 52: ((b^x)^y)^z, never b^x^y^z */
    print_in(c, w, b, level(c, b) < 5);                     /* a base with a ^ of its own: (X^2)^Y */
    for (int i = k - 1; i >= 0; i--) {
        ref y = f, x;
        for (int j = 0; j < i; j++) y = nd(c, y)->a;
        x = nd(c, y)->b;
        puts_(w, "^");
        print_in(c, w, x, level(c, x) < 5);
        if (i > 0) puts_(w, ")");
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
        print_in(c, w, b, level(c, b) < 5);
        puts_(w, "^(-");
        put_exp(c, w, x);
        puts_(w, ")");
    } else if (abs_is(c, x, 1, 1)) print_in(c, w, b, level(c, b) < min);
    else if (abs_is(c, x, 1, 2)) {
        puts_(w, "SQRT(");
        print(c, w, b);
        puts_(w, ")");
    } else {
        print_in(c, w, b, level(c, b) < 5);                 /* (X^Y)^0.3 (decision 52) */
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
    /* ±1/2 prints as SQRT(u), or in a denominator; but 0^(-1/2) prints as written, 0^(-0.5), with its ^
       (review: Y^(0^-0.5) came out as a bare chain) */
    return abs_is(c, x, 1, 2) && !(num_negative(c, x) && !below(c, f)) ? 5 : 4;
}

/* A product with draws: its factors in their order, × or ÷ as each comes, so the draws happen in the
   order they were written (1÷RAND×RAND is not RAND÷RAND; review). Out of line: off print's frame. */
static __attribute__((noinline)) void print_drawn(ctx *c, out_t *w, const ref *f, int n, int start, bool flip)
{
    bool top, bottom;
    int items;
    /* the coefficient whole and first, p÷q as one item (RAND÷3 prints 1÷3×RAND; review) */
    coef_parts(c, start ? f[0] : 0, &top, &bottom);
    items = put_coef(c, w, start ? f[0] : 0, flip, true);
    if (bottom) {
        if (!items) puts_(w, "1");
        put_coef(c, w, start ? f[0] : 0, flip, false);
        items = 1;
    }
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
    if (!enter(c)) return;              /* stopped: c->st is NO_ROOM, which finish() reports */
    print_(c, w, r);
    c->depth--;
}

/* ---- the API ---- */

static casim_status finish(ctx *c, ref n, char *out, size_t cap)
{
    out_t w = {out, cap, 0, false};
    if (cap == 0) return CASIM_TOO_LONG;
    out[0] = '\0';
    if (c->st == CASIM_OK && (n == SPARE || n == 0)) c->st = CASIM_NO_ROOM;     /* never an empty OK */
    if (c->st == CASIM_OK) {
        print(c, &w, n);                /* the depth guard may stop it: c->st is then NO_ROOM */
        if (w.full) c->st = CASIM_TOO_LONG;
    }
    if (c->st == CASIM_SYNTAX) {
        char d[12];
        int k = 0, e = c->errpos;
        do d[k++] = (char)('0' + e % 10); while ((e /= 10) && k < 11);
        while (k) put(&w, &d[--k], 1);
    } else if (c->st == CASIM_NOT_DIFFERENTIABLE) put(&w, c->bad, (size_t)c->badlen);
    if (w.full || c->st == CASIM_TOO_LONG || c->st == CASIM_NO_ROOM) out[0] = '\0';
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
        c->st = CASIM_NO_ROOM;
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

static __attribute__((noinline)) casim_status do_derive(const char *eqn, int var, int angle, char *out, size_t cap,
                                                         void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;                 /* review: no text is a bad argument */
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.var = var;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = canon(&c, n);
    if (c.st == CASIM_OK) n = deriv(&c, n);
    return finish(&c, n, out, cap);
}

static __attribute__((noinline)) casim_status do_simplify(const char *eqn, int angle, char *out, size_t cap, void *arena,
                                                           size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;                 /* review: no text is a bad argument */
    if (angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
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
    /* a sum c_k as a factor reads back made primitive, which can merge a run its sum left apart: so it is
       printed as that, and collecting again changes nothing (delta review: 547 in 20,000) */
    for (int i = 0; c->apart && i < nc; i++)
        if (nd(c, ct[i].c)->kind == K_ADD) ct[i].c = mul(c, &ct[i].c, 1);
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

static __attribute__((noinline)) casim_status do_expand(const char *eqn, int angle, char *out, size_t cap, void *arena,
                                                         size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;                 /* review: no text is a bad argument */
    if (angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    c.sub_var = -1;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = compact(&c, 16, canon(&c, n));     /* the raw tree is garbage now */
    if (c.st == CASIM_OK) n = expand_fully(&c, n);
    return finish(&c, n, out, cap < SLOT ? cap : SLOT);
}

static __attribute__((noinline)) casim_status do_collect(const char *eqn, int var, int angle, char *out, size_t cap,
                                                          void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE;
    out_t w = {out, cap < SLOT ? cap : SLOT, 0, false};
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;                 /* review: no text is a bad argument */
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
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

static __attribute__((noinline)) casim_status do_subst(const char *eqn, int var, const char *repl, int angle, char *out,
                                                        size_t cap, void *arena, size_t arena_cap, bool rtl,
                                                        bool repl_rtl)
{
    ctx c;
    ref n = SPARE, rr;
    int chars = 0;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;                 /* review: no text is a bad argument */
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD || !repl) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st != CASIM_OK) return finish(&c, n, out, cap < SLOT ? cap : SLOT);
    /* the replacement: a second text, its literals marked (id 1), its SYNTAX offset counted as if the call
       were eqn + "|" + repl (#4264 point 6) */
    for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    c.s2 = repl;
    c.s = repl;
    c.no_eq = true;                     /* a replacement is an expression, not an equation (review) */
    c.rtl = repl_rtl;                   /* read as its own model (CAS 004) */
    rr = parse(&c);
    c.rtl = rtl;
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

/* ---- the API ---- */

casim_status casim_derive(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    return do_derive(eqn, var, angle, out, cap, arena, arena_cap, false);
}

casim_status casim_simplify(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    return do_simplify(eqn, angle, out, cap, arena, arena_cap, false);
}

casim_status casim_expand(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    return do_expand(eqn, angle, out, cap, arena, arena_cap, false);
}

casim_status casim_collect(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t arena_cap)
{
    return do_collect(eqn, var, angle, out, cap, arena, arena_cap, false);
}

casim_status casim_subst(const char *eqn, int var, const char *repl, int angle, char *out, size_t cap, void *arena,
                         size_t arena_cap)
{
    return do_subst(eqn, var, repl, angle, out, cap, arena, arena_cap, false, false);
}

/* A model the core stamps (unit 031), or 0 (read as 35). */
static bool model_ok(int m)
{
    return m == 0 || m == CASIM_MODEL_STU || m == CASIM_MODEL_33 || m == CASIM_MODEL_35;
}

/* CAS 004: one call for the calculator's menu, each text read as its stamp. */
const char *casim_version(void)
{
    return CASIM_VERSION;
}

casim_status casim_run(const casim_req *rq, char *out, size_t cap, void *arena, size_t arena_cap)
{
    bool rtl;
    if (cap) out[0] = '\0';
    if (!rq || !model_ok(rq->model)) return CASIM_BAD_ARG;
    rtl = rq->model == CASIM_MODEL_STU;
    switch (rq->op) {
    case CASIM_DERIVE: return do_derive(rq->eqn, rq->var, rq->angle, out, cap, arena, arena_cap, rtl);
    case CASIM_SIMPLIFY: return do_simplify(rq->eqn, rq->angle, out, cap, arena, arena_cap, rtl);
    case CASIM_EXPAND: return do_expand(rq->eqn, rq->angle, out, cap, arena, arena_cap, rtl);
    case CASIM_COLLECT: return do_collect(rq->eqn, rq->var, rq->angle, out, cap, arena, arena_cap, rtl);
    case CASIM_SOLVE: return casim_solve(rq->eqn, rq->var, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_INTEGRATE: return casim_integrate(rq->eqn, rq->var, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_FACTOR: return casim_factor(rq->eqn, rq->var, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_DEFINT: return casim_defint(rq->eqn, rq->var, rq->repl, rq->repl2, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_PDIV:
        return casim_pdiv(rq->eqn, rq->var, rq->repl, rq->model, rq->repl_model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_PGCD:
        return casim_pgcd(rq->eqn, rq->var, rq->repl, rq->model, rq->repl_model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_CANCEL: return casim_cancel(rq->eqn, rq->var, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_SUBST:
        if (!model_ok(rq->repl_model)) return CASIM_BAD_ARG;
        return do_subst(rq->eqn, rq->var, rq->repl, rq->angle, out, cap, arena, arena_cap, rtl,
                        rq->repl_model == CASIM_MODEL_STU);
    default: return CASIM_BAD_ARG;
    }
}

/* ---- solve (unit CAS 005) ---- */

/* The real roots of a polynomial in var with exact rational coefficients, cleared to a primitive integer
   polynomial f (degree <= 12, |coefficients| <= 10^7, else NO_ROOM). Its linear and quadratic factors over Q
   are found from numeric candidates (Aberth's method in double, a cluster's centroid polished by Newton on a
   derivative) and verified only by exact division, which also gives each factor's multiplicity; a candidate
   the numbers miss can only weaken SOLVED to PARTIAL, never print a wrong root. int64 is enough: every
   polynomial divided is a factor of f, so by Mignotte's bound its coefficients are at most 1.16E10, and
   every intermediate of a division by a true factor stays under 3.3E17 (docs; abacus #4844, #4845). The
   checks stay: an overflow means "not a factor". */

#define SOLVE_DEG 12
#define SOLVE_COEF 10000000LL
#define SOLVE_DIVS 448                  /* the most divisors of any n <= 10^7 (8,648,640 has 448) */

typedef struct {
    int n;
    int64_t a[SOLVE_DEG + 1];               /* a[0] + a[1] x + ... + a[n] x^n */
} ipoly;

typedef struct {
    int deg;                                /* 1: q x - p; 2: a x^2 + b x + c */
    int64_t a, b, c;                        /* deg 1: a = q, b = p */
    int mult;
} ifac;

/* The numeric work, in the arena (off the stack): roots and candidates. */
typedef struct {
    double re[3 * SOLVE_DEG], im[3 * SOLVE_DEG], cf[SOLVE_DEG + 1];  /* n roots, then centroids */
    int count;
    ifac fac[SOLVE_DEG];                /* each factor divides f: at most its degree of them */
    int nfac;
} solve_ws;

/* h / (q x - p) exactly, into g; false when it does not divide or a step would overflow. */
static bool div_lin(const ipoly *h, int64_t q, int64_t p, ipoly *g)
{
    int64_t r, t;
    int n = h->n;
    if (n < 1 || q == 0 || h->a[n] % q) return false;
    g->n = n - 1;
    r = h->a[n] / q;
    g->a[n - 1] = r;
    for (int i = n - 1; i >= 1; i--) {
        if (__builtin_mul_overflow(p, r, &t) || __builtin_add_overflow(h->a[i], t, &t) || t % q) return false;
        r = t / q;
        g->a[i - 1] = r;
    }
    return !__builtin_mul_overflow(p, r, &t) && !__builtin_add_overflow(h->a[0], t, &t) && t == 0;
}

/* h / (a x^2 + b x + c) exactly, into g. */
static bool div_quad(const ipoly *h, int64_t a, int64_t b, int64_t c, ipoly *g)
{
    ipoly r = *h;
    int n = h->n;
    if (n < 2 || a == 0) return false;
    g->n = n - 2;
    for (int i = n; i >= 2; i--) {
        int64_t qc, t;
        if (r.a[i] % a) return false;
        qc = r.a[i] / a;
        g->a[i - 2] = qc;
        if (__builtin_mul_overflow(qc, b, &t) || __builtin_sub_overflow(r.a[i - 1], t, &r.a[i - 1])) return false;
        if (__builtin_mul_overflow(qc, c, &t) || __builtin_sub_overflow(r.a[i - 2], t, &r.a[i - 2])) return false;
    }
    return r.a[1] == 0 && r.a[0] == 0;
}

/* cos and sin of 2π/n, n = 1..12: Aberth's starting circle without a trigonometric library. */
static const double ROT[13][2] = {
    {1, 0}, {1, 0}, {-1, 0}, {-0.5, 0.86602540378443865}, {0, 1}, {0.30901699437494742, 0.95105651629515357},
    {0.5, 0.86602540378443865}, {0.62348980185873353, 0.78183148246802981}, {0.70710678118654752, 0.70710678118654752},
    {0.76604444311897804, 0.64278760968653933}, {0.80901699437494742, 0.58778525229247313},
    {0.84125353283118117, 0.54064081745559756}, {0.86602540378443865, 0.5},
};

/* All n complex roots of f (n >= 1), Aberth-Ehrlich in double, at most 150 rounds (measured on the S3: 0.44 s
   at worst with the search). */
static __attribute__((noinline)) void aberth(const ipoly *f, solve_ws *s)
{
    int n = f->n;
    double mx = 0, rad, wr = 0.92106099400288508, wi = 0.38941834230865049;     /* e^(0.4i): off the axes */
    for (int i = 0; i <= n; i++) s->cf[i] = (double)f->a[i] / (double)f->a[n];
    for (int i = 0; i < n; i++) mx = s->cf[i] < 0 ? (-s->cf[i] > mx ? -s->cf[i] : mx) : (s->cf[i] > mx ? s->cf[i] : mx);
    rad = 0.5 * (1 + mx);
    for (int i = 0; i < n; i++) {
        double t;
        s->re[i] = rad * wr, s->im[i] = rad * wi;
        t = wr * ROT[n][0] - wi * ROT[n][1];
        wi = wr * ROT[n][1] + wi * ROT[n][0], wr = t;
    }
    for (int it = 0; it < 150; it++) {
        double worst = 0;
        for (int i = 0; i < n; i++) {
            double pr = 1, pi = 0, dr = 0, di = 0, zr = s->re[i], zi = s->im[i], t, den, nr, ni, sr = 0, si = 0;
            for (int j = n - 1; j >= 0; j--) {
                t = dr * zr - di * zi + pr, di = dr * zi + di * zr + pi, dr = t;
                t = pr * zr - pi * zi + s->cf[j], pi = pr * zi + pi * zr, pr = t;
            }
            den = dr * dr + di * di;
            if (den == 0) continue;
            nr = (pr * dr + pi * di) / den, ni = (pi * dr - pr * di) / den;          /* f/f' */
            for (int j = 0; j < n; j++)
                if (j != i) {
                    double ar = zr - s->re[j], ai = zi - s->im[j], d2 = ar * ar + ai * ai;
                    if (d2 != 0) sr += ar / d2, si -= ai / d2;                       /* 1/(z - z_j) */
                }
            {
                double tr = 1 - (nr * sr - ni * si), ti = -(nr * si + ni * sr), td = tr * tr + ti * ti, xr, xi, m;
                if (td == 0) continue;
                xr = (nr * tr + ni * ti) / td, xi = (ni * tr - nr * ti) / td;
                s->re[i] -= xr, s->im[i] -= xi;
                m = s->re[i] * s->re[i] + s->im[i] * s->im[i];
                t = (xr * xr + xi * xi) / (m > 1 ? m : 1);
                if (t > worst) worst = t;
            }
        }
        if (worst < 1e-30) break;
    }
    s->count = n;
}

static const double RADIUS2[3] = {1e-6, 1e-3, 0.06};        /* squared distance, relative to |z|^2 */

/* The cluster of approximation i at squared radius r2 (relative to |z|^2), its members marked in grp, and its
   centroid, polished by Newton on the (m-1)th derivative, where an m-fold root is simple. Returns m. */
static int centroid(const ipoly *f, const solve_ws *s, int i, int *grp, double r2, double *cr, double *ci)
{
    double gr = s->re[i], gi = s->im[i], scale = s->re[i] * s->re[i] + s->im[i] * s->im[i], d[SOLVE_DEG + 1];
    int m = 1, nd;
    grp[i] = i;
    if (scale < 1) scale = 1;
    for (int j = i + 1; j < f->n; j++) {
        double ar = s->re[i] - s->re[j], ai = s->im[i] - s->im[j];
        if (grp[j] < 0 && ar * ar + ai * ai < r2 * scale) grp[j] = i, gr += s->re[j], gi += s->im[j], m++;
    }
    if (m > 1) {
        gr /= m, gi /= m;
        nd = f->n - (m - 1);
        for (int j = 0; j <= nd; j++) {
            double v = (double)f->a[j + m - 1];
            for (int t = 1; t <= m - 1; t++) v *= (double)(j + t);
            d[j] = v;
        }
        for (int it = 0; it < 8 && nd >= 1; it++) {
            double pr = d[nd], pi = 0, dr = 0, di = 0, t, den;
            for (int j = nd - 1; j >= 0; j--) {
                t = dr * gr - di * gi + pr, di = dr * gi + di * gr + pi, dr = t;
                t = pr * gr - pi * gi + d[j], pi = pr * gi + pi * gr, pr = t;
            }
            den = dr * dr + di * di;
            if (den == 0) break;
            gr -= (pr * dr + pi * di) / den, gi -= (pi * dr - pr * di) / den;
        }
    }
    *cr = gr, *ci = gi;
    return m;
}

/* Clusters of approximations (a multiple root's ring): each centroid, polished by Newton on the (m-1)th
   derivative, where an m-fold root is simple, added as a candidate beside the approximations. An m-fold
   root's ring has a radius near eps^(1/m) of its size, so clusters are formed at three radii, the widest for
   m up to 12 (the review of 93daf60: one radius missed m >= 7). */
static __attribute__((noinline)) void clusters(const ipoly *f, solve_ws *s)
{
    int n = f->n;
    for (int pass = 0; pass < 3; pass++) {
        int grp[SOLVE_DEG];
        for (int i = 0; i < n; i++) grp[i] = -1;
        for (int i = 0; i < n; i++) {
            double gr, gi;
            if (grp[i] >= 0) continue;
            if (centroid(f, s, i, grp, RADIUS2[pass], &gr, &gi) == 1 || s->count >= 3 * SOLVE_DEG) continue;
            s->re[s->count] = gr, s->im[s->count] = gi, s->count++;
        }
    }
}

/* The nearest integer to x, or false past 9E18. */
static bool round64(double x, int64_t *r)
{
    if (x > 9e18 || x < -9e18 || x != x) return false;
    *r = (int64_t)(x < 0 ? x - 0.5 : x + 0.5);
    return true;
}

/* The positive divisors of v (|v| <= 10^7: at most 448), into d; returns how many. */
static int divisors_of(int64_t v, int32_t *d, int cap)
{
    int k = 0;
    if (v < 0) v = -v;
    for (int64_t i = 1; i * i <= v && k < cap; i++)
        if (v % i == 0) {
            d[k++] = (int32_t)i;
            if (i != v / i && k < cap) d[k++] = (int32_t)(v / i);
        }
    return k;
}

static void add_fac(solve_ws *s, int deg, int64_t a, int64_t b, int64_t c, int mult)
{
    if (s->nfac < SOLVE_DEG) s->fac[s->nfac++] = (ifac){deg, a, b, c, mult};
}

/* The linear and quadratic factors of f (no root 0), divided out of it, each with its multiplicity. dq is
   room for lc's divisors. */
static __attribute__((noinline)) void factors(ctx *c, ipoly *f, solve_ws *s, int32_t *dq)
{
    ipoly g;
    int nq;
    (void)c;
    if (f->n < 1) return;
    aberth(f, s);
    clusters(f, s);
    nq = divisors_of(f->a[f->n], dq, SOLVE_DIVS);
    /* rational roots: a real candidate x and q | lc give p = round(q x), p | a_0 */
    for (int r = 0; r < s->count && f->n >= 1; r++) {
        double ax = s->re[r] < 0 ? -s->re[r] : s->re[r], ai = s->im[r] < 0 ? -s->im[r] : s->im[r];
        if (ai > 1e-3 * (ax > 1 ? ax : 1)) continue;
        for (int i = 0; i < nq && f->n >= 1; i++) {
            int64_t q = dq[i], p;
            int m = 0;
            if (!round64(s->re[r] * (double)q, &p) || p == 0 || f->a[f->n] % q || f->a[0] % p || gcd64(p, q) != 1) continue;
            while (f->n >= 1 && div_lin(f, q, p, &g)) *f = g, m++;
            if (m) add_fac(s, 1, q, p, 0, m);
        }
    }
    /* quadratic factors: a pair of candidates (real, or conjugate) gives sum and product; a | lc gives
       b = -round(a sum), c = round(a product), c | a_0 */
    for (int i = 0; i < s->count && f->n >= 2; i++)
        for (int j = i + 1; j < s->count && f->n >= 2; j++) {
            double sum = s->re[i] + s->re[j], pim = s->im[i] + s->im[j];
            double prod = s->re[i] * s->re[j] - s->im[i] * s->im[j], qim = s->re[i] * s->im[j] + s->im[i] * s->re[j];
            double as = sum < 0 ? -sum : sum, ap = prod < 0 ? -prod : prod;
            if ((pim < 0 ? -pim : pim) > 1e-3 * (as > 1 ? as : 1) || (qim < 0 ? -qim : qim) > 1e-3 * (ap > 1 ? ap : 1)) continue;
            nq = divisors_of(f->a[f->n], dq, SOLVE_DIVS);
            for (int u = 0; u < nq && f->n >= 2; u++) {
                int64_t a = dq[u], b, cc;
                int m = 0;
                if (!round64(-sum * (double)a, &b) || !round64(prod * (double)a, &cc) || cc == 0 || f->a[0] % cc) continue;
                while (f->n >= 2 && div_quad(f, a, b, cc, &g)) *f = g, m++;
                if (m) add_fac(s, 2, a, b, cc, m);
            }
        }
    /* what is left: degree 1 or 2 is a factor itself */
    if (f->n == 1 || f->n == 2) {
        int64_t sg = f->a[f->n] < 0 ? -1 : 1;
        if (f->n == 1) add_fac(s, 1, sg * f->a[1], -sg * f->a[0], 0, 1);
        else add_fac(s, 2, sg * f->a[2], sg * f->a[1], sg * f->a[0], 1);
        f->n = 0, f->a[0] = 1;
    }
}

/* A root: rational p/q (q > 0), or (A + B√r)/D with r square-free > 1, D > 0, B != 0 (its sign the root's). */
typedef struct {
    bool irr;
    int64_t p, q, A, B, r, D;
    int mult;
    abn v;
} iroot;

/* v as an abn, exactly (|v| < 10^18). */
static abn abn_of(int64_t v)
{
    abn hi = abn_from_int((int32_t)(v / 1000000000)), lo = abn_from_int((int32_t)(v % 1000000000)), g = abn_from_int(1000000000), r;
    if (abn_mul(&r, &hi, &g) != ABN_OK || abn_add(&r, &r, &lo) != ABN_OK) return abn_zero();
    return r;
}

/* d = k^2 r with r square-free: trial division to d's cube root, then what is left is 1, a prime, a prime
   squared or two primes (d <= 2.5E15: a quadratic factor's discriminant). */
static void square_free(int64_t d, int64_t *k, int64_t *r)
{
    *k = 1, *r = 1;
    for (int64_t p = 2; p * p * p <= d; p += p == 2 ? 1 : 2) {
        while (d % (p * p) == 0) d /= p * p, *k *= p;
        if (d % p == 0) d /= p, *r *= p;
    }
    if (d > 1) {
        int64_t s = (int64_t)__builtin_sqrt((double)d);
        while (s * s > d) s--;
        while ((s + 1) * (s + 1) <= d) s++;
        if (s * s == d) *k *= s;
        else *r *= d;
    }
}

static void add_root(iroot *rs, int *n, iroot x)
{
    for (int i = 0; i < *n; i++)
        if (rs[i].irr == x.irr && rs[i].p == x.p && rs[i].q == x.q && rs[i].A == x.A && rs[i].B == x.B &&
            rs[i].r == x.r && rs[i].D == x.D) {
            rs[i].mult += x.mult;
            return;
        }
    if (*n < SOLVE_DEG) rs[(*n)++] = x;
}

static void rational_root(iroot *rs, int *n, int64_t p, int64_t q, int mult)
{
    int64_t g = gcd64(p, q);
    iroot x = {false, 0, 1, 0, 0, 0, 1, mult, {{0}}};
    abn ap, aq;
    if (q < 0) p = -p, q = -q;
    if (g > 1) p /= g, q /= g;
    x.p = p, x.q = q;
    ap = abn_of(p), aq = abn_of(q);
    if (abn_div(&x.v, &ap, &aq) != ABN_OK) x.v = abn_zero();
    add_root(rs, n, x);
}

/* Whether p/q is a root of a x^2 + b x + c, exactly (a p^2 + b p q + c q^2 = 0; false on an overflow). */
static bool quad_root_ok(int64_t a, int64_t b, int64_t c, int64_t p, int64_t q)
{
    int64_t t1, t2, t3;
    return !__builtin_mul_overflow(p, p, &t1) && !__builtin_mul_overflow(t1, a, &t1) &&
           !__builtin_mul_overflow(p, q, &t2) && !__builtin_mul_overflow(t2, b, &t2) &&
           !__builtin_mul_overflow(q, q, &t3) && !__builtin_mul_overflow(t3, c, &t3) &&
           !__builtin_add_overflow(t1, t2, &t1) && !__builtin_add_overflow(t1, t3, &t1) && t1 == 0;
}

/* The roots of a x^2 + b x + c (a > 0): none, one twice (discriminant 0), two rationals, or (-b ± k√r)/(2a)
   in lowest terms. A rational root is checked exactly against the quadratic before it is kept (the review
   of 93daf60: a discriminant of 0 printed two wrong roots); false when a check fails, so the caller counts
   the factor as unsolved rather than print anything. */
static bool quad_roots(iroot *rs, int *n, int64_t a, int64_t b, int64_t c, int mult)
{
    int64_t d = b * b - 4 * a * c, k, r, g;
    if (d < 0) return true;
    if (d == 0) {
        /* defensive (abacus #5077: no input reaches it): a repeated rational root leaves as a linear factor
           before any quadratic is formed, so (q x - p)^2 arrives here only if the numbers missed it as
           linear. Without this branch, the exact check below would refuse the derived roots: PARTIAL, never
           a wrong root. */
        if (!quad_root_ok(a, b, c, -b, 2 * a)) return false;
        rational_root(rs, n, -b, 2 * a, 2 * mult);
        return true;
    }
    square_free(d, &k, &r);
    if (r == 1) {
        if (!quad_root_ok(a, b, c, -b - k, 2 * a) || !quad_root_ok(a, b, c, -b + k, 2 * a)) return false;
        rational_root(rs, n, -b - k, 2 * a, mult);
        rational_root(rs, n, -b + k, 2 * a, mult);
        return true;
    }
    g = gcd64(gcd64(b, k), 2 * a);
    for (int s = -1; s <= 1; s += 2) {
        iroot x = {true, 0, 1, -b / g, s * (k / g), r, 2 * a / g, mult, {{0}}};
        abn A = abn_of(x.A), B = abn_of(x.B), R = abn_of(x.r), D = abn_of(x.D), t;
        if (abn_sqrt(&t, &R) != ABN_OK || abn_mul(&t, &t, &B) != ABN_OK || abn_add(&t, &t, &A) != ABN_OK ||
            abn_div(&x.v, &t, &D) != ABN_OK)
            x.v = abn_zero();
        add_root(rs, n, x);
    }
    return true;
}

static void put_root(out_t *w, const iroot *x)
{
    if (!x->irr) {
        put_int(w, x->p);
        if (x->q != 1) puts_(w, DIVIDE), put_int(w, x->q);
    } else {
        int64_t b = x->B < 0 ? -x->B : x->B;
        bool paren = x->A != 0 && x->D != 1;
        if (paren) puts_(w, "(");
        if (x->A != 0) put_int(w, x->A);
        if (x->B < 0) puts_(w, "-");
        else if (x->A != 0) puts_(w, "+");
        if (b != 1) put_int(w, b), puts_(w, TIMES);
        puts_(w, "SQRT("), put_int(w, x->r), puts_(w, ")");
        if (paren) puts_(w, ")");
        if (x->D != 1) puts_(w, DIVIDE), put_int(w, x->D);
    }
    if (x->mult > 1) puts_(w, " (MULT "), put_int(w, x->mult), puts_(w, ")");
}

/* ---- the polynomial as typed (rule 2, amended b1a8a66 on #5040) ----
   The coefficients come from the parsed tree, not from 002's canonical form: each node's polynomial in var,
   SOLVE_DEG + 1 exact rationals in the arena, sums and products multiplied out. So the refusals apply to
   what was typed: var in a denominator, inside a function, or to a power not a whole number >= 0 is
   NOT_POLYNOMIAL even where it would cancel (X^2÷X), and no root is printed where the typed equation is
   undefined. Left chains of + - × ÷ ^ are walked in a loop, as canon walks them; a degree past 12 at any
   step is NO_ROOM. */

static bool raw_has_var(ctx *c, ref r, int var);

/* Whether the raw subtree r holds var: left spines in a loop, right sides through the depth guard. */
static bool raw_has_var(ctx *c, ref r, int var)
{
    for (;;) {
        const node *x = nd(c, r);
        switch (x->kind) {
        case K_VAR: return x->id == var;
        case K_NEG:
        case K_GROUP:
        case K_RFN: r = x->a; continue;
        case K_BADD:
        case K_BSUB:
        case K_BMUL:
        case K_BDIV:
        case K_BPOW:
        case K_REQ:
        case K_ROFN:
            if (x->b) {
                bool h;
                if (!enter(c)) return false;
                h = raw_has_var(c, x->b, var);
                c->depth--;
                if (h) return true;
            }
            r = x->a;
            continue;
        default: return false;
        }
    }
}

static int pdeg(const rat *p)
{
    int d = SOLVE_DEG;
    while (d > 0 && p[d].p == 0) d--;
    return d;
}

static void pzero(rat *p)
{
    for (int k = 0; k <= SOLVE_DEG; k++) rat_int(&p[k], 0);
}

/* out *= t (both polynomials): NO_ROOM past degree 12 or 2^62. tmp is room for one more polynomial. */
static void pmul(ctx *c, rat *out, const rat *t, rat *tmp)
{
    int da = pdeg(out), db = pdeg(t);
    if (da + db > SOLVE_DEG) {
        c->st = CASIM_NO_ROOM;
        return;
    }
    pzero(tmp);
    for (int i = 0; i <= da; i++)
        for (int j = 0; j <= db; j++) {
            rat m;
            if (!rat_mul(&m, &out[i], &t[j]) || !rat_add(&tmp[i + j], &tmp[i + j], &m)) {
                c->st = CASIM_NO_ROOM;
                return;
            }
        }
    memcpy(out, tmp, (SOLVE_DEG + 1) * sizeof(rat));
}

static void pw(ctx *c, ref r, int var, rat *out);

/* A chain of + and - (or of × and ÷) from its leftmost operand up, in a loop. */
static void pw_chain(ctx *c, ref r, int var, rat *out, bool sum)
{
    int m = 0;
    ref y = r, sl;
    rat *t, *tmp;
    while (sum ? (nd(c, y)->kind == K_BADD || nd(c, y)->kind == K_BSUB)
               : (nd(c, y)->kind == K_BMUL || nd(c, y)->kind == K_BDIV))
        m++, y = nd(c, y)->a;
    if (!(sl = grab(c, (size_t)m * sizeof(ref)))) return;
    y = r;
    for (int k = m - 1; k >= 0; k--) list(c, sl)[k] = y, y = nd(c, y)->a;
    {
        /* one buffer held across the recursion (the right operand's); a product's scratch is taken only
           after it returns, so a nest that alternates × and + costs one polynomial a level */
        ref tr = grab(c, (SOLVE_DEG + 1) * sizeof(rat));
        if (!tr) return;
        t = (rat *)(void *)nd(c, tr);
    }
    pw(c, y, var, out);
    for (int k = 0; k < m && c->st == CASIM_OK; k++) {
        const node *z = nd(c, list(c, sl)[k]);
        if (z->kind == K_BDIV && raw_has_var(c, z->b, var)) {
            c->st = CASIM_NOT_POLYNOMIAL;                   /* var in a denominator, as typed */
            return;
        }
        pw(c, z->b, var, t);
        if (c->st != CASIM_OK) return;
        if (z->kind == K_BADD || z->kind == K_BSUB) {
            for (int i = 0; i <= SOLVE_DEG; i++) {
                rat v = t[i];
                if (z->kind == K_BSUB) v.p = -v.p;
                if (!rat_add(&out[i], &out[i], &v)) c->st = CASIM_NO_ROOM;
            }
        } else if (z->kind == K_BMUL) {
            size_t m2 = c->used;
            ref sr = grab(c, (SOLVE_DEG + 1) * sizeof(rat));
            if (!sr) return;
            tmp = (rat *)(void *)nd(c, sr);
            pmul(c, out, t, tmp);
            if (c->st == CASIM_OK) c->used = m2;
        } else {
            /* ÷ a constant: not 0 (1÷0 is no exact rational) */
            rat inv;
            if (t[0].p == 0 || !rat_make(&inv, t[0].q, t[0].p)) {
                c->st = t[0].p == 0 ? CASIM_NOT_NUMERIC : CASIM_NO_ROOM;
                return;
            }
            for (int i = 0; i <= SOLVE_DEG; i++)
                if (!rat_mul(&out[i], &out[i], &inv)) c->st = CASIM_NO_ROOM;
        }
    }
}

/* A chain of ^, its base raised to each exponent in turn (left to right as parsed; STU's right chains
   arrive as one exponent that is itself a chain). An exponent must be a constant; on var's polynomial a
   whole number >= 0 (else NOT_POLYNOMIAL), on a constant an integer (else NOT_NUMERIC). */
static void pw_power(ctx *c, ref r, int var, rat *out)
{
    int m = 0;
    ref y = r, sl, tr;
    rat *t, *tmp, *base;
    while (nd(c, y)->kind == K_BPOW) m++, y = nd(c, y)->a;
    if (!(sl = grab(c, (size_t)m * sizeof(ref))) || !(tr = grab(c, (SOLVE_DEG + 1) * sizeof(rat)))) return;
    t = (rat *)(void *)nd(c, tr);                   /* the exponent's, held across its walk */
    y = r;
    for (int k = m - 1; k >= 0; k--) list(c, sl)[k] = nd(c, y)->b, y = nd(c, y)->a;
    pw(c, y, var, out);
    for (int k = 0; k < m && c->st == CASIM_OK; k++) {
        ref e = list(c, sl)[k];
        int d = pdeg(out);
        long w;
        if (raw_has_var(c, e, var)) {
            c->st = CASIM_NOT_POLYNOMIAL;                   /* 2^X, X^X */
            return;
        }
        pw(c, e, var, t);
        if (c->st == CASIM_NOT_NUMERIC && d >= 1) c->st = CASIM_NOT_POLYNOMIAL;     /* X^SQRT(2) */
        if (c->st != CASIM_OK) return;
        if (!rat_whole(&t[0], &w)) {
            c->st = d >= 1 ? CASIM_NOT_POLYNOMIAL : CASIM_NOT_NUMERIC;            /* X^0.5, 2^0.5 */
            return;
        }
        if (d >= 1) {
            if (w < 0) {
                c->st = CASIM_NOT_POLYNOMIAL;               /* X^-1 */
                return;
            }
            if ((long)d * w > SOLVE_DEG) {
                c->st = CASIM_NO_ROOM;
                return;
            }
            {
                /* the base and the product's scratch, off the stack (pw recurses) and only for now */
                size_t m2 = c->used;
                ref br = grab(c, 2 * (SOLVE_DEG + 1) * sizeof(rat));
                if (!br) return;
                base = (rat *)(void *)nd(c, br), tmp = base + SOLVE_DEG + 1;
                memcpy(base, out, (SOLVE_DEG + 1) * sizeof(rat));
                pzero(out), rat_int(&out[0], 1);
                for (long i = 0; i < w && c->st == CASIM_OK; i++) pmul(c, out, base, tmp);
                if (c->st == CASIM_OK) c->used = m2;
            }
        } else {
            rat v;
            if (out[0].p == 0 && w < 0) {
                c->st = CASIM_NOT_NUMERIC;                   /* 0^-1 */
                return;
            }
            if (!rat_pow(&v, &out[0], w)) {
                c->st = CASIM_NO_ROOM;
                return;
            }
            out[0] = v;
        }
    }
}

/* r's polynomial in var, into out (SOLVE_DEG + 1 rationals); the arena r's walk used is given back. */
static void pw(ctx *c, ref r, int var, rat *out)
{
    const node *x;
    size_t mark = c->used;
    pzero(out);
    if (c->st != CASIM_OK || !enter(c)) return;
    x = nd(c, r);
    switch (x->kind) {
    case K_LIT:
        if (!literal(&out[0], lit_text(c, x), x->n)) c->st = CASIM_NO_ROOM;       /* past 2^62 */
        break;
    case K_NUM: out[0] = *val(c, r); break;
    case K_VAR:
        if (x->id == var) rat_int(&out[1], 1);
        else c->st = CASIM_NOT_NUMERIC;                     /* another letter */
        break;
    case K_GROUP: pw(c, x->a, var, out); break;
    case K_NEG: {
        int k = 0;
        ref y = r;
        while (nd(c, y)->kind == K_NEG) k++, y = nd(c, y)->a;
        pw(c, y, var, out);
        if (k % 2)
            for (int i = 0; i <= SOLVE_DEG; i++) out[i].p = -out[i].p;
        break;
    }
    case K_BADD:
    case K_BSUB: pw_chain(c, r, var, out, true); break;
    case K_BMUL:
    case K_BDIV: pw_chain(c, r, var, out, false); break;
    case K_BPOW: pw_power(c, r, var, out); break;
    default:
        /* a function, π, a name, a draw: of var not a polynomial, else not a rational number */
        c->st = raw_has_var(c, r, var) ? CASIM_NOT_POLYNOMIAL : CASIM_NOT_NUMERIC;
        break;
    }
    c->depth--;
    if (c->st == CASIM_OK) c->used = mark;
}

/* co cleared to a primitive integer polynomial, its leading coefficient positive; NO_ROOM past the bounds. */
static bool clear(ctx *c, const rat *co, ipoly *f)
{
    int64_t l = 1, g = 0, gp = 0;
    f->n = -1;
    for (int k = 0; k <= SOLVE_DEG; k++)
        if (co[k].p != 0) {
            int64_t q = co[k].q / gcd64(l, co[k].q);
            if (!mul64(&l, l, q)) return c->st = CASIM_NO_ROOM, false;
            gp = gcd64(gp, co[k].p);
            f->n = k;
        }
    /* the numerators' gcd out before scaling: (p/gp)·(l/q) (review: 4000000000000000001/3 X^2 + ...÷2) */
    for (int k = 0; k <= SOLVE_DEG; k++) {
        f->a[k] = 0;
        if (k <= f->n && co[k].p != 0 && !mul64(&f->a[k], co[k].p / gp, l / co[k].q)) return c->st = CASIM_NO_ROOM, false;
        g = gcd64(g, f->a[k]);
    }
    if (f->n < 0) return true;
    if (f->a[f->n] < 0) g = -g;
    for (int k = 0; k <= f->n; k++) {
        f->a[k] /= g;
        if (f->a[k] > SOLVE_COEF || f->a[k] < -SOLVE_COEF) return c->st = CASIM_NO_ROOM, false;
    }
    return true;
}

/* The answer's text (abacus #4841): its status first, then each root once, ascending, "; " between. */
static __attribute__((noinline)) void put_solution(ctx *c, out_t *w, ipoly *f, int var, solve_ws *s, int32_t *dq)
{
    char name[2] = {(char)('A' + var), '\0'};
    iroot *rs;
    int nr = 0, zero = 0;
    ref room = grab(c, SOLVE_DEG * sizeof(iroot));
    if (!room) return;
    rs = (iroot *)(void *)nd(c, room);
    if (f->n < 0) {
        puts_(w, "EVERY "), puts_(w, name);
        return;
    }
    if (f->n == 0) {
        puts_(w, "NO SOLUTION");
        return;
    }
    while (f->n >= 1 && f->a[0] == 0) {                 /* the root 0, and its multiplicity */
        memmove(f->a, f->a + 1, (size_t)f->n * sizeof f->a[0]);
        f->n--, zero++;
    }
    s->nfac = 0;
    factors(c, f, s, dq);
    if (zero) rational_root(rs, &nr, 0, 1, zero);
    for (int i = 0; i < s->nfac; i++) {
        const ifac *x = &s->fac[i];
        if (x->deg == 1) rational_root(rs, &nr, x->b, x->a, x->mult);
        else if (!quad_roots(rs, &nr, x->a, x->b, x->c, x->mult)) f->n += 2 * x->mult;     /* unsolved: left */
    }
    /* ascending, by value in decimal128 (34 digits) */
    for (int i = 1; i < nr; i++) {
        iroot t = rs[i];
        int j = i;
        while (j > 0 && abn_cmp(&rs[j - 1].v, &t.v) > 0) rs[j] = rs[j - 1], j--;
        rs[j] = t;
    }
    if (f->n >= 3) {
        puts_(w, "PARTIAL, DEGREE "), put_int(w, f->n), puts_(w, " LEFT");
    } else puts_(w, nr ? "SOLVED" : "NO REAL ROOTS");
    for (int i = 0; i < nr; i++) {
        puts_(w, i ? "; " : ": "), puts_(w, name), puts_(w, "=");
        put_root(w, &rs[i]);
    }
}

static __attribute__((noinline)) casim_status do_solve(const char *eqn, int var, int angle, char *out, size_t cap,
                                                        void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE, ws, cr, dq;
    out_t w = {out, cap < SLOT ? cap : SLOT, 0, false};
    ipoly f;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    size_t mark;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    cr = grab(&c, 2 * (SOLVE_DEG + 1) * sizeof(rat));
    mark = c.used;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st != CASIM_OK || cap == 0) return finish(&c, n, out, cap < SLOT ? cap : SLOT);
    {
        /* L - R, or the expression, as typed */
        rat *co = (rat *)(void *)nd(&c, cr), *r2 = co + SOLVE_DEG + 1;
        if (nd(&c, n)->kind == K_REQ) {
            pw(&c, nd(&c, n)->a, var, co);
            pw(&c, nd(&c, n)->b, var, r2);
            for (int k = 0; k <= SOLVE_DEG && c.st == CASIM_OK; k++) {
                r2[k].p = -r2[k].p;
                if (!rat_add(&co[k], &co[k], &r2[k])) c.st = CASIM_NO_ROOM;
            }
        } else pw(&c, n, var, co);
    }
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, cap < SLOT ? cap : SLOT);
    /* the tree is done with: its arena goes to the search */
    c.used = mark;
    ws = grab(&c, sizeof(solve_ws));
    dq = grab(&c, SOLVE_DIVS * sizeof(int32_t));
    if (c.st == CASIM_OK && clear(&c, (rat *)(void *)nd(&c, cr), &f))
        put_solution(&c, &w, &f, var, (solve_ws *)(void *)nd(&c, ws), (int32_t *)(void *)nd(&c, dq));
    if (w.full && c.st == CASIM_OK) c.st = CASIM_TOO_LONG;
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, cap < SLOT ? cap : SLOT);
    return CASIM_OK;
}

/* ---- the API: CAS 005 ---- */

casim_status casim_solve(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                         size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_solve(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- integrate (unit CAS 006) ----
   The table's forms, recognised on the typed tree (rule 6: as typed, as CAS 005's rule 2): a sum term by term; in
   a product, the factors free of var are one constant, and what holds var must be one form of the table, or
   polynomials multiplied out. A linear argument's a and b come from CAS 005's walk (pw): rational numbers.
   Every answer is differentiated back (deriv, in the angle mode) and F' - f, with two check-only rewrites
   (abacus #5124), must expand to exactly 0 before it prints; otherwise NOT_INTEGRABLE. */

static ref integ_term(ctx *c, ref r, int var, const rat **poly);

static ref canon_const(ctx *c, ref r, int var);

static void not_integrable(ctx *c)
{
    if (c->st == CASIM_OK) c->st = CASIM_NOT_INTEGRABLE;
}

/* r's polynomial in var (pw), into a fresh arena list; false (and no status) when r is not one. */
static bool poly_of(ctx *c, ref r, int var, rat **out)
{
    ref pr = grab(c, (SOLVE_DEG + 1) * sizeof(rat));
    if (!pr) return false;
    *out = (rat *)(void *)nd(c, pr);
    pw(c, r, var, *out);
    if (c->st == CASIM_NO_ROOM && c->starved) return false;        /* the arena: stays NO_ROOM */
    if (c->st == CASIM_NOT_POLYNOMIAL || c->st == CASIM_NOT_NUMERIC || c->st == CASIM_NO_ROOM) {
        c->st = CASIM_OK;
        return false;
    }
    return c->st == CASIM_OK;
}

/* The canonical sum of p's terms, c_k X^k: what a polynomial term is checked against (flat, so F' - p cancels
   without expanding a typed product). */
static ref poly_tree(ctx *c, const rat *p, int var)
{
    ref acc = small(c, C_0), x = leaf(c, K_VAR, var);
    for (int k = SOLVE_DEG; k >= 0; k--)
        if (p[k].p != 0) {
            rat e;
            rat_int(&e, k);
            acc = add2(c, acc, mul2(c, num(c, &p[k]), power(c, x, num(c, &e))));
        }
    return acc;
}

/* ∫ of a polynomial, term by term: c_k X^(k+1) / (k+1). */
static ref integ_poly(ctx *c, const rat *p, int var)
{
    ref acc = small(c, C_0), x = leaf(c, K_VAR, var);
    if (pdeg(p) >= SOLVE_DEG) return not_integrable(c), SPARE;
    for (int k = 0; k <= SOLVE_DEG; k++)
        if (p[k].p != 0) {
            rat e, ck;
            rat_int(&e, k + 1);
            if (!rat_make(&ck, 1, k + 1) || !rat_mul(&ck, &ck, &p[k])) return not_integrable(c), SPARE;
            acc = add2(c, acc, mul2(c, num(c, &ck), power(c, x, num(c, &e))));
        }
    return acc;
}

static bool linear(ctx *c, ref u, int var, rat **lin);

/* Whether f, as typed, is a power of X alone, X^e with e rational, into *e (rule 6a, bd37a0b): X; X^n; SQRT(X);
   SQ(u), INV(u) and u^n (n whole) of such a u; products and quotients of them. *neg_seen is set when any
   factor's own exponent is negative (an X in a denominator, or X^-k): combining may then hide X = 0. A
   fractional power of anything but X itself is not one ((X^2)^(1/2) is ABS(X)). */
static bool mono_pow(ctx *c, ref f, int var, rat *e, bool *neg_seen)
{
    const node *x = nd(c, f);
    bool ok = false;
    rat *ex;
    long w;
    if (!enter(c)) return false;
    while (x->kind == K_GROUP) f = x->a, x = nd(c, f);
    if (x->kind == K_VAR && x->id == var) rat_int(e, 1), ok = true;
    else if (x->kind == K_RFN && x->id == F_SQRT) {
        const node *u = nd(c, x->a);
        while (u->kind == K_GROUP) u = nd(c, u->a);
        if (u->kind == K_VAR && u->id == var) rat_make(e, 1, 2), ok = true;
    } else if (x->kind == K_RFN && (x->id == F_SQ || x->id == F_INV)) {
        if ((ok = mono_pow(c, x->a, var, e, neg_seen))) {
            rat k;
            rat_int(&k, x->id == F_SQ ? 2 : -1);
            ok = rat_mul(e, e, &k);
            if (x->id == F_INV) *neg_seen = true;
        }
    } else if (x->kind == K_BPOW && !raw_has_var(c, x->b, var) && poly_of(c, x->b, var, &ex) && pdeg(ex) == 0) {
        const node *b = nd(c, x->a);
        rat n = ex[0];
        while (b->kind == K_GROUP) b = nd(c, b->a);
        if (b->kind == K_VAR && b->id == var) *e = n, ok = true;
        else if (rat_whole(&n, &w) && (ok = mono_pow(c, x->a, var, e, neg_seen))) ok = rat_mul(e, e, &n);
        if (ok && n.p < 0) *neg_seen = true;
    } else if (x->kind == K_BMUL || x->kind == K_BDIV) {
        rat l, r;
        if ((ok = mono_pow(c, x->a, var, &l, neg_seen) && mono_pow(c, x->b, var, &r, neg_seen))) {
            if (x->kind == K_BDIV) r.p = -r.p, *neg_seen = true;
            ok = rat_add(e, &l, &r);
        }
    }
    c->depth--;
    return ok && c->st == CASIM_OK;
}

/* ∫ of X^R times a polynomial: c_k X^(k+R+1) / (k+R+1), and c_k LN(ABS(X)) where k + R is -1. */
static ref integ_poly_r(ctx *c, const rat *p, const rat *r, int var)
{
    ref acc = small(c, C_0), x = leaf(c, K_VAR, var);
    for (int k = 0; k <= SOLVE_DEG; k++)
        if (p[k].p != 0) {
            rat e, ck, one = {1, 1};
            rat_int(&e, k);
            if (!rat_add(&e, &e, r)) return not_integrable(c), SPARE;
            if (rat_is(&e, -1, 1)) {
                acc = add2(c, acc, mul2(c, num(c, &p[k]), fn(c, F_LN, fn(c, F_ABS, x))));
                continue;
            }
            if (!rat_add(&e, &e, &one) || !rat_make(&ck, e.q, e.p) || !rat_mul(&ck, &ck, &p[k]))
                return not_integrable(c), SPARE;
            acc = add2(c, acc, mul2(c, num(c, &ck), power(c, x, num(c, &e))));
        }
    return acc;
}

/* ∫ (u)^n, u = aX + b linear (a, b rational, a != 0): (u)^(n+1)/((n+1) a), or LN(ABS(u))/a for n = -1. */
static ref integ_linpow(ctx *c, ref u, const rat *lin, const rat *n)
{
    rat np1, k;
    ref uc = canon(c, u);
    if (rat_is(n, -1, 1)) return mul2(c, fn(c, F_LN, fn(c, F_ABS, uc)), power(c, num(c, &lin[1]), small(c, C_M1)));
    if (!rat_add(&np1, n, &(rat){1, 1}) || !rat_mul(&k, &np1, &lin[1])) return not_integrable(c), SPARE;
    return mul2(c, power(c, uc, num(c, &np1)), power(c, num(c, &k), small(c, C_M1)));
}

/* u's linear coefficients (a in lin[1], b in lin[0]); false unless u is aX + b with a != 0, as typed. */
static bool linear(ctx *c, ref u, int var, rat **lin)
{
    return poly_of(c, u, var, lin) && pdeg(*lin) == 1 && (*lin)[1].p != 0;
}

/* ∫ of one factor holding var, to the power s (+1, or -1 for a divisor): the table's forms. */
static ref integ(ctx *c, ref r, int var);

static ref integ_factor(ctx *c, ref f, int s, int var)
{
    const node *x = nd(c, f);
    rat *lin, *p, n;
    int negs = 0;
    /* a minus through, and parentheses (the review: -COS(2×X)÷2 was refused) */
    while (x->kind == K_GROUP || x->kind == K_NEG) {
        if (x->kind == K_NEG) negs++;
        f = x->a, x = nd(c, f);
    }
    if (negs % 2) {
        ref r = integ_factor(c, f, s, var);
        return c->st == CASIM_OK ? neg(c, r) : SPARE;
    }
    /* a constant times a sum (linearity): the sum term by term, each checked (the review) */
    if (s > 0 && (x->kind == K_BADD || x->kind == K_BSUB)) return integ(c, f, var);
    /* a product inside a minus or parentheses: a term of its own (the review's fuzz: -(1÷(2X+1)^2)÷A) */
    if (s > 0 && (x->kind == K_BMUL || x->kind == K_BDIV)) {
        const rat *inner = NULL;
        return integ_term(c, f, var, &inner);
    }
    if (x->kind == K_RFN) {
        int id = x->id;
        ref u = x->a, uc, a_inv, r;
        if (id == F_SQRT || id == F_SQ || id == F_INV) {
            rat_int(&n, id == F_SQ ? 2 : -1);
            if (id == F_SQRT) rat_make(&n, 1, 2);
            if (s < 0) n.p = -n.p;
            if (linear(c, u, var, &lin)) return integ_linpow(c, u, lin, &n);
            if (id == F_SQ && s > 0 && poly_of(c, u, var, &p)) {
                /* SQ of a polynomial: squared out */
                ref tr = grab(c, 2 * (SOLVE_DEG + 1) * sizeof(rat));
                rat *q;
                if (!tr) return SPARE;
                q = (rat *)(void *)nd(c, tr);
                memcpy(q, p, (SOLVE_DEG + 1) * sizeof(rat));
                pmul(c, q, p, q + SOLVE_DEG + 1);
                if (c->st != CASIM_OK) return c->st = CASIM_NOT_INTEGRABLE, SPARE;
                return integ_poly(c, q, var);
            }
            return not_integrable(c), SPARE;
        }
        if (s < 0 || !linear(c, u, var, &lin)) return not_integrable(c), SPARE;
        uc = canon(c, u);
        a_inv = power(c, num(c, &lin[1]), small(c, C_M1));
        switch (id) {
        case F_EXP: r = fn(c, F_EXP, uc); break;
        case F_SINH: r = fn(c, F_COSH, uc); break;
        case F_COSH: r = fn(c, F_SINH, uc); break;
        case F_ALOG: r = mul2(c, fn(c, F_ALOG, uc), power(c, fn(c, F_LN, small(c, C_10)), small(c, C_M1))); break;
        case F_SIN: r = neg(c, mul2(c, fn(c, F_COS, uc), angle_factor(c, true))); break;
        case F_COS: r = mul2(c, fn(c, F_SIN, uc), angle_factor(c, true)); break;
        case F_TAN: r = neg(c, mul2(c, fn(c, F_LN, fn(c, F_ABS, fn(c, F_COS, uc))), angle_factor(c, true))); break;
        default: return not_integrable(c), SPARE;
        }
        return mul2(c, r, a_inv);
    }
    if (x->kind == K_BPOW && nd(c, x->a)->kind != K_BPOW) {
        /* base^e, e a constant: (aX+b)^n, or a polynomial to a whole power */
        rat *e;
        if (raw_has_var(c, x->b, var) || !poly_of(c, x->b, var, &e) || pdeg(e) != 0) return not_integrable(c), SPARE;
        n = e[0];
        if (s < 0) n.p = -n.p;
        if (linear(c, x->a, var, &lin)) return integ_linpow(c, x->a, lin, &n);
    }
    /* a polynomial factor (×), or 1/(aX+b) (÷) */
    if (s < 0) {
        rat m1 = {-1, 1};
        if (linear(c, f, var, &lin)) return integ_linpow(c, f, lin, &m1);
        return not_integrable(c), SPARE;
    }
    if (poly_of(c, f, var, &p)) {
        if (pdeg(p) == 1) {
            rat one = {1, 1};
            return integ_linpow(c, f, p, &one);
        }
        return integ_poly(c, p, var);
    }
    return not_integrable(c), SPARE;
}

/* A product term's factors free of var, as one canonical constant (1 when there are none). */
static ref canon_const(ctx *c, ref r, int var)
{
    ref k = small(c, C_1), z = r;
    while (nd(c, z)->kind == K_BMUL || nd(c, z)->kind == K_BDIV) {
        ref o = nd(c, z)->b;
        if (!raw_has_var(c, o, var)) {
            ref oc = canon(c, o);
            k = mul2(c, k, nd(c, z)->kind == K_BMUL ? oc : power(c, oc, small(c, C_M1)));
        }
        z = nd(c, z)->a;
    }
    if (!raw_has_var(c, z, var)) k = mul2(c, k, canon(c, z));
    return k;
}

/* ∫ of one term (no + or - at its top): its constant factors times the ∫ of the rest. *poly is set to the
   exact polynomial (as typed, without the constant) when the term was integrated through one. */
static ref integ_term(ctx *c, ref r, int var, const rat **poly)
{
    int m = 0, nv = 0, last = -1, sl_s = 0;
    ref y = r, sl, k = small(c, C_1);
    if (c->st != CASIM_OK) return SPARE;
    if (!raw_has_var(c, r, var)) return mul2(c, canon(c, r), leaf(c, K_VAR, var));      /* ∫ k = k X */
    while (nd(c, y)->kind == K_BMUL || nd(c, y)->kind == K_BDIV) m++, y = nd(c, y)->a;
    if (!(sl = grab(c, (size_t)(2 * m + 2) * sizeof(ref)))) return SPARE;
    /* the operands, each with its sign of exponent: the leftmost, then each right side */
    list(c, sl)[0] = y, list(c, sl)[1] = 1;
    {
        ref z = r;
        for (int i = m; i >= 1; i--) {
            list(c, sl)[2 * i] = nd(c, z)->b, list(c, sl)[2 * i + 1] = nd(c, z)->kind == K_BDIV ? (ref)-1 : 1;
            z = nd(c, z)->a;
        }
    }
    for (int i = 0; i <= m; i++) {
        ref o = list(c, sl)[2 * i];
        int sgn = list(c, sl)[2 * i + 1] == 1 ? 1 : -1;
        if (raw_has_var(c, o, var)) {
            nv++, last = i, sl_s = sgn;
        } else {
            ref oc = canon(c, o);
            k = mul2(c, k, sgn > 0 ? oc : power(c, oc, small(c, C_M1)));
        }
    }
    {
        /* powers of X and polynomials multiplied out: P(X) X^R (rule 2's polynomial times a form; rule 6a's
           powers combining). A single factor that is neither goes to the table's forms. */
        rat *acc = NULL, *f, *tmp, R;
        bool neg_seen = false;
        ref tr = grab(c, (SOLVE_DEG + 1) * sizeof(rat));
        if (!tr) return SPARE;
        tmp = (rat *)(void *)nd(c, tr);
        rat_int(&R, 0);
        if (nv == 1) {
            rat e;
            bool ns = false;
            if (!mono_pow(c, list(c, sl)[2 * last], var, &e, &ns)) {
                if (c->st != CASIM_OK) return SPARE;
                return mul2(c, k, integ_factor(c, list(c, sl)[2 * last], sl_s, var));
            }
        }
        for (int i = 0; i <= m; i++) {
            ref o = list(c, sl)[2 * i];
            bool den = list(c, sl)[2 * i + 1] != 1;
            rat e;
            if (!raw_has_var(c, o, var)) continue;
            if (mono_pow(c, o, var, &e, &neg_seen)) {
                if (den) e.p = -e.p, neg_seen = true;
                if (!rat_add(&R, &R, &e)) return not_integrable(c), SPARE;
                continue;
            }
            if (c->st != CASIM_OK) return SPARE;
            if (den || !poly_of(c, o, var, &f)) return not_integrable(c), SPARE;
            if (!acc) acc = f;
            else {
                pmul(c, acc, f, tmp);
                if (c->st != CASIM_OK) return c->st = CASIM_NOT_INTEGRABLE, SPARE;
            }
        }
        if (!acc) {
            ref ar = grab(c, (SOLVE_DEG + 1) * sizeof(rat));
            if (!ar) return SPARE;
            acc = (rat *)(void *)nd(c, ar);
            pzero(acc), rat_int(&acc[0], 1);
        }
        if (neg_seen) {
            /* as typed: refused when combining would hide X = 0 (every term's exponent >= 0: X^2÷X) */
            bool hides = true;
            for (int i = 0; i <= SOLVE_DEG; i++) {
                rat ei;
                rat_int(&ei, i);
                if (acc[i].p != 0 && rat_add(&ei, &ei, &R) && ei.p < 0) hides = false;
            }
            if (hides) return not_integrable(c), SPARE;
        }
        if (R.p != 0) return mul2(c, k, integ_poly_r(c, acc, &R, var));
        *poly = acc;
        return mul2(c, k, integ_poly(c, acc, var));
    }
}

static bool closes(ctx *c, ref F, ref f);

/* ∫ of one term, checked: F' must be the term exactly (a polynomial term against its flat form). The term's
   garbage (its walks, its check) is given back: what is kept is compacted down to where the term began. */
static ref integ_checked(ctx *c, ref t, int var, int sign)
{
    size_t mark = c->used;
    const rat *poly = NULL;
    ref F = integ_term(c, t, var, &poly), f;
    if (c->st != CASIM_OK) return SPARE;
    f = poly ? mul2(c, canon_const(c, t, var), poly_tree(c, poly, var)) : canon(c, t);
    if (sign < 0) F = neg(c, F), f = neg(c, f);                 /* the sign checked too (the review) */
    if (!closes(c, F, f)) {
        if (c->st == CASIM_OK) c->st = CASIM_NOT_INTEGRABLE;   /* never an unchecked answer */
        return SPARE;
    }
    return compact(c, mark, F);
}

/* ∫ of r: a chain of + and - term by term, a minus through. */
static ref integ(ctx *c, ref r, int var)
{
    int m = 0, sign = 1;
    ref y = r, acc;
    while (nd(c, y)->kind == K_GROUP || nd(c, y)->kind == K_NEG) {
        if (nd(c, y)->kind == K_NEG) sign = -sign;
        y = nd(c, y)->a;
    }
    r = y;
    if (nd(c, r)->kind != K_BADD && nd(c, r)->kind != K_BSUB) return integ_checked(c, r, var, sign);
    while (nd(c, y)->kind == K_BADD || nd(c, y)->kind == K_BSUB) m++, y = nd(c, y)->a;
    acc = integ_checked(c, y, var, sign);
    for (int i = m - 1; i >= 0 && c->st == CASIM_OK; i--) {
        ref z = r, t;
        for (int j = 0; j < i; j++) z = nd(c, z)->a;
        t = integ_checked(c, nd(c, z)->b, var, nd(c, z)->kind == K_BSUB ? -sign : sign);
        acc = add2(c, acc, t);
    }
    return acc;
}

/* The check's two rewrites (abacus #5124), never in an answer: SGN(u)×ABS(u)^-1 is u^-1 (both undefined at
   u = 0), and TAN(u) is SIN(u)×COS(u)^-1 (undefined exactly where COS is 0). */
static ref check_rewrite(ctx *c, ref r)
{
    const node *x;
    ref v = r;
    if (c->st != CASIM_OK || !enter(c)) return SPARE;
    x = nd(c, r);
    switch (x->kind) {
    case K_VAR:                         /* CAS 009: with sub_val set, the walk substitutes (F at a bound) and rewrites nothing */
        if (c->sub_val && x->id == c->sub_var) v = c->sub_val;
        break;
    case K_FN: {
        ref a = check_rewrite(c, x->a);
        x = nd(c, r);
        if (x->id == F_TAN && !c->sub_val) v = mul2(c, fn(c, F_SIN, a), power(c, fn(c, F_COS, a), small(c, C_M1)));
        else v = fn(c, x->id, a);
        break;
    }
    case K_POW: {
        ref b = check_rewrite(c, x->a), e = check_rewrite(c, nd(c, r)->b);
        v = power(c, b, e);
        break;
    }
    case K_ADD:
    case K_MUL: {
        int n = x->n, kind = x->kind;
        ref l = grab(c, (size_t)n * sizeof(ref));
        if (!l) break;
        for (int i = 0; i < n; i++) {
            ref t = check_rewrite(c, list(c, nd(c, r)->a)[i]);
            list(c, l)[i] = t;
        }
        if (kind == K_MUL && !c->sub_val)
            for (int i = 0; i < n; i++) {
                const node *g = nd(c, list(c, l)[i]);
                if (g->kind != K_FN || g->id != F_SGN) continue;
                for (int j = 0; j < n; j++) {
                    const node *h = nd(c, list(c, l)[j]);
                    if (j == i || h->kind != K_POW || !is_k(c, h->b, -1, 1)) continue;
                    if (nd(c, h->a)->kind != K_FN || nd(c, h->a)->id != F_ABS || cmp(c, nd(c, h->a)->a, g->a) != 0) continue;
                    list(c, l)[i] = power(c, g->a, small(c, C_M1));
                    list(c, l)[j] = small(c, C_1);
                    break;
                }
            }
        v = kind == K_ADD ? add(c, list(c, l), n) : mul(c, list(c, l), n);
        break;
    }
    default: break;
    }
    c->depth--;
    return v;
}

/* Whether F differentiates back to f exactly: F' - f, rewritten, expanded, is 0. */
static __attribute__((noinline)) bool closes(ctx *c, ref F, ref f)
{
    ref d = deriv(c, F), t;
    if (c->st != CASIM_OK) return false;
    t = add2(c, d, neg(c, f));
    t = check_rewrite(c, t);
    c->sub_var = -1;
    t = expand_fully(c, t);
    return c->st == CASIM_OK && is_k(c, t, 0, 1);
}

static __attribute__((noinline)) casim_status do_integrate(const char *eqn, int var, int angle, char *out, size_t cap,
                                                            void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE, F = SPARE;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    c.var = var;                        /* the check's deriv differentiates in it */
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;         /* an equation */
    if (c.st == CASIM_OK) F = integ(&c, n, var);         /* each term checked as it goes (integ_checked) */
    return finish(&c, F, out, cap < SLOT ? cap : SLOT);
}

/* ---- the API: CAS 006 ---- */

casim_status casim_integrate(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                             size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_integrate(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- definite integrals (unit CAS 009) ----
   F(hi) - F(lo), with F integrate's verified antiderivative, substituted by check_rewrite's walk, so CAS 008's exact
   values apply. Printed only when the interval check proves the integrand real and finite on the closed interval:
   every u linear in var under a negative power is nonzero on it, under a non-integer power positive (0 allowed
   for a positive power: the core's y^x has no real value for y < 0, odd denominators too), and TAN(u) has no pole
   on it in the angle mode. A pole within a relative 1E-20 of the interval counts as on it. */

enum { IV_OK, IV_IMPROPER, IV_LETTER };

/* a bound: its value, and its tree when it is a rational number (then u at it is exact) */
typedef struct {
    abn v;
    const rat *q;
} bound;

/* u = l1 var + l0 at a bound: exact in rationals at a rational bound (u = 0 at X = 1/3 is 0, not a rounding below
   it), else in abn */
static bool lin_at(const rat *l, const bound *x, abn *v)
{
    abn a, b;
    rat t;
    if (x->q && rat_mul(&t, &l[1], x->q) && rat_add(&t, &t, &l[0])) return rat_abn(&t, v);
    return rat_abn(&l[1], &a) && rat_abn(&l[0], &b) && abn_mul(v, &a, &x->v) == ABN_OK && abn_add(v, v, &b) == ABN_OK;
}

/* the relative edge: 1E-20 × max(1, |p|, |q|) */
static abn edge_tol(const abn *p, const abn *q)
{
    abn t, m = abn_from_int(1), ap = abn_abs(p), aq = abn_abs(q);
    if (abn_cmp(&ap, &m) > 0) m = ap;
    if (abn_cmp(&aq, &m) > 0) m = aq;
    abn_pow10(&t, -20);
    abn_mul(&t, &t, &m);
    return t;
}

/* a canonical term of u: k (a number), var, or k×var, into l[0] + l[1] var */
static bool lin_term(ctx *c, ref t, int var, rat *l)
{
    const node *x = nd(c, t);
    rat s;
    if (is_num(c, t)) return rat_add(&l[0], &l[0], val(c, t));
    if (x->kind == K_VAR && x->id == var) return rat_int(&s, 1), rat_add(&l[1], &l[1], &s);
    if (x->kind == K_MUL && x->n == 2 && is_num(c, list(c, x->a)[0]) && nd(c, list(c, x->a)[1])->kind == K_VAR &&
        nd(c, list(c, x->a)[1])->id == var)
        return rat_add(&l[1], &l[1], val(c, list(c, x->a)[0]));
    return false;
}

/* u's range over [A, B] (A <= B): umin, umax; false if u isn't l1 var + l0 with numbers (another letter: *letter).
   The canonical tree, read directly (pw walks the typed tree). CAS 006 only integrates numeric linear arguments, so a
   letter here can't come from today's integrands; the check stays for the day the scope widens. */
static bool lin_range(ctx *c, ref u, int var, const bound *A, const bound *B, abn *umin, abn *umax, bool *letter)
{
    rat lin[2];
    abn ua, ub;
    const node *x = nd(c, u);
    bool ok = true;
    *letter = false;
    for (int l = 0; l < 26; l++)
        if (l != var && contains(c, u, l)) return *letter = true, false;
    rat_int(&lin[0], 0), rat_int(&lin[1], 0);
    if (x->kind == K_ADD)
        for (int i = 0; i < x->n && ok; i++) ok = lin_term(c, list(c, x->a)[i], var, lin);
    else ok = lin_term(c, u, var, lin);
    if (!ok || lin[1].p == 0 || !lin_at(lin, A, &ua) || !lin_at(lin, B, &ub)) return false;
    if (abn_cmp(&ua, &ub) <= 0) *umin = ua, *umax = ub;
    else *umin = ub, *umax = ua;
    return true;
}

/* the power u^e (e a number): u = 0 on the interval under a negative power, u < 0 under a non-integer one */
static __attribute__((noinline)) int pow_hits(ctx *c, ref u, const rat *ex, int var, const bound *A, const bound *B)
{
    abn umin, umax, tol, z = abn_zero(), ntol;
    bool letter;
    if (!lin_range(c, u, var, A, B, &umin, &umax, &letter)) return letter ? IV_LETTER : IV_IMPROPER;
    tol = edge_tol(&umin, &umax), ntol = abn_neg(&tol);
    if (ex->p < 0 && abn_cmp(&umin, &tol) <= 0 && abn_cmp(&umax, &ntol) >= 0) return IV_IMPROPER;     /* u = 0 on it */
    if (ex->q != 1 && abn_cmp(&umin, &z) < 0) return IV_IMPROPER;                                      /* u < 0 on it */
    return IV_OK;
}

/* TAN(u)'s poles, half + k × period in u's units (RAD π/2 + kπ, DEG 90 + 180k, GRAD 100 + 200k): k = floor((umin -
   half) / period), so p0 = half + k period <= umin < p1 = p0 + period up to rounding; either within the widened
   interval is a pole on it */
static __attribute__((noinline)) int tan_hits(ctx *c, ref u, int var, const bound *A, const bound *B)
{
    abn umin, umax, tol, half, per, t, p;      /* t: the quotient, then the widened interval's ends (umin, umax reused) */
    bool letter;
    if (!lin_range(c, u, var, A, B, &umin, &umax, &letter)) return letter ? IV_LETTER : IV_IMPROPER;
    if (c->angle == CASIM_RAD) {
        abn two = abn_from_int(2);
        per = abn_pi();
        abn_div(&half, &per, &two);
    } else half = abn_from_int(c->angle == CASIM_DEG ? 90 : 100), per = abn_from_int(c->angle == CASIM_DEG ? 180 : 200);
    tol = edge_tol(&umin, &umax);
    if (abn_sub(&t, &umin, &half) != ABN_OK || abn_div(&t, &t, &per) != ABN_OK || abn_floor(&t, &t) != ABN_OK ||
        abn_mul(&p, &t, &per) != ABN_OK || abn_add(&p, &p, &half) != ABN_OK || abn_sub(&umin, &umin, &tol) != ABN_OK ||
        abn_add(&umax, &umax, &tol) != ABN_OK)
        return IV_IMPROPER;
    if (abn_cmp(&p, &umin) >= 0) return IV_IMPROPER;                    /* p0 at the low end */
    return abn_add(&p, &p, &per) != ABN_OK || abn_cmp(&p, &umax) <= 0 ? IV_IMPROPER : IV_OK;       /* p1 inside */
}

static int interval_walk(ctx *c, ref r, int var, const bound *A, const bound *B)
{
    const node *x;
    int st = IV_OK;
    if (!contains(c, r, var)) return IV_OK;             /* a constant factor (A×X^2's A) has no pole in var */
    if (!enter(c)) return IV_IMPROPER;
    x = nd(c, r);
    switch (x->kind) {
    case K_POW:
        if (is_num(c, x->b) && (val(c, x->b)->p < 0 || val(c, x->b)->q != 1)) st = pow_hits(c, x->a, val(c, x->b), var, A, B);
        if (st == IV_OK) st = interval_walk(c, x->a, var, A, B);
        break;
    case K_FN:
        if (x->id == F_TAN) st = tan_hits(c, x->a, var, A, B);
        if (st == IV_OK) st = interval_walk(c, x->a, var, A, B);
        break;
    case K_ADD:
    case K_MUL:
        for (int i = 0; i < x->n && st == IV_OK; i++) st = interval_walk(c, list(c, x->a)[i], var, A, B);
        break;
    default: break;
    }
    c->depth--;
    return st;
}

/* a bound: parsed as subst parses its replacement (its SYNTAX offset counted after eqn and what came before it, as
   if the call were eqn|lo|hi), canonical, letter-free, valued; false with c->st set */
static ref read_bound(ctx *c, const char *t, int at, abn *v)
{
    ref r;
    int cv;
    c->s2 = t, c->s = t, c->no_eq = true;
    r = parse(c);
    if (c->st == CASIM_SYNTAX) c->errpos += at;
    if (c->st != CASIM_OK) return SPARE;
    r = canon(c, r);
    if (c->st != CASIM_OK) return SPARE;
    for (int l = 0; l < 26; l++)
        if (contains(c, r, l)) return c->st = CASIM_BAD_ARG, SPARE;
    cv = const_val(c, r, v);
    if (cv != CV_OK) c->st = cv == CV_UNDEF ? CASIM_UNDEFINED : CASIM_BAD_ARG;
    return c->st == CASIM_OK ? r : SPARE;
}

static __attribute__((noinline)) casim_status do_defint(const char *eqn, int var, const char *lo, const char *hi, int angle,
                                                        char *out, size_t cap, void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE, F = SPARE, f, a, b;
    bound ba, bb;
    int chars = 0, iv;
    size_t ocap = cap < SLOT ? cap : SLOT;
    if (cap) out[0] = '\0';
    if (!eqn || !lo || !hi) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    c.var = var;
    c.sub_var = -1;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;         /* an equation */
    if (c.st == CASIM_OK) F = integ(&c, n, var);                        /* CAS 006, checked as it goes */
    if (c.st != CASIM_OK) return finish(&c, F, out, ocap);
    f = canon(&c, n);                                                   /* the integrand, for the interval check */
    for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    a = read_bound(&c, lo, chars + 1, &ba.v);
    for (const char *p = lo; *p && c.st == CASIM_OK; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    b = c.st == CASIM_OK ? read_bound(&c, hi, chars + 2, &bb.v) : SPARE;
    c.s = eqn;
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    ba.q = is_num(&c, a) ? val(&c, a) : NULL, bb.q = is_num(&c, b) ? val(&c, b) : NULL;
    iv = abn_cmp(&ba.v, &bb.v) <= 0 ? interval_walk(&c, f, var, &ba, &bb) : interval_walk(&c, f, var, &bb, &ba);
    if (iv != IV_OK) return c.st = iv == IV_LETTER ? CASIM_BAD_ARG : CASIM_IMPROPER, finish(&c, SPARE, out, ocap);
    c.sub_var = var;
    c.sub_val = b;
    b = check_rewrite(&c, F);                                           /* F(hi) */
    c.sub_val = a;
    a = check_rewrite(&c, F);                                           /* F(lo) */
    c.sub_val = 0, c.sub_var = -1;
    return finish(&c, add2(&c, b, neg(&c, a)), out, ocap);
}

casim_status casim_defint(const char *eqn, int var, const char *lo, const char *hi, int model, int angle, char *out,
                          size_t cap, void *arena, size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_defint(eqn, var, lo, hi, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- factor (unit CAS 007) ----
   The polynomial as typed (CAS 005's walk), cleared to a primitive integer f, its roots at 0 taken out. Repeated
   factors are found from the clusters' centroids and taken out by exact division; then each of them, and what is
   left, is split into irreducible factors or proven irreducible by a complete search (rule 5a, abacus #5192):
   its roots enclosed in disks, every conjugation-closed subset of size 1..n/2 and every positive d | lc tried
   with exact interval bounds. What the search cannot decide is printed whole, with CASIM_PARTIAL. */

#define FACTOR_BUDGET 80000L            /* divisor checks in one call: a count, so host and S3 agree (rule 5a said 150,000;
                                           at 4.3 us a check on the S3 that ran 1.13 s, so 80,000 keeps it under 1 s) */

typedef struct {
    ipoly p;
    int mult;
    bool proven;
} fitem;

/* The search's work, in the arena (off the stack). */
typedef struct {
    union {
        struct {                            /* the search */
            double rho[SOLVE_DEG];          /* the disks' radii */
            double az[SOLVE_DEG], bz[SOLVE_DEG];    /* |z| and |z| + rho */
            int pair[SOLVE_DEG];            /* each root's conjugate */
        };
        struct {                            /* the finder */
            double cr[SOLVE_DEG], ci[SOLVE_DEG];    /* the centroids */
            int cm[SOLVE_DEG];              /* and their multiplicities */
        };
    };
    double er[SOLVE_DEG + 1], ei[SOLVE_DEG + 1], ea[SOLVE_DEG + 1], eb[SOLVE_DEG + 1];
    int64_t co[SOLVE_DEG + 1];              /* a candidate's rounded coefficients */
} fwork;

/* h / g exactly into q (g's leading coefficient positive); false when it does not divide or a step overflows. */
static bool div_poly(const ipoly *h, const ipoly *g, ipoly *q)
{
    ipoly r = *h;
    int n = h->n, k = g->n;
    if (k < 1 || n < k || g->a[k] == 0) return false;
    q->n = n - k;
    for (int i = n; i >= k; i--) {
        int64_t qc, t;
        if (r.a[i] % g->a[k]) return false;
        qc = r.a[i] / g->a[k];
        q->a[i - k] = qc;
        for (int j = 0; j <= k; j++)
            if (__builtin_mul_overflow(qc, g->a[j], &t) || __builtin_sub_overflow(r.a[i - k + j], t, &r.a[i - k + j]))
                return false;
    }
    for (int i = 0; i < k; i++)
        if (r.a[i] != 0) return false;
    return true;
}

/* a *= b exactly; false past degree 12 or on overflow */
static __attribute__((noinline)) bool ipmul(ipoly *a, const ipoly *b)
{
    ipoly r = {a->n + b->n, {0}};
    if (r.n > SOLVE_DEG) return false;
    for (int i = 0; i <= a->n; i++)
        for (int j = 0; j <= b->n; j++) {
            int64_t t;
            if (__builtin_mul_overflow(a->a[i], b->a[j], &t) || __builtin_add_overflow(r.a[i + j], t, &r.a[i + j])) return false;
        }
    *a = r;
    return true;
}

static double cmod(double re, double im)
{
    return __builtin_sqrt(re * re + im * im);
}

/* h(z) and h'(z) by Horner (complex), and the running-error bound's magnitude sum |a_k||z|^k. */
static __attribute__((noinline)) void horner(const ipoly *h, double zr, double zi, double *pr, double *pi, double *dr, double *di, double *mag)
{
    double az = cmod(zr, zi), zk = 1;
    *pr = *pi = *dr = *di = 0, *mag = 0;
    for (int k = h->n; k >= 0; k--) {
        double t;
        t = *dr * zr - *di * zi + *pr, *di = *dr * zi + *di * zr + *pi, *dr = t;
        t = *pr * zr - *pi * zi + (double)h->a[k], *pi = *pr * zi + *pi * zr, *pr = t;
    }
    for (int k = 0; k <= h->n; k++) *mag += __builtin_fabs((double)h->a[k]) * zk, zk *= az;
}

/* The roots' inclusion disks: Weierstrass corrections W_i = h(z_i) / (lc prod_{j != i} (z_i - z_j)), radius
   n |W_i|, |h(z_i)| inflated by Horner's running-error bound 4(n+2) u sum|a_k||z_i|^k and the radius by 1.01
   for the denominator's rounding. The inclusion theorem: the disks' union holds every root, and a component of
   m disks holds m roots (D. Braess and K. P. Hadeler, "Simultaneous Inclusion of the Zeros of a Polynomial",
   Numerische Mathematik 21 (1973), 161-165; abacus checked the record but could not read the paper, and the
   constant n|W_i| is from casim's memory and the papers citing it, not verified against the text). A second
   argument for the constant, by casim's fresh-eyes reviewer (abacus #5199: keep both): the matrix
   diag(z) - W 1^T has characteristic polynomial h/lc, and its Gerschgorin disks lie within |x - z_i| <= n|W_i|.
   So pairwise disjoint disks hold one root each; false when two meet. */
static __attribute__((noinline)) bool incl_disks(const ipoly *h, const solve_ws *s, double *rho)
{
    int n = h->n;
    for (int i = 0; i < n; i++) {
        double pr, pi, dr, di, mag, denr = 1, deni = 0;
        horner(h, s->re[i], s->im[i], &pr, &pi, &dr, &di, &mag);
        for (int j = 0; j < n; j++)
            if (j != i) {
                double ar = s->re[i] - s->re[j], ai = s->im[i] - s->im[j], t = denr * ar - deni * ai;
                deni = denr * ai + deni * ar, denr = t;
            }
        if (denr == 0 && deni == 0) return false;
        rho[i] = n * (cmod(pr, pi) + 4.0 * (n + 2) * 1.2e-16 * mag) / (__builtin_fabs((double)h->a[n]) * cmod(denr, deni));
        rho[i] = rho[i] * 1.01 + 1e-300;
        if (!(rho[i] < 1e300)) return false;            /* a NaN or infinite radius proves nothing */
    }
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (!(cmod(s->re[i] - s->re[j], s->im[i] - s->im[j]) > rho[i] + rho[j])) return false;  /* NaN: meets */
    return true;
}

/* Each root's conjugate: itself when no other disk meets the mirror of its disk (the conjugate root lies in the
   mirror and in some disk: then in its own, which holds one root, so the root is real); else the one other disk
   that meets the mirror. False when that is not unique or not mutual. */
static __attribute__((noinline)) bool conj_pairs(const ipoly *h, const solve_ws *s, const double *rho, int *pair)
{
    int n = h->n;
    for (int i = 0; i < n; i++) {
        int found = -1, count = 0;
        for (int j = 0; j < n; j++)
            if (j != i && cmod(s->re[j] - s->re[i], s->im[j] + s->im[i]) <= rho[i] + rho[j]) found = j, count++;
        if (count == 0) pair[i] = i;
        else if (count == 1 && cmod(0, 2 * s->im[i]) > 2 * rho[i]) pair[i] = found;
        else return false;
    }
    for (int i = 0; i < n; i++)
        if (pair[pair[i]] != i) return false;
    return true;
}

/* e_1..e_k of the points in mask m (k its size), complex, into w->er, w->ei; with bounds (the search), the
   bound E_j = e_j(|z|+rho) - e_j(|z|) + 1E-13 e_j(|z|+rho) on each one's error into w->ea. */
static __attribute__((noinline)) int esym(const double *re, const double *im, int n, unsigned m, bool bounds, fwork *w)
{
    int k = 0;
    for (int j = 0; j <= SOLVE_DEG; j++) w->er[j] = w->ea[j] = w->eb[j] = j == 0, w->ei[j] = 0;
    for (int i = 0; i < n; i++)
        if (m >> i & 1) {
            k++;
            for (int j = k; j >= 1; j--) {
                double tr = w->er[j - 1] * re[i] - w->ei[j - 1] * im[i], ti = w->er[j - 1] * im[i] + w->ei[j - 1] * re[i];
                w->er[j] += tr, w->ei[j] += ti;
                if (bounds) w->ea[j] += w->ea[j - 1] * w->az[i], w->eb[j] += w->eb[j - 1] * w->bz[i];
            }
        }
    for (int j = 1; j <= k && bounds; j++) w->ea[j] = (w->eb[j] - w->ea[j]) + 1e-13 * w->eb[j];
    return k;
}

/* The complete search on a square-free h (degree n >= 2): 1 with a factor g of degree 1..n/2 and h / g in q
   (g the first found in ascending size: irreducible; the roots left in s are q's), 0 when none exists (proven:
   h is irreducible), -1 when it cannot be proven (disks meet, a conjugate is not unique, an interval is 0.5 or
   wider, or the budget ran out). fresh: s does not yet hold h's roots. */
static __attribute__((noinline)) int factor_search(const ipoly *h, solve_ws *s, fwork *w, int32_t *dq, long *budget, bool fresh,
                                                   ipoly *g, ipoly *q)
{
    int n = h->n, nd, unproven = 0, u0, u1;
    double a0 = __builtin_fabs((double)h->a[0]), lo, hi, dmin, dmax;
    if (fresh) aberth(h, s);
    for (int i = 0; i < n; i++)
        for (int it = 0; it < 4; it++) {
            double pr, pi, dr, di, mag, den;
            horner(h, s->re[i], s->im[i], &pr, &pi, &dr, &di, &mag);
            den = dr * dr + di * di;
            if (den == 0) break;
            s->re[i] -= (pr * dr + pi * di) / den, s->im[i] -= (pi * dr - pr * di) / den;
        }
    if (!incl_disks(h, s, w->rho) || !conj_pairs(h, s, w->rho, w->pair)) return -1;
    for (int i = 0; i < n; i++) w->az[i] = cmod(s->re[i], s->im[i]), w->bz[i] = w->az[i] + w->rho[i];
    nd = divisors_of(h->a[n], dq, SOLVE_DIVS);
    for (int i = 1; i < nd; i++) {
        int32_t t = dq[i];
        int j = i;
        for (; j > 0 && dq[j - 1] > t; j--) dq[j] = dq[j - 1];
        dq[j] = t;
    }
    for (int k = 1; k <= n / 2; k++) {
        for (unsigned m = 1; m < (1u << n); m++) {
            bool closed = true;
            if (__builtin_popcount(m) != k) continue;
            for (int i = 0; i < n && closed; i++)
                if (m >> i & 1) closed = (m >> w->pair[i] & 1) != 0;
            if (!closed) continue;
            /* the constant term: d e_k is a nonzero integer dividing a_0, so 1 <= d |e_k| <= |a_0|, and |e_k| lies
               between the products of |z| - rho and of |z| + rho: only the divisors in that window (dq ascending) */
            lo = hi = 1;
            for (int i = 0; i < n; i++)
                if (m >> i & 1) lo *= w->az[i] > w->rho[i] ? w->az[i] - w->rho[i] : 0, hi *= w->bz[i];
            dmin = (1 - 1e-9) / hi, dmax = lo > 0 ? a0 * (1 + 1e-9) / lo : 1e300;
            for (u0 = 0, u1 = nd; u0 < u1;) {
                int mid = (u0 + u1) / 2;
                if (dq[mid] < dmin) u0 = mid + 1;
                else u1 = mid;
            }
            if (u0 >= nd || dq[u0] > dmax) continue;
            esym(s->re, s->im, n, m, true, w);
            for (int u = u0; u < nd && dq[u] <= dmax; u++) {
                double d = dq[u];
                bool reject = false, wide = false;
                if (--*budget < 0) return -1;
                for (int j = 1; j <= k && !reject; j++) {
                    /* d e_j lies within d E_j of the computed value, E_j = e_j(|z|+rho) - e_j(|z|) + 1E-13 e_j(|z|+rho):
                       no integer there rejects; a half-width under 0.25 holds at most one */
                    double v = d * w->er[j], e = d * w->ea[j];
                    if (!(e < 0.25) || !round64(v, &w->co[j])) wide = true;   /* unproven (NaN too), unless another rejects */
                    else if (__builtin_fabs(v - (double)w->co[j]) > e || __builtin_fabs(d * w->ei[j]) > e) reject = true;
                }
                if (reject) continue;
                if (wide) {
                    unproven = 1;
                    continue;
                }
                g->n = k, g->a[k] = dq[u];
                for (int j = 1; j <= k; j++) g->a[k - j] = j % 2 ? -w->co[j] : w->co[j];
                if (g->a[0] != 0 && h->a[0] % g->a[0] == 0 && div_poly(h, g, q)) {
                    int t = 0;
                    for (int i = 0; i < n; i++)
                        if (!(m >> i & 1)) s->re[t] = s->re[i], s->im[t] = s->im[i], t++;
                    return 1;
                }
            }
        }
        if (unproven) return -1;
    }
    return 0;
}

/* Repeated factors (abacus: (X^3-2)^2): an m-fold root's approximations ring it, so the clusters' centroids
   (solve's, at its three radii, the tightest first) stand for the distinct roots. A subset of centroids of one
   multiplicity, with d | lc and its coefficients rounded, is a candidate, taken out of h as often as it divides.
   A finder, not a proof: what it finds is split or proven by the search like any other factor. */
static __attribute__((noinline)) void repeated_factors(ipoly *h, solve_ws *s, fwork *w, int32_t *dq, fitem *it, int *ni)
{
    bool fresh = false;
    for (int pass = 0; pass < 3 && h->n >= 2; pass++) {
        int grp[SOLVE_DEG], nc = 0, nd;
        if (!fresh) aberth(h, s), fresh = true;
        for (int i = 0; i < h->n; i++) grp[i] = -1;
        for (int i = 0; i < h->n; i++) {
            int m;
            if (grp[i] >= 0) continue;
            m = centroid(h, s, i, grp, RADIUS2[pass], &w->cr[nc], &w->ci[nc]);
            if (m > 1) w->cm[nc++] = m;
        }
        nd = divisors_of(h->a[h->n], dq, SOLVE_DIVS);
        for (unsigned msk = 1; msk < (1u << nc) && fresh; msk++) {
            int k, mult = 0;
            bool one = true;
            for (int i = 0; i < nc; i++)
                if (msk >> i & 1) one = one && (mult == 0 || w->cm[i] == mult), mult = w->cm[i];
            if (!one) continue;
            k = esym(w->cr, w->ci, nc, msk, false, w);
            for (int u = 0; u < nd; u++) {
                ipoly g, q;
                int times = 0;
                bool fits = true;
                g.n = k, g.a[k] = dq[u];
                for (int j = 1; j <= k && fits; j++) {
                    double v = (double)dq[u] * w->er[j];
                    fits = __builtin_fabs((double)dq[u] * w->ei[j]) <= 1e-3 * (1 + __builtin_fabs(v)) && round64(v, &g.a[k - j]);
                    if (j % 2) g.a[k - j] = -g.a[k - j];
                }
                if (!fits || g.a[0] == 0) continue;
                while (h->n >= k && div_poly(h, &g, &q)) *h = q, times++;
                if (times && *ni <= SOLVE_DEG) {
                    it[(*ni)++] = (fitem){g, times, false};
                    fresh = false;              /* h changed: its roots again, from the tightest radius */
                    break;
                }
            }
        }
        if (!fresh) pass = -1;
    }
}

/* The factor's canonical tree, for ordering and printing. */
static __attribute__((noinline)) ref ipoly_tree(ctx *c, const ipoly *p, int var)
{
    rat r[SOLVE_DEG + 1];
    for (int k = 0; k <= SOLVE_DEG; k++) rat_int(&r[k], 0);
    for (int k = 0; k <= p->n; k++) rat_make(&r[k], p->a[k], 1);
    return poly_tree(c, r, var);
}

/* content p/q as text: an integer or p÷q (the example's 1÷2), never a decimal */
static __attribute__((noinline)) void put_content(out_t *w, const rat *v)
{
    put_int(w, v->p);
    if (v->q != 1) puts_(w, DIVIDE), put_int(w, v->q);
}

/* Item i split by the search into irreducible factors, its first piece in its place and the rest appended: 0
   all proven, 1 the last piece left whole (unproven), 2 no room for the pieces. */
static __attribute__((noinline)) int split(fitem *it, int i, int *ni, solve_ws *s, fwork *w, int32_t *dq, long *budget)
{
    ipoly h = it[i].p, g, q;
    int slot = i, r;
    bool fresh = true;
    for (;;) {
        r = h.n >= 2 ? factor_search(&h, s, w, dq, budget, fresh, &g, &q) : 0;
        if (slot < 0) {
            if (*ni > SOLVE_DEG) return 2;
            slot = (*ni)++, it[slot].mult = it[i].mult;
        }
        it[slot].p = r == 1 ? g : h, it[slot].proven = r >= 0;
        if (r != 1) return r < 0;
        slot = -1, h = q, fresh = false;
    }
}

/* Rule 4: the text, read back (in its own arena), is the input's polynomial co exactly. */
static __attribute__((noinline)) bool read_back(const char *text, int var, const rat *co, void *arena, size_t arena_cap)
{
    ctx r;
    ref rn;
    rat back[SOLVE_DEG + 1];
    bool same;
    start(&r, text, CASIM_RAD, arena, arena_cap);
    r.opaque = true;
    rn = parse(&r);
    if (r.st == CASIM_OK) pw(&r, rn, var, back);
    same = r.st == CASIM_OK;
    for (int k = 0; k <= SOLVE_DEG && same; k++) same = rat_cmp(&back[k], &co[k]) == 0;
    return same;
}

static __attribute__((noinline)) casim_status do_factor(const char *eqn, int var, int angle, char *out, size_t cap,
                                                         void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE, cr, ws, wr, dqr, itr;
    size_t ocap = cap < SLOT ? cap : SLOT, mark0, mark;
    out_t o = {out, ocap, 0, false};
    ipoly f;
    rat *co, content, inv;
    fitem *it;
    solve_ws *s;
    fwork *w;
    int32_t *dq;
    int ni = 0, n0, zero = 0, items = 0;
    bool partial = false;
    long budget = FACTOR_BUDGET;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    /* the arena: the coefficients, then the work areas, then the input's tree, dropped after the walk */
    cr = grab(&c, (SOLVE_DEG + 1) * sizeof(rat));
    mark0 = c.used;
    ws = grab(&c, sizeof(solve_ws));
    wr = grab(&c, sizeof(fwork));
    dqr = grab(&c, SOLVE_DIVS * sizeof(int32_t));
    itr = grab(&c, (SOLVE_DEG + 1) * sizeof(fitem));   /* distinct factors: their degrees sum to <= 12 */
    mark = c.used;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;
    if (c.st != CASIM_OK || cap == 0) return finish(&c, n, out, ocap);
    co = (rat *)(void *)nd(&c, cr);
    pw(&c, n, var, co);
    if (c.st != CASIM_OK || !clear(&c, co, &f)) return finish(&c, SPARE, out, ocap);
    c.used = mark;
    if (f.n <= 0) {
        put_content(&o, &co[0]);        /* a constant: itself */
        if (o.full) c.st = CASIM_TOO_LONG;
        return c.st == CASIM_OK ? CASIM_OK : finish(&c, SPARE, out, ocap);
    }
    if (!rat_make(&inv, 1, f.a[f.n]) || !rat_mul(&content, &co[f.n], &inv)) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
    s = (solve_ws *)(void *)nd(&c, ws);
    w = (fwork *)(void *)nd(&c, wr);
    dq = (int32_t *)(void *)nd(&c, dqr);
    it = (fitem *)(void *)nd(&c, itr);
    while (f.a[0] == 0) {
        memmove(f.a, f.a + 1, (size_t)f.n * sizeof f.a[0]);
        f.n--, zero++;
    }
    if (zero) it[ni++] = (fitem){{1, {0, 1}}, zero, true};
    repeated_factors(&f, s, w, dq, it, &ni);
    /* a factor the finder took out may itself be a power (a 9-fold root's ring split in two clusters gives
       (X+2)^2 four times, abacus #5203): the finder again on each, multiplicities multiplied */
    for (int j = zero ? 1 : 0; j < ni; j++) {
        int k0 = ni;
        repeated_factors(&it[j].p, s, w, dq, it, &ni);
        for (int t = k0; t < ni; t++) it[t].mult *= it[j].mult;
        if (it[j].p.n == 0) it[j].mult = 0, it[j].proven = true;     /* all of it taken out */
    }
    if (f.n >= 1) it[ni++] = (fitem){f, 1, false};
    /* each unproven item split by the search into irreducible factors (its first piece in its place, the rest
       after), or left whole when the search cannot decide */
    n0 = ni;
    for (int i = 0; i < n0; i++) {
        int r = it[i].proven ? 0 : split(it, i, &ni, s, w, dq, &budget);
        if (r == 2) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
        partial = partial || r == 1;
    }
    if (partial) {
        /* PARTIAL leaves one factor whole (rule 5's contract): the unproven ones multiplied back into it */
        int w0 = -1;
        for (int i = 0; i < ni; i++) {
            ipoly base = it[i].p;
            if (it[i].proven) continue;
            if (w0 < 0) w0 = i;
            for (int m = (w0 == i); m < it[i].mult; m++)
                if (!ipmul(&it[w0].p, &base)) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
            if (w0 != i) it[i].mult = 0;
        }
        it[w0].mult = 1;
    }
    {
        /* equal factors merged; then ascending degree, ties in 002's order of the canonical factor */
        ref tl = grab(&c, (size_t)ni * sizeof(ref));
        if (!tl) return finish(&c, SPARE, out, ocap);
        for (int i = 0; i < ni; i++)
            for (int j = i + 1; j < ni; j++)
                if (it[i].mult && it[j].mult && it[i].p.n == it[j].p.n &&
                    memcmp(it[i].p.a, it[j].p.a, (size_t)(it[i].p.n + 1) * sizeof(int64_t)) == 0)
                    it[i].mult += it[j].mult, it[j].mult = 0;
        for (int i = 0; i < ni; i++)
            if (it[i].mult) it[items] = it[i], list(&c, tl)[items++] = ipoly_tree(&c, &it[i].p, var);
        for (int i = 1; i < items; i++) {
            fitem t = it[i];
            ref tt = list(&c, tl)[i];
            int j = i;
            while (j > 0 && (it[j - 1].p.n > t.p.n || (it[j - 1].p.n == t.p.n && cmp(&c, list(&c, tl)[j - 1], tt) > 0))) {
                it[j] = it[j - 1], list(&c, tl)[j] = list(&c, tl)[j - 1];
                j--;
            }
            it[j] = t, list(&c, tl)[j] = tt;
        }
        if (rat_is(&content, -1, 1)) puts_(&o, "-1");
        else if (!rat_is(&content, 1, 1)) put_content(&o, &content);
        for (int i = 0; i < items; i++) {
            ref t = list(&c, tl)[i];
            bool paren = nd(&c, t)->kind == K_ADD && (items > 1 || it[i].mult > 1 || o.n > 0);
            if (o.n > 0) puts_(&o, TIMES);
            if (paren) puts_(&o, "(");
            print(&c, &o, t);
            if (paren) puts_(&o, ")");
            if (it[i].mult > 1) puts_(&o, "^"), put_int(&o, it[i].mult);
        }
    }
    if (o.full) c.st = CASIM_TOO_LONG;
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    if (!read_back(out, var, co, c.mem + mark0, c.cap - mark0)) {   /* everything after the coefficients is done with */
        out[0] = '\0';
        return CASIM_NO_ROOM;           /* never printed unverified (a defect if it happens; the arena if it is short) */
    }
    return partial ? CASIM_PARTIAL : CASIM_OK;
}

/* ---- the API: CAS 007 ---- */

casim_status casim_factor(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_factor(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- polynomial division, GCD and cancelling over Q (unit CAS 010) ---- */

/* the gcd's mod-p polynomials and its reconstruction, in the arena (rule 7: off the stack) */
typedef struct {
    uint32_t a[SOLVE_DEG + 1], b[SOLVE_DEG + 1], h[SOLVE_DEG + 1];
    int64_t acc[SOLVE_DEG + 1];
    ipoly cand, q;                      /* the candidate and its cofactor */
} gcdwork;

/* the operations' polynomials, in the arena too: f and g cleared, the gcd, the reduced quotient's two sides and its
   check, and the removed values' search (the gcd's work area is solve's then, the gcd being done) */
typedef struct {
    ipoly fi, gi, gd, nr, dr, back, h, rest;
    rat roots[SOLVE_DEG + 1];
    rat kf, kg, k, inv, d;              /* the contents and the reduced quotient's constant */
} polywork;

/* The primes, largest 31-bit first. A test build (tests/gcdprimes_check.c) swaps in small ones to reach the paths the
   real primes never need: an unlucky prime, a candidate that fails, a reconstruction that wraps. */
#ifndef CASIM_GCD_PRIMES
#define CASIM_GCD_PRIMES 2147483629u, 2147483587u, 2147483579u, 2147483563u, 2147483549u, 2147483543u, 2147483497u, 2147483489u
#endif
static const uint32_t GCD_PRIMES[] = {CASIM_GCD_PRIMES};

static uint32_t inv_mod(uint32_t a, uint32_t p)
{
    int64_t t = 0, nt = 1, r = p, nr = a;
    while (nr) {
        int64_t q = r / nr, x = t - q * nt;
        t = nt, nt = x, x = r - q * nr, r = nr, nr = x;
    }
    return (uint32_t)(t < 0 ? t + p : t);
}

/* f mod p into m; its degree (-1: 0) */
static int to_modp(const ipoly *f, uint32_t p, uint32_t *m)
{
    int n = f->n;
    for (int i = 0; i <= n; i++) {
        int64_t v = f->a[i] % (int64_t)p;
        m[i] = (uint32_t)(v < 0 ? v + p : v);
    }
    while (n >= 0 && m[n] == 0) n--;
    return n;
}

/* the monic gcd of w->a (degree na) and w->b (nb) mod p, into w->h; its degree */
static int gcd_modp(gcdwork *w, int na, int nb, uint32_t p)
{
    uint32_t *a = w->a, *b = w->b, *t;
    while (nb >= 0) {
        uint32_t il = inv_mod(b[nb], p);
        while (na >= nb) {
            uint32_t q = (uint32_t)((uint64_t)a[na] * il % p);
            int s = na - nb;
            for (int j = 0; j <= nb; j++) a[s + j] = (uint32_t)((a[s + j] + p - (uint64_t)q * b[j] % p) % p);
            while (na >= 0 && a[na] == 0) na--;
        }
        t = a, a = b, b = t;
        { int x = na; na = nb, nb = x; }
    }
    if (na >= 0) {
        uint32_t il = inv_mod(a[na], p);
        for (int i = 0; i <= na; i++) w->h[i] = (uint32_t)((uint64_t)a[i] * il % p);
    }
    return na;
}

/* gcd of f and g, primitive integer polynomials of degree >= 1 with positive leading coefficients (rule 3) */
static __attribute__((noinline)) bool int_gcd(const ipoly *f, const ipoly *g, gcdwork *w, ipoly *out)
{
    int64_t gam = gcd64(f->a[f->n], g->a[g->n]), M = 1;
    int best = SOLVE_DEG + 1;
    bool have = false;
    for (int pi = 0; pi < (int)(sizeof GCD_PRIMES / sizeof GCD_PRIMES[0]); pi++) {
        uint32_t p = GCD_PRIMES[pi], gp;
        int nh;
        ipoly *cand = &w->cand, *q = &w->q;
        int64_t ct = 0;
        if (f->a[f->n] % (int64_t)p == 0 || g->a[g->n] % (int64_t)p == 0) continue;
        nh = gcd_modp(w, to_modp(f, p, w->a), to_modp(g, p, w->b), p);
        if (nh > best) continue;                        /* an unlucky prime: its gcd's degree is too large */
        if (nh < best) best = nh, have = false;         /* a smaller degree: start the reconstruction again */
        if (best == 0) return out->n = 0, out->a[0] = 1, true;
        gp = (uint32_t)(((gam % (int64_t)p) + p) % p);
        for (int i = 0; i <= best; i++) w->h[i] = (uint32_t)((uint64_t)w->h[i] * gp % p);     /* its lc is gam */
        if (!have) {
            for (int i = 0; i <= best; i++) w->acc[i] = w->h[i] > p / 2 ? (int64_t)w->h[i] - p : (int64_t)w->h[i];
            M = p, have = true;
        } else {
            /* Garner: acc + M × ((h - acc) / M mod p), symmetric, every step overflow-checked */
            uint32_t iM = inv_mod((uint32_t)(M % (int64_t)p), p);
            int64_t nM;
            if (__builtin_mul_overflow(M, (int64_t)p, &nM)) return false;
            for (int i = 0; i <= best; i++) {
                int64_t am = w->acc[i] % (int64_t)p, ds, t;
                uint64_t d;
                if (am < 0) am += p;
                d = ((uint64_t)w->h[i] + p - (uint64_t)am) % p * iM % p;
                ds = d > p / 2 ? (int64_t)d - p : (int64_t)d;
                if (__builtin_mul_overflow(M, ds, &t) || __builtin_add_overflow(w->acc[i], t, &w->acc[i])) return false;
            }
            M = nM;
        }
        cand->n = best;
        for (int i = 0; i <= best; i++) cand->a[i] = w->acc[i], ct = gcd64(ct, w->acc[i]);
        if (ct == 0) continue;
        for (int i = 0; i <= best; i++) cand->a[i] /= cand->a[best] < 0 ? -ct : ct;
        /* Rule 3's proof: the candidate is accepted only if it divides both f and g exactly. Then it divides their
           true gcd, so its degree is at most the gcd's; and the gcd's degree is at most every modular degree (a gcd
           over Z reduces mod p to a divisor of the gcd mod p), so at most best, which is the candidate's. The degrees
           are equal, and a divisor of the gcd of the same degree, primitive with a positive lc, is the gcd. */
        if (div_poly(f, cand, q) && div_poly(g, cand, q)) return *out = *cand, true;
    }
    return false;                                       /* NO_ROOM: never an unproven gcd */
}

/* a polynomial text read by CAS 005's walk into co: the first (eqn) as parsed already, or the second (repl) parsed here
   as subst parses its replacement, its SYNTAX offset counted after eqn */
static ref read_second(ctx *c, const char *eqn, const char *repl, bool repl_rtl, bool rtl)
{
    ref r;
    int chars = 0;
    for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    c->s2 = repl, c->s = repl, c->no_eq = true, c->rtl = repl_rtl;
    r = parse(c);
    c->rtl = rtl, c->s = eqn;
    if (c->st == CASIM_SYNTAX) c->errpos += chars + 1;
    return r;
}

/* a polynomial's degree, -1 for 0 (pdeg says 0 for both 0 and a constant) */
static int rdeg(const rat *p)
{
    int d = pdeg(p);
    return d == 0 && p[0].p == 0 ? -1 : d;
}

/* p × k for every coefficient */
static bool rat_scale(rat *p, const rat *k)
{
    for (int i = 0; i <= SOLVE_DEG; i++)
        if (p[i].p != 0 && !rat_mul(&p[i], &p[i], k)) return false;
    return true;
}

/* f = q g + r over Q; false on an int64 overflow. Checked: q g + r is f exactly, deg r < deg g (rule 5) */
static __attribute__((noinline)) bool pdiv_q(const rat *f, const rat *g, rat *q, rat *r, rat *s)
{
    int nf = rdeg(f), ng = rdeg(g);
    rat inv, t;
    pzero(q);
    for (int k = 0; k <= SOLVE_DEG; k++) r[k] = f[k];
    if (!rat_make(&inv, g[ng].q, g[ng].p)) return false;
    for (int i = nf; i >= ng; i--) {
        if (!rat_mul(&q[i - ng], &r[i], &inv)) return false;
        for (int j = 0; j <= ng; j++) {
            if (!rat_mul(&t, &q[i - ng], &g[j])) return false;
            t.p = -t.p;
            if (!rat_add(&r[i - ng + j], &r[i - ng + j], &t)) return false;
        }
    }
    if (rdeg(r) >= ng) return false;
    /* the check: s = q g + r, which must be f */
    for (int k = 0; k <= SOLVE_DEG; k++) s[k] = r[k];
    for (int i = 0; i <= SOLVE_DEG; i++)
        for (int j = 0; j <= ng && q[i].p != 0; j++) {
            if (i + j > SOLVE_DEG || !rat_mul(&t, &q[i], &g[j]) || !rat_add(&s[i + j], &s[i + j], &t)) return false;
        }
    for (int k = 0; k <= SOLVE_DEG; k++)
        if (rat_cmp(&s[k], &f[k]) != 0) return false;
    return true;
}

/* the content of co cleared to f: co's lead over f's (f = co / content) */
static bool content_of(const rat *co, const ipoly *f, rat *k)
{
    rat inv;
    return rat_make(&inv, 1, f->a[f->n]) && rat_mul(k, &co[f->n], &inv);
}

/* the rows CANCEL prints for the removed values: X=a for each rational root of g, ascending and once, then the rest
   of g as p=0 when it has no rational root */
static __attribute__((noinline)) void removed_rows(ctx *c, out_t *w, const ipoly *g, int var, solve_ws *s, int32_t *dq,
                                                  polywork *P)
{
    ipoly *h = &P->h, *rest = &P->rest, *q = &P->back;
    rat *roots = P->roots;
    int nr = 0;
    *h = *g, *rest = *g;
    if (h->a[0] == 0) {
        rat_int(&roots[nr++], 0);
        while (h->n > 0 && h->a[0] == 0) memmove(h->a, h->a + 1, (size_t)h->n * sizeof h->a[0]), h->n--;
    }
    s->nfac = 0;
    if (h->n >= 1) factors(c, h, s, dq);
    for (int i = 0; i < s->nfac; i++)
        if (s->fac[i].deg == 1) rat_make(&roots[nr++], s->fac[i].b, s->fac[i].a);
    for (int i = 1; i < nr; i++)
        for (int j = i; j > 0 && rat_cmp(&roots[j - 1], &roots[j]) > 0; j--) {
            rat t = roots[j];
            roots[j] = roots[j - 1], roots[j - 1] = t;
        }
    for (int i = 0; i < nr; i++) {
        ipoly lin = {1, {-roots[i].p, roots[i].q}};
        char v[2] = {(char)('A' + var), 0};
        while (rest->n >= 1 && div_poly(rest, &lin, q)) *rest = *q;
        puts_(w, "; "), puts_(w, v), puts_(w, "=");
        put_int(w, roots[i].p);
        if (roots[i].q != 1) puts_(w, DIVIDE), put_int(w, roots[i].q);
    }
    if (rest->n >= 1) {
        puts_(w, "; ");
        print(c, w, ipoly_tree(c, rest, var));
        puts_(w, "=0");
    }
}

static __attribute__((noinline)) casim_status do_poly(int op, const char *eqn, int var, const char *repl, int angle,
                                                      char *out, size_t cap, void *arena, size_t arena_cap, bool rtl,
                                                      bool repl_rtl)
{
    ctx c;
    ref n = SPARE, n2 = SPARE, cr, wr, pr, mark;
    size_t ocap = cap < SLOT ? cap : SLOT;
    out_t w = {out, ocap, 0, false};
    rat *f, *g, *q, *r, *s;
    if (cap) out[0] = '\0';
    if (!eqn || (op != CASIM_CANCEL && !repl)) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    cr = grab(&c, 5 * (SOLVE_DEG + 1) * sizeof(rat));
    wr = grab(&c, sizeof(gcdwork) > sizeof(solve_ws) ? sizeof(gcdwork) : sizeof(solve_ws));
    pr = grab(&c, sizeof(polywork));
    mark = (ref)c.used;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;          /* an equation */
    if (c.st == CASIM_OK && op != CASIM_CANCEL) n2 = read_second(&c, eqn, repl, repl_rtl, rtl);
    if (c.st != CASIM_OK || cap == 0) return finish(&c, SPARE, out, ocap);
    f = (rat *)(void *)nd(&c, cr), g = f + SOLVE_DEG + 1, q = g + SOLVE_DEG + 1, r = q + SOLVE_DEG + 1, s = r + SOLVE_DEG + 1;
    if (op == CASIM_CANCEL) {
        /* num÷den, as typed: the top division's two sides (no division: den is 1) */
        ref t = n;
        while (nd(&c, t)->kind == K_GROUP) t = nd(&c, t)->a;
        if (nd(&c, t)->kind == K_BDIV) pw(&c, nd(&c, t)->a, var, f), pw(&c, nd(&c, t)->b, var, g);
        else pw(&c, t, var, f), pzero(g), rat_int(&g[0], 1);
    } else {
        pw(&c, n, var, f);
        if (c.st == CASIM_OK) {
            c.s = repl;
            pw(&c, n2, var, g);
            c.s = eqn;
        }
    }
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    if (op == CASIM_PDIV) {
        polywork *P = (polywork *)(void *)nd(&c, pr);
        if (rdeg(g) < 0) return c.st = CASIM_UNDEFINED, finish(&c, SPARE, out, ocap);
        if (!clear(&c, f, &P->h) || !clear(&c, g, &P->h)) return finish(&c, SPARE, out, ocap);     /* rule 1's bounds */
        if (!pdiv_q(f, g, q, r, s)) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
        c.used = (size_t)mark;
        print(&c, &w, poly_tree(&c, q, var));
        puts_(&w, "; ");
        print(&c, &w, poly_tree(&c, r, var));
    } else {
        polywork *P = (polywork *)(void *)nd(&c, pr);
        ipoly *fi = &P->fi, *gi = &P->gi, *gd = &P->gd, *nr = &P->nr, *dr = &P->dr;
        rat *kf = &P->kf, *kg = &P->kg, *k = &P->k, *inv = &P->inv;
        if (!clear(&c, f, fi) || !clear(&c, g, gi)) return finish(&c, SPARE, out, ocap);
        if (op == CASIM_CANCEL && gi->n < 0) return c.st = CASIM_UNDEFINED, finish(&c, SPARE, out, ocap);   /* ÷0 */
        /* the gcd: of 0 and 0, 0; with 0, the other's primitive part; with a constant, 1 */
        if (fi->n < 0 && gi->n < 0) gd->n = -1;
        else if (fi->n < 0 || gi->n < 0) *gd = fi->n < 0 ? *gi : *fi;
        else if (fi->n == 0 || gi->n == 0) gd->n = 0, gd->a[0] = 1;
        else if (!int_gcd(fi, gi, (gcdwork *)(void *)nd(&c, wr), gd)) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
        if (gd->n == 0) gd->a[0] = 1;
        if (op == CASIM_PGCD) c.used = (size_t)mark;          /* CANCEL still prints from the typed tree */
        if (op == CASIM_PGCD) {
            if (gd->n < 0) puts_(&w, "0");
            else print(&c, &w, ipoly_tree(&c, gd, var));
        } else if (gd->n <= 0) {
            print(&c, &w, canon(&c, n));                   /* nothing cancels: the quotient as 002 writes it */
        } else {
            /* the reduced quotient: (kf / kg) × (fi / gd) ÷ (gi / gd), checked (rule 5) */
            ipoly *bk = &P->back;
            ref num_t;
            if (!div_poly(fi, gd, nr) || !div_poly(gi, gd, dr) || !ipmul((*bk = *nr, bk), gd) ||
                memcmp(bk->a, fi->a, (size_t)(fi->n + 1) * sizeof fi->a[0]) != 0 || !ipmul((*bk = *dr, bk), gd) ||
                memcmp(bk->a, gi->a, (size_t)(gi->n + 1) * sizeof gi->a[0]) != 0)
                return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);                 /* never unchecked */
            if (!content_of(f, fi, kf) || !content_of(g, gi, kg) || !rat_make(inv, kg->q, kg->p) || !rat_mul(k, kf, inv))
                return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
            if (dr->n == 0 && (!rat_make(&P->d, 1, dr->a[0]) || !rat_mul(k, k, &P->d)))
                return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
            pzero(q);
            for (int i = 0; i <= nr->n; i++) rat_int(&q[i], 0), rat_make(&q[i], nr->a[i], 1);
            if (!rat_scale(q, k)) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
            num_t = poly_tree(&c, q, var);
            if (dr->n > 0) num_t = mul2(&c, num_t, power(&c, ipoly_tree(&c, dr, var), small(&c, C_M1)));
            print(&c, &w, num_t);
            {
                ref dqr = grab(&c, SOLVE_DIVS * sizeof(int32_t));
                if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
                removed_rows(&c, &w, gd, var, (solve_ws *)(void *)nd(&c, wr), (int32_t *)(void *)nd(&c, dqr), P);
            }
        }
    }
    if (w.full) c.st = CASIM_TOO_LONG;
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    return CASIM_OK;
}

casim_status casim_pdiv(const char *eqn, int var, const char *repl, int model, int repl_model, int angle, char *out,
                        size_t cap, void *arena, size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model) || !model_ok(repl_model)) return CASIM_BAD_ARG;
    return do_poly(CASIM_PDIV, eqn, var, repl, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU,
                   repl_model == CASIM_MODEL_STU);
}

casim_status casim_pgcd(const char *eqn, int var, const char *repl, int model, int repl_model, int angle, char *out,
                        size_t cap, void *arena, size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model) || !model_ok(repl_model)) return CASIM_BAD_ARG;
    return do_poly(CASIM_PGCD, eqn, var, repl, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU,
                   repl_model == CASIM_MODEL_STU);
}

casim_status casim_cancel(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_poly(CASIM_CANCEL, eqn, var, NULL, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU, false);
}
