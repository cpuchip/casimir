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
    bool own_text;              /* the text is casim's own print (taylor's round trip, read with opaque names): an
                                   opaque SGN is the SGN derive writes */
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
    bool steps;                 /* show steps: derive records each node's (before, after) as it returns */
    ref step_last;              /* the last step recorded; each links to the one before */
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
   stays within 12 KB with interrupts on top (unit CAS 002b; abacus's ruling, #4236). An interrupt's frame is 192 B
   on the S3, but readings with interrupts masked against unmasked showed up to 272 B more (#6319), so the allowance
   is two frames, 384 B, until that is explained (abacus #6321). The sum, product and power constructors
   are not counted: they recurse only one level on their own, and their depth is the walks' (counting them
   too made a nest of sums cost two units a level, against one for a nest of powers, at the same stack). */
/* scripts/frame-table.py's control plants a frame here (in do_poly); otherwise nothing */
#ifndef CASIM_FRAME_PLANT
#define CASIM_FRAME_PLANT
#endif

#ifndef CASIM_DEPTH_MAX
#define CASIM_DEPTH_MAX 44              /* 47 until CAS 010 (abacus #5634), 46 until rc5's allowance (#6321) */
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
static inline __attribute__((always_inline)) ref deriv(ctx *c, ref r);

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

/* bytes the arena holds already, where grab would put them next: their ref, the bytes kept (show steps reads what
   do_solve left in the arena, at the places it grabbed them) */
static ref held(ctx *c, size_t bytes)
{
    size_t need = (bytes + 7u) & ~(size_t)7u;
    ref r;
    if (c->st == CASIM_OK && c->used + need > c->top) c->st = CASIM_NO_ROOM;
    if (c->st != CASIM_OK) return 0;
    r = (ref)(c->used / 8u);
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

/* cas-pi-form: the logs of positive numbers in a sum combine. Terms k×LN(a) whose coefficients share one denominator
   are one log, the coefficients' gcd g outside: Σ k×LN(a) = g×LN(Π a^(k/g)) (2×LN(2)+2×LN(3) is 2×LN(6)); LOG
   likewise. LN(1÷m) is -LN(m), LOG(10^j) is j. Numbers only, never a letter (LN(X)+LN(Y) would change the domain). */

/* the gcd of two rationals >= 0, gcd(p1, p2) ÷ lcm(q1, q2); gcd(0, y) = y */
static __attribute__((noinline)) bool rat_gcd(rat *r, const rat *x, const rat *y)
{
    int64_t gq, l;
    if (x->p == 0) return *r = *y, true;
    gq = gcd64(x->q, y->q);
    if (!mul64(&l, x->q / gq, y->q)) return false;
    return rat_make(r, gcd64(x->p, y->p), l);
}

/* t as k×LN(a) or k×LOG(a), a a positive number: the function (k and a set), or -1 */
static __attribute__((noinline)) int log_term(const ctx *c, ref t, rat *k, rat *a)
{
    const node *x = nd(c, t);
    ref f = t;
    rat_int(k, 1);
    if (x->kind == K_MUL) {
        if (x->n != 2 || !is_num(c, list(c, x->a)[0])) return -1;
        *k = *val(c, list(c, x->a)[0]);
        f = list(c, x->a)[1];
    }
    x = nd(c, f);
    if (x->kind != K_FN || (x->id != F_LN && x->id != F_LOG) || !is_num(c, x->a) || val(c, x->a)->p <= 0) return -1;
    *a = *val(c, x->a);
    return x->id;
}

/* whether t is a log of function f whose coefficient has denominator den (one group: k and a set) */
static bool in_group(const ctx *c, ref t, int f, int64_t den, rat *k, rat *a)
{
    return log_term(c, t, k, a) == f && k->q == den;
}

/* whether n > 0 is 10^j, j >= 0 */
static bool pow10_of(int64_t n, long *j)
{
    *j = 0;
    while (n > 1 && n % 10 == 0) n /= 10, ++*j;
    return n == 1;
}

/* f(q), q > 0, in its form: 0 for q = 1, j for LOG(10^j) (and 10^-j), -f(m) for q = 1÷m, else f(q) */
static __attribute__((noinline)) ref log_of(ctx *c, int f, const rat *q)
{
    rat m = *q;
    long j;
    bool neg_ = false;
    ref r;
    if (rat_is(q, 1, 1)) return small(c, C_0);
    if (f == F_LOG && ((q->q == 1 && pow10_of(q->p, &j)) || (q->p == 1 && pow10_of(q->q, &j)))) {
        rat_int(&m, (int)(q->q == 1 ? j : -j));
        return num(c, &m);
    }
    if (q->p == 1) rat_int(&m, 1), m.p = q->q, neg_ = true;
    r = mk(c, K_FN, num(c, &m), 0);
    if (r == SPARE) return SPARE;
    nd(c, r)->id = (uint8_t)f;
    return neg_ ? mul_node(c, small(c, C_M1), &r, 1) : r;
}

/* A sum's logs, t[0..*k): two or more of one function whose coefficients share a denominator become one term.
   Returns whether t changed (one group a call; the caller runs the sum's steps again). Past 2^62 a group stays. */
static __attribute__((noinline)) bool combine_logs(ctx *c, ref *t, int *k)
{
    for (int f = F_LN; f <= F_LOG; f++)
        for (int i = 0; i < *k; i++) {
            rat ki, ai, kj, aj, g, q, x;
            int n = 0, w = 0;
            ref r;
            if (log_term(c, t[i], &ki, &ai) != f) continue;
            rat_int(&g, 0);
            for (int j = i; j < *k; j++)
                if (in_group(c, t[j], f, ki.q, &kj, &aj)) {
                    if (kj.p < 0) kj.p = -kj.p;
                    if (!rat_gcd(&g, &g, &kj)) return false;
                    n++;
                }
            if (n < 2) continue;
            rat_int(&q, 1);
            for (int j = i; j < *k; j++)
                if (in_group(c, t[j], f, ki.q, &kj, &aj)) {
                    rat inv, e, p;
                    long wh;
                    if (!rat_make(&inv, g.q, g.p) || !rat_mul(&e, &kj, &inv) || !rat_whole(&e, &wh) ||
                        !rat_pow(&p, &aj, wh) || !rat_mul(&q, &q, &p))
                        return false;
                }
            if ((r = log_of(c, f, &q)) == SPARE) return false;
            if (is_num(c, r)) {
                if (!rat_mul(&x, val(c, r), &g)) return false;
                r = num(c, &x);
            } else if (!rat_is(&g, 1, 1))
                r = mul2(c, num(c, &g), r);
            for (int j = 0; j < *k; j++)
                if (j < i || !in_group(c, t[j], f, ki.q, &kj, &aj)) t[w++] = t[j];
            if (!is_k(c, r, 0, 1)) t[w++] = r;
            *k = w;
            return true;
        }
    return false;
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
    if (combine_logs(c, t, &k)) {
        /* a group of logs is one term: it sorts elsewhere, may be a number (LOG(10)) or like another term. Each
           call leaves fewer logs, so this ends. */
        m = k, k = 0;
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
        if (is_k(c, a, 0, 1)) return c->st = CASIM_UNDEFINED, SPARE;     /* CAS 016 rule 1: a pole, as 1÷0 is */
        if (is_num(c, a) && val(c, a)->p == 1 && val(c, a)->q > 1) return log_of(c, f, val(c, a));   /* -LN(m) */
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
        if (!angle_n(c, a, &n)) break;
        if (table_val(f, n, &v)) return xval_tree(c, &v);
        if (f == F_TAN && table_val(F_COS, n, &v) && v.p == 0)
            return c->st = CASIM_UNDEFINED, SPARE;                  /* CAS 016 rule 1: COS is 0 there, a pole */
        break;
    }
    case F_ASIN:
    case F_ACOS:
    case F_ATAN:
        if ((r = exact_inverse(c, f, a)) != 0) return r;
        break;
    case F_LOG:
        if (x->kind == K_FN && x->id == F_ALOG) return x->a;
        if (is_k(c, a, 0, 1)) return c->st = CASIM_UNDEFINED, SPARE;     /* CAS 016 rule 1 */
        if (is_num(c, a) && val(c, a)->p > 0) {
            long j;
            const rat *v = val(c, a);
            if ((v->q == 1 && pow10_of(v->p, &j)) || (v->p == 1 && v->q > 1)) return log_of(c, f, v);    /* j, -LOG(m) */
        }
        break;
    case F_ALOG:
        if (x->kind == K_FN && x->id == F_LOG) return x->a;
        if (is_k(c, a, 0, 1)) return small(c, C_1);                 /* CAS 016: the exact values a pole hides behind */
        break;
    case F_ATANH:
        if (is_k(c, a, 1, 1) || is_k(c, a, -1, 1)) return c->st = CASIM_UNDEFINED, SPARE;   /* CAS 016 rule 1 */
        if (is_k(c, a, 0, 1)) return small(c, C_0);
        break;
    case F_SINH:
    case F_TANH:
    case F_ASINH:
        if (is_k(c, a, 0, 1)) return small(c, C_0);                 /* CAS 016 (abacus #7951): SINH(0), TANH(0), ASINH(0) */
        break;
    case F_COSH: if (is_k(c, a, 0, 1)) return small(c, C_1); break;
    case F_ACOSH: if (is_k(c, a, 1, 1)) return small(c, C_0); break;
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
    ref a = canon(c, nd(c, r)->a), b = nd(c, r)->b ? canon(c, nd(c, r)->b) : 0, o;
    if (c->own_text && !b && strcmp(NAMES[nd(c, r)->id].name, "SGN") == 0)
        return fn(c, F_SGN, a);                 /* ABS's derivative, in casim's own text */
    o = mk(c, K_OFN, a, b);
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
/* Inline, always: show steps' templates call it too, and as a call of its own it moved deriv_'s code. */
static inline __attribute__((always_inline)) ref deriv_fn(ctx *c, ref r, ref du)
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

/* Show steps (work/steps-derive.md): one step in one 8-byte unit, linked backwards. */
typedef struct {
    ref prev, before, after, pad;
} step_t;

static step_t *step_at(const ctx *c, ref s)
{
    return (step_t *)(void *)(c->mem + (size_t)s * 8u);
}

/* A node's step, as it returns (a leaf makes none). Out of line: off deriv_'s recursive frame. */
static __attribute__((noinline)) void note(ctx *c, ref r, ref v)
{
    int k = nd(c, r)->kind;
    ref s;
    if (k == K_NUM || k == K_VAR || k == K_PI || v == SPARE || c->st != CASIM_OK || !(s = grab(c, 8))) return;
    step_at(c, s)->prev = c->step_last;
    step_at(c, s)->before = r;
    step_at(c, s)->after = v;
    c->step_last = s;
}

/* Inline, always: a call of its own would be a frame at every level of the recursion (32 bytes on the S3, 46
   levels: three times the stack's margin). make check's frame gate refuses a build where it is not. */
static inline __attribute__((always_inline)) ref deriv(ctx *c, ref r)
{
    ref v;
    if (!enter(c)) return SPARE;
    v = deriv_(c, r);
    if (c->steps) note(c, r, v);
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
static __attribute__((noinline)) void coef_parts(const ctx *c, ref f0, bool frac, bool *top, bool *bottom)
{
    rat co;
    abn d;
    *top = *bottom = false;
    if (!f0) return;
    co = *val(c, f0);
    co.p = co.p < 0 ? -co.p : co.p;
    if (!frac && decimal_of(&co, &d)) *top = !rat_is(&co, 1, 1);
    else *top = co.p != 1, *bottom = co.q != 1;
}

/* The coefficient's sign and numerator (top) or its ÷ denominator, p÷q whole when frac; returns whether it printed
   an item. */
static __attribute__((noinline)) int put_coef(const ctx *c, out_t *w, ref f0, bool frac, bool flip, bool top)
{
    rat co;
    abn d;
    bool neg_;
    rat_int(&co, 1);
    if (f0) co = *val(c, f0);
    neg_ = (co.p < 0) != flip;
    co.p = co.p < 0 ? -co.p : co.p;
    if (top && neg_) puts_(w, "-");
    if (!frac && decimal_of(&co, &d)) {
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

/* Whether a product's coefficient prints as a fraction (cas-pi-form): π, or π to a whole positive power, among its
   numerator's factors. π÷4 and π^2÷32, not 0.25×π; under a ÷ or a root π keeps the decimal (0.5÷π, 0.5×SQRT(π)). */
static __attribute__((noinline)) bool pi_frac(const ctx *c, const ref *f, int start, int n)
{
    if (!start) return false;
    for (int i = start; i < n; i++) {
        const node *x = nd(c, f[i]);
        if (x->kind == K_PI) return true;
        if (x->kind == K_POW && nd(c, x->a)->kind == K_PI && is_num(c, x->b) && val(c, x->b)->q == 1 &&
            val(c, x->b)->p > 0)
            return true;
    }
    return false;
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
    coef_parts(c, start ? f[0] : 0, pi_frac(c, f, start, n), &top, &bottom);
    items = put_coef(c, w, start ? f[0] : 0, pi_frac(c, f, start, n), flip, true);
    if (bottom) {
        if (!items) puts_(w, "1");
        put_coef(c, w, start ? f[0] : 0, pi_frac(c, f, start, n), flip, false);
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
    items = put_coef(c, w, start ? f[0] : 0, pi_frac(c, f, start, n), flip, true);
    for (int i = start; i < n; i++) {
        if (below(c, f[i])) continue;
        if (items++) puts_(w, TIMES);
        put_factor(c, w, f[i], 2);
    }
    if (!items) puts_(w, "1");
    put_coef(c, w, start ? f[0] : 0, pi_frac(c, f, start, n), flip, false);
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
    coef_parts(c, start ? f[0] : 0, pi_frac(c, f, start, n), &top, &bottom);
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

/* ---- show steps (work/steps-derive.md) ---- */

static bool model_ok(int m);

/* The walk over a derive's steps: each node's template line (its rule with the parts still to do), its parts' steps,
   then its result; CASIM_STEPS_ONE_LINE leaves the template lines out. The last step is the answer either way. It
   counts every step and notes step k. */
typedef struct {
    int k, at, flags;
    casim_step_info *info;
    ref r;                      /* step k's node */
} walk_t;

static const char *const RULE_NAME[] = {"SUM", "CONSTANT", "VARIABLE", "CONSTANT MULTIPLE", "PRODUCT", "POWER",
                                        "EXPONENTIAL", "GENERAL POWER", "CHAIN", "BOTH SIDES", "RECIPROCAL",
                                        "POLYNOMIAL", "CHECK", "INTEGER COEFFICIENTS", "COMMON FACTOR",
                                        "LINEAR FACTOR", "QUADRATIC FACTOR", "LINEAR", "QUADRATIC FORMULA", "LEFT",
                                        "ANSWER"};
static const char *const RULE_EXT_NAME[] = {"POLYNOMIAL PART", "FACTOR", "FORM", "CLEARED", "COVER-UP", "COEFFICIENTS",
                                               "PARTIAL FRACTIONS", "SPLIT", "LOG DERIVATIVE", "COMPLETE THE SQUARE",
                                               "ARCTANGENT", "SUBSTITUTION", "BY PARTS", "CYCLE"};

const char *casim_rule_name(int rule)
{
    if (rule >= 0 && rule < CASIM_RULE_FUNCTION) return RULE_NAME[rule];
    if (rule >= CASIM_RULE_EXT && rule < CASIM_RULE_EXT + (int)(sizeof RULE_EXT_NAME / sizeof RULE_EXT_NAME[0]))
        return RULE_EXT_NAME[rule - CASIM_RULE_EXT];
    if (rule >= CASIM_RULE_FUNCTION && rule < CASIM_RULE_FUNCTION + (int)(sizeof FNAME / sizeof FNAME[0]))
        return FNAME[rule - CASIM_RULE_FUNCTION];
    return "";
}

static bool is_var(const ctx *c, ref r)
{
    return nd(c, r)->kind == K_VAR && nd(c, r)->id == c->var;
}

/* whether var occurs in r: asked of r's children, as the walk has already entered r (asking of r itself would
   count r's level twice, and a derivative at the depth guard's edge would page as NO_ROOM) */
static __attribute__((noinline)) bool dep(ctx *c, ref r)
{
    const node *x = nd(c, r);
    switch (x->kind) {
    case K_VAR: return x->id == c->var;
    case K_FN: return contains(c, x->a, c->var);
    case K_POW:
    case K_EQ: return contains(c, x->a, c->var) || contains(c, x->b, c->var);
    case K_ADD:
    case K_MUL:
        for (int i = 0; i < x->n; i++)
            if (contains(c, list(c, x->a)[i], c->var)) return true;
        return false;
    default: return false;
    }
}

static __attribute__((noinline)) int rule_of(ctx *c, ref r)
{
    const node *x = nd(c, r);
    int n = 0;
    if (!dep(c, r)) return CASIM_RULE_CONSTANT;
    switch (x->kind) {
    case K_VAR: return CASIM_RULE_VARIABLE;
    case K_ADD: return CASIM_RULE_SUM;
    case K_EQ: return CASIM_RULE_BOTH_SIDES;
    case K_MUL:
        for (int i = 0; i < x->n; i++) n += contains(c, list(c, x->a)[i], c->var);
        return n == 1 ? CASIM_RULE_CONSTANT_MULTIPLE : CASIM_RULE_PRODUCT;
    case K_POW:
        if (!contains(c, x->b, c->var)) return CASIM_RULE_POWER;
        return contains(c, x->a, c->var) ? CASIM_RULE_GENERAL_POWER : CASIM_RULE_EXPONENTIAL;
    case K_FN: return is_var(c, x->a) ? CASIM_RULE_FUNCTION + x->id : CASIM_RULE_CHAIN;
    default: return CASIM_RULE_CONSTANT;
    }
}

/* r's i-th part under its rule (what its template differentiates), or 0 */
static __attribute__((noinline)) ref part(ctx *c, ref r, int rule, int i)
{
    const node *x = nd(c, r);
    switch (rule) {
    case CASIM_RULE_SUM: return i < x->n ? list(c, x->a)[i] : 0;
    case CASIM_RULE_PRODUCT:
    case CASIM_RULE_CONSTANT_MULTIPLE:
        for (int j = 0; j < x->n; j++)
            if (contains(c, list(c, x->a)[j], c->var) && i-- == 0) return list(c, x->a)[j];
        return 0;
    case CASIM_RULE_POWER: return i == 0 ? x->a : 0;
    case CASIM_RULE_EXPONENTIAL: return i == 0 ? x->b : 0;
    case CASIM_RULE_GENERAL_POWER: return i == 0 ? x->b : i == 1 ? x->a : 0;     /* v, then u, as the template */
    case CASIM_RULE_CHAIN: return i == 0 ? x->a : 0;
    case CASIM_RULE_BOTH_SIDES: return i == 0 ? x->a : i == 1 ? nd(c, r)->b : 0;
    default: return 0;
    }
}

/* whether r's rule has a template: a part that is not var itself (X^2, SIN(X), 3×X are one line) */
static bool has_template(ctx *c, ref r, int rule)
{
    ref p;
    for (int i = 0; (p = part(c, r, rule, i)) != 0; i++)
        if (!is_var(c, p)) return true;
    return false;
}

/* r, in parentheses when its level is below min. The steps' own (not print_in: another caller of print_in moved
   gcc's inlining of it in put_factor, and every level of a power chain's printing paid a frame for it on the S3) */
static __attribute__((noinline)) void put_whole(ctx *c, out_t *w, ref r, int min)
{
    bool paren = level(c, r) < min;
    if (paren) puts_(w, "(");
    print(c, w, r);
    if (paren) puts_(w, ")");
}

static void put_dvar(ctx *c, out_t *w, ref r)
{
    char d[6] = {'d', '/', 'd', (char)('A' + c->var), '(', '\0'};
    puts_(w, d);
    print(c, w, r);
    puts_(w, ")");
}

/* a product's template: Σ over its parts j of the product with factor j as d/dX(fj) */
static void put_product_rule(ctx *c, out_t *w, ref r)
{
    const node *x = nd(c, r);
    int n = x->n, start = has_coef(c, list(c, x->a), n), terms = 0;
    bool neg_ = start && num_negative(c, list(c, x->a)[0]);
    for (int j = start; j < n; j++) {
        int items = 0;
        if (!contains(c, list(c, x->a)[j], c->var)) continue;
        if (terms++ && !neg_) puts_(w, "+");
        if (start) {
            rat k = *val(c, list(c, x->a)[0]);
            if (k.p < 0) puts_(w, "-"), k.p = -k.p;
            if (!rat_is(&k, 1, 1)) {
                ref kn = num(c, &k);
                put_factor(c, w, kn, 2);
                items++;
            }
        }
        for (int i = start; i < n; i++) {          /* the numerator, d/dX(fj) in fj's place */
            if (i != j && below(c, list(c, x->a)[i])) continue;
            if (items++) puts_(w, TIMES);
            if (i == j) put_dvar(c, w, list(c, x->a)[i]);
            else put_factor(c, w, list(c, x->a)[i], 2);
        }
        if (!items) puts_(w, "1");
        for (int i = start; i < n; i++) {          /* then ÷ each other factor with a negative exponent */
            if (i == j || !below(c, list(c, x->a)[i])) continue;
            puts_(w, DIVIDE);
            put_factor(c, w, list(c, x->a)[i], 3);
        }
    }
}

/* r's template line's right side, by its rule */
static __attribute__((noinline)) void put_template(ctx *c, out_t *w, ref r, int rule)
{
    const node *x = nd(c, r);
    ref t;
    switch (rule) {
    case CASIM_RULE_SUM:
        for (int i = 0; i < x->n; i++) {
            if (i) puts_(w, "+");
            put_dvar(c, w, list(c, x->a)[i]);
        }
        return;
    case CASIM_RULE_PRODUCT:
    case CASIM_RULE_CONSTANT_MULTIPLE: put_product_rule(c, w, r); return;
    case CASIM_RULE_POWER:          /* n×u^(n-1)×d/dX(u) */
        t = mul2(c, x->b, power(c, x->a, add2(c, x->b, small(c, C_M1))));
        break;
    case CASIM_RULE_EXPONENTIAL:    /* a^u×LN(a)×d/dX(u); 0^u is 0 where defined, as derive has it (LN(0) is a pole) */
        t = is_k(c, x->a, 0, 1) ? small(c, C_0) : mul2(c, r, fn(c, F_LN, x->a));
        break;
    case CASIM_RULE_GENERAL_POWER:  /* u^v×(d/dX(v)×LN(u)+v×d/dX(u)÷u) */
        put_whole(c, w, r, 2);
        puts_(w, TIMES "(");
        put_dvar(c, w, x->b);
        puts_(w, TIMES "LN(");
        print(c, w, x->a);
        puts_(w, ")+");
        put_whole(c, w, x->b, 2);
        puts_(w, TIMES);
        put_dvar(c, w, x->a);
        puts_(w, DIVIDE);
        put_whole(c, w, x->a, 5);                           /* the whole u: ÷(1÷X), not ÷X */
        puts_(w, ")");
        return;
    case CASIM_RULE_CHAIN: t = deriv_fn(c, r, small(c, C_1)); break;      /* f'(u)×d/dX(u) */
    case CASIM_RULE_BOTH_SIDES:
        put_dvar(c, w, x->a);
        puts_(w, "=");
        put_dvar(c, w, nd(c, r)->b);
        return;
    default: return;
    }
    if (c->st != CASIM_OK) return;
    put_whole(c, w, t, 2);
    puts_(w, TIMES);
    put_dvar(c, w, part(c, r, rule, 0));
}

/* r's recorded derivative (var's is 1, a constant's 0) */
static ref step_after(ctx *c, ref r, int rule)
{
    if (rule == CASIM_RULE_VARIABLE) return small(c, C_1);
    if (rule == CASIM_RULE_CONSTANT) return small(c, C_0);
    for (ref s = c->step_last; s; s = step_at(c, s)->prev)
        if (step_at(c, s)->before == r) return step_at(c, s)->after;
    if (c->st == CASIM_OK) c->st = CASIM_NO_ROOM;       /* not recorded: the steps did not fit */
    return SPARE;
}

/* one line of the walk: counted, and noted when it is step k (printed after the walk, at depth 0, as plain derive
   prints its answer: printed inside the walk, the root's answer would start a level deeper) */
static __attribute__((noinline)) void emit(walk_t *W, ref r, int rule, int depth, int line)
{
    if (W->at++ != W->k) return;
    W->info->depth = depth;
    W->info->line = line;
    W->info->rule = rule;
    W->r = r;
}

/* step k's text, from what the walk noted */
static __attribute__((noinline)) void put_step(ctx *c, out_t *w, const casim_step_info *info, ref r)
{
    if (info->line == CASIM_STEP_RESULT && info->rule == CASIM_RULE_BOTH_SIDES) {
        print(c, w, step_after(c, r, info->rule));
        return;
    }
    if (info->rule != CASIM_RULE_BOTH_SIDES) {
        put_dvar(c, w, r);
        puts_(w, " = ");
    }
    if (info->line == CASIM_STEP_TEMPLATE) put_template(c, w, r, info->rule);
    else print(c, w, step_after(c, r, info->rule));
}

static void walk(ctx *c, ref r, int depth, walk_t *W)
{
    int rule;
    bool tmpl, one = W->flags & CASIM_STEPS_ONE_LINE;
    ref p;
    if (!enter(c)) return;
    rule = rule_of(c, r);
    tmpl = has_template(c, r, rule);
    if (tmpl && !one) emit(W, r, rule, depth, CASIM_STEP_TEMPLATE);
    if (tmpl)
        for (int i = 0; (p = part(c, r, rule, i)) != 0 && c->st == CASIM_OK; i++) walk(c, p, depth + 1, W);
    emit(W, r, rule, depth, CASIM_STEP_RESULT);         /* both ways the walk ends on the answer (abacus #6252) */
    c->depth--;
}

static casim_status do_isteps(const casim_req *rq, int k, int flags, casim_step_info *info, char *out, size_t cap,
                              void *arena, size_t arena_cap);

static casim_status do_ssteps(const casim_req *rq, walk_t *W, out_t *w, void *arena, size_t arena_cap);
static casim_status do_psteps(const casim_req *rq, walk_t *W, out_t *w, void *arena, size_t arena_cap);

casim_status casim_step(const casim_req *rq, int k, int flags, casim_step_info *info, char *out, size_t cap,
                        void *arena, size_t arena_cap)
{
    ctx c;
    ref n = SPARE, root;
    out_t w = {out, cap, 0, false};
    walk_t W;
    casim_step_info none;
    if (!info) info = &none;
    if (rq && rq->op == CASIM_INTEGRATE) return do_isteps(rq, k, flags, info, out, cap, arena, arena_cap);
    memset(info, 0, sizeof *info);
    if (cap) out[0] = '\0';
    if (rq && rq->op == CASIM_SOLVE) {
        W.k = k, W.flags = flags, W.info = info;        /* five arguments: none on the stack (the S3 passes six) */
        return do_ssteps(rq, &W, &w, arena, arena_cap);
    }
    if (rq && rq->op == CASIM_PARTFRAC) {
        W.k = k, W.flags = flags, W.info = info;
        return do_psteps(rq, &W, &w, arena, arena_cap);
    }
    if (!rq || !rq->eqn || rq->op != CASIM_DERIVE || !model_ok(rq->model) || rq->var < 0 || rq->var > 25 ||
        rq->angle < CASIM_DEG || rq->angle > CASIM_GRAD)
        return CASIM_BAD_ARG;
    if (cap == 0 && k >= 0) return CASIM_TOO_LONG;
    start(&c, rq->eqn, rq->angle, arena, arena_cap);
    c.rtl = rq->model == CASIM_MODEL_STU;
    c.var = rq->var;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK) n = canon(&c, n);
    root = n;
    c.steps = true;
    if (c.st == CASIM_OK) n = deriv(&c, n);
    c.steps = false;
    if (c.st == CASIM_OK && (n == SPARE || n == 0)) c.st = CASIM_NO_ROOM;
    if (c.st != CASIM_OK) return cap ? finish(&c, n, out, cap) : c.st;       /* SYNTAX's place, a refused name */
    W.k = k, W.at = 0, W.flags = flags, W.info = info, W.r = 0;
    walk(&c, root, 0, &W);
    info->count = W.at;
    if (c.st == CASIM_OK && k >= W.at) c.st = CASIM_BAD_ARG;
    if (c.st == CASIM_OK && k >= 0) put_step(&c, &w, info, W.r);
    if (c.st == CASIM_OK && w.full) c.st = CASIM_TOO_LONG;
    if (c.st != CASIM_OK && cap) out[0] = '\0';
    return c.st;
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
    case CASIM_PARTFRAC: return casim_partfrac(rq->eqn, rq->var, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_TAYLOR: {
        /* the order as text in repl2 (work/014 rule 1 (b): casim_req unchanged): a whole number, else BAD_ARG */
        int n = 0;
        const char *p = rq->repl2;
        if (cap) out[0] = '\0';
        if (!p || !*p) return CASIM_BAD_ARG;
        for (; *p; p++) {
            if (*p < '0' || *p > '9' || n > 99) return CASIM_BAD_ARG;
            n = n * 10 + (*p - '0');
        }
        return casim_taylor(rq->eqn, rq->var, rq->repl, n, rq->model, rq->angle, out, cap, arena, arena_cap);
    }
    case CASIM_SUM:
        return casim_sum(rq->eqn, rq->var, rq->repl, rq->repl2, rq->model, rq->angle, out, cap, arena, arena_cap);
    case CASIM_LIMIT:
        return casim_limit(rq->eqn, rq->var, rq->repl, rq->repl2 ? rq->repl2 : "", rq->model, rq->angle, out, cap,
                           arena, arena_cap);
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

/* h / (q x - p) exactly, into g; false when it does not divide or a step would overflow. This, div_quad, square_free,
   quad_roots and put_root are always inline: solve inlined each (one caller) before show steps called them too, and
   its frames on the S3 stay as they were. */
static inline __attribute__((always_inline)) bool div_lin(const ipoly *h, int64_t q, int64_t p, ipoly *g)
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
static inline __attribute__((always_inline)) bool div_quad(const ipoly *h, int64_t a, int64_t b, int64_t c, ipoly *g)
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
static inline __attribute__((always_inline)) void square_free(int64_t d, int64_t *k, int64_t *r)
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
static inline __attribute__((always_inline)) bool quad_roots(iroot *rs, int *n, int64_t a, int64_t b, int64_t c, int mult)
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

static inline __attribute__((always_inline)) void put_root(out_t *w, const iroot *x)
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
        /* a function, π, a name, a draw: of var not a polynomial, else not a rational number; but a pole (TAN(π÷2),
           LN(0)) has no value at all, as in every op (CAS 016 rule 1) */
        if (raw_has_var(c, r, var)) c->st = CASIM_NOT_POLYNOMIAL;
        else {
            canon(c, r);
            if (c->st != CASIM_UNDEFINED) c->st = CASIM_NOT_NUMERIC;
        }
        break;
    }
    c->depth--;
    if (c->st == CASIM_OK) c->used = mark;
}

/* a pole in a raw r (CAS 016 rule 1): a function of a constant whose canonical form is UNDEFINED, anywhere in r. pw
   stops at its first refusal, so solve and factor ask this of the whole input when pw refuses it, and a pole decides
   whichever term it is in (SIN(X)+TAN(π÷2)=0, review) */
static bool pw_pole_(ctx *c, ref r, int var)
{
    const node *x = nd(c, r);
    bool p = false;
    if (r == 0 || r == SPARE || c->st != CASIM_OK || !enter(c)) return false;
    if (x->kind == K_RFN && !raw_has_var(c, r, var)) {
        canon(c, r);
        p = c->st == CASIM_UNDEFINED;
        c->st = CASIM_OK;
    } else if (x->kind == K_RFN || x->kind == K_NEG || x->kind == K_GROUP) p = pw_pole_(c, nd(c, r)->a, var);
    else if (x->kind >= K_BADD && x->kind <= K_BPOW) p = pw_pole_(c, x->a, var) || pw_pole_(c, nd(c, r)->b, var);
    else if (x->kind == K_REQ) p = pw_pole_(c, x->a, var) || pw_pole_(c, nd(c, r)->b, var);
    c->depth--;
    return p;
}

/* pw refused r (NOT_POLYNOMIAL, NOT_NUMERIC): UNDEFINED instead when a pole is in it */
static __attribute__((noinline)) void pw_refused(ctx *c, ref r, int var)
{
    casim_status s = c->st;
    size_t mark = c->used;
    if (s != CASIM_NOT_POLYNOMIAL && s != CASIM_NOT_NUMERIC) return;
    c->st = CASIM_OK;
    s = pw_pole_(c, r, var) ? CASIM_UNDEFINED : s;
    c->used = mark;
    c->st = s;
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
    if (c.st != CASIM_OK) return pw_refused(&c, n, var), finish(&c, SPARE, out, cap < SLOT ? cap : SLOT);
    /* the tree is done with: its arena goes to the search (show steps reads cr and ws here afterwards: do_ssteps) */
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

/* ---- show steps, 3: solve (work/steps-solve.md; design #6286, approved #6288) ----
   solve's own run, unchanged (the polynomial as typed, clear, put_solution), then a printer over what it leaves: the
   moved coefficients, the cleared polynomial, the factors and what is left. No records: every page runs solve and
   prints step k, the walk counting every step either way. */

typedef struct {
    int k, at, one;
    size_t cap;                 /* the caller's: a step line's bound (the answer keeps plain solve's) */
    casim_step_info *info;
} sstep_t;

/* the printer's room, in the arena: the typed text's comparison, what is left as each factor goes, the factors in
   the order they print, a factor's roots */
typedef struct {
    ipoly g, cur, q, before;
    ifac fs[SOLVE_DEG];
    iroot r[2];
    char t[SLOT];
} sswork;

/* whether this step is page k (counted either way); out starts afresh for it, but for the answer, already there */
static bool ss_hit(sstep_t *S, out_t *w, int depth, int line, int rule)
{
    if (line == CASIM_STEP_TEMPLATE && S->one) return false;
    if (S->at++ != S->k) return false;
    S->info->depth = depth, S->info->line = line, S->info->rule = rule;
    if (rule != CASIM_RULE_ANSWER) {
        w->cap = S->cap, w->n = 0, w->full = false;
        if (w->cap) w->o[0] = '\0';
    }
    return true;
}

/* co as a student writes it: descending powers, each coefficient a whole number or p÷q after its power */
static void ss_rpoly(out_t *w, const rat *co, int var)
{
    char name[2] = {(char)('A' + var), '\0'};
    bool any = false;
    for (int k = SOLVE_DEG; k >= 0; k--) {
        int64_t p = co[k].p, ap = p < 0 ? -p : p;
        if (!p) continue;
        if (p < 0) puts_(w, "-");
        else if (any) puts_(w, "+");
        if (ap != 1 || k == 0) put_int(w, ap);
        if (k > 0) {
            if (ap != 1) puts_(w, TIMES);
            puts_(w, name);
            if (k > 1) puts_(w, "^"), put_int(w, k);
        }
        if (co[k].q != 1) puts_(w, DIVIDE), put_int(w, co[k].q);
        any = true;
    }
    if (!any) puts_(w, "0");
}

/* p the same way, in parentheses when paren and it has two terms or more */
static __attribute__((noinline)) void ss_ipoly(out_t *w, const ipoly *p, int var, bool paren)
{
    rat r[SOLVE_DEG + 1];
    int terms = 0;
    for (int k = 0; k <= SOLVE_DEG; k++) r[k].p = k <= p->n ? p->a[k] : 0, r[k].q = 1, terms += r[k].p != 0;
    if (paren && terms > 1) puts_(w, "(");
    ss_rpoly(w, r, var);
    if (paren && terms > 1) puts_(w, ")");
}

static void ss_fac_poly(const ifac *x, ipoly *p)
{
    if (x->deg == 1) p->n = 1, p->a[1] = x->a, p->a[0] = -x->b;
    else p->n = 2, p->a[2] = x->a, p->a[1] = x->b, p->a[0] = x->c;
}

/* the order (#6288, D1): linear factors ascending by root (p/q: |p|, q <= 10^7, so the products are exact), then
   quadratics ascending by (a, b, c) */
static bool ss_before(const ifac *x, const ifac *y)
{
    if (x->deg != y->deg) return x->deg < y->deg;
    if (x->deg == 1) return x->b * y->a < y->b * x->a;
    return x->a != y->a ? x->a < y->a : x->b != y->b ? x->b < y->b : x->c < y->c;
}

static bool ipmul(ipoly *a, const ipoly *b);

/* whether p (degree >= 2, its leading coefficient positive) is r^k for a whole k >= 2, the largest k: r and k into
   *r and *k. Each coefficient of r is solved for from the top (p's at X^(n-j) is k·c^(k-1)·r_(m-j) plus what the
   ones above give), and r^k is multiplied back and compared: exact, false on any overflow. */
static __attribute__((noinline)) bool ss_power(const ipoly *p, ipoly *r, int *k)
{
    for (int kk = p->n; kk >= 2; kk--) {
        int m = p->n / kk;
        int64_t lo = 1, hi = p->a[p->n], c = 0, lead = kk, t;
        ipoly q;
        bool ok = true;
        if (p->n % kk) continue;
        /* c with c^kk the leading coefficient, by bisection (a power past it, or past int64, is too big) */
        while (lo <= hi && !c) {
            int64_t mid = lo + (hi - lo) / 2;
            bool big = false;
            t = 1;
            for (int i = 0; i < kk && !big; i++) big = __builtin_mul_overflow(t, mid, &t) || t > p->a[p->n];
            if (!big && t == p->a[p->n]) c = mid;
            else if (big) hi = mid - 1;
            else lo = mid + 1;
        }
        for (int i = 1; i < kk && c && ok; i++) ok = !__builtin_mul_overflow(lead, c, &lead);   /* k·c^(k-1) */
        if (!c || !ok) continue;
        memset(r, 0, sizeof *r);
        r->n = m;
        r->a[m] = c;
        for (int j = 1; j <= m && ok; j++) {
            int64_t d;
            q = *r;
            for (int i = 1; i < kk && ok; i++) ok = ipmul(&q, r);
            if (!ok || __builtin_sub_overflow(p->a[p->n - j], q.a[p->n - j], &d) || d % lead) ok = false;
            else r->a[m - j] = d / lead;
        }
        q = *r;
        for (int i = 1; i < kk && ok; i++) ok = ipmul(&q, r);
        if (ok && q.n == p->n && memcmp(q.a, p->a, (size_t)(p->n + 1) * sizeof p->a[0]) == 0) {
            *k = kk;
            return true;
        }
    }
    return false;
}

/* what is left when nothing more is divided out: a perfect power as (r)^k, which shows why its degree is left */
static void ss_left(out_t *w, const ipoly *rest, int var, bool paren)
{
    ipoly r;
    int k;
    if (rest->n >= 2 && ss_power(rest, &r, &k)) ss_ipoly(w, &r, var, true), puts_(w, "^"), put_int(w, k);
    else ss_ipoly(w, rest, var, paren);
}

/* X^zero × the first nf factors, each to its multiplicity, × rest (when it is not 1), = 0; last: nothing more is
   divided out of rest */
static void ss_product(out_t *w, int var, int zero, const ifac *fs, int nf, const ipoly *rest, bool last)
{
    char name[2] = {(char)('A' + var), '\0'};
    bool any = false;
    if (zero) {
        puts_(w, name);
        if (zero > 1) puts_(w, "^"), put_int(w, zero);
        any = true;
    }
    for (int i = 0; i < nf; i++) {
        ipoly p;
        ss_fac_poly(&fs[i], &p);
        if (any) puts_(w, TIMES);
        ss_ipoly(w, &p, var, true);
        if (fs[i].mult > 1) puts_(w, "^"), put_int(w, fs[i].mult);
        any = true;
    }
    if (rest->n >= 1) {
        if (any) puts_(w, TIMES);
        if (last) ss_left(w, rest, var, any);
        else ss_ipoly(w, rest, var, any);
    }
    puts_(w, "=0");
}

/* v, in parentheses when negative (a value in the discriminant's arithmetic) */
static void ss_val(out_t *w, int64_t v)
{
    if (v < 0) puts_(w, "(");
    put_int(w, v);
    if (v < 0) puts_(w, ")");
}

/* a quadratic factor = 0: the formula's values or the discriminant (template), then its roots */
static void ss_quadratic(sstep_t *S, out_t *w, sswork *W, const ifac *x, int var, bool solved, int nr)
{
    char name[2] = {(char)('A' + var), '\0'};
    int64_t d = x->b * x->b - 4 * x->a * x->c, k = 0, r = 1;
    ipoly p;
    ss_fac_poly(x, &p);
    if (d > 0) square_free(d, &k, &r);
    if (!solved) {                  /* not reached (abacus #5077): quad_roots refused a check, so solve leaves it */
        if (ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_LEFT))
            ss_ipoly(w, &p, var, false), puts_(w, "=0: DEGREE 2 LEFT");
        return;
    }
    /* the formula's values are its result already (d square-free, nothing to take out, once): one line */
    if (!(k == 1 && r > 1 && x->mult == 1) && ss_hit(S, w, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_QUADRATIC_FORMULA)) {
        ss_ipoly(w, &p, var, false), puts_(w, "=0: ");
        if (d < 0) {
            ss_val(w, x->b), puts_(w, "^2-4"), puts_(w, TIMES), ss_val(w, x->a), puts_(w, TIMES), ss_val(w, x->c);
            puts_(w, "="), put_int(w, d), puts_(w, "<0");
        } else
            for (int sg = -1; sg <= 1; sg += 2) {
                if (sg > 0) puts_(w, "; ");
                puts_(w, name), puts_(w, "=");
                if (x->b) puts_(w, "("), put_int(w, -x->b), puts_(w, sg < 0 ? "-" : "+");
                else if (sg < 0) puts_(w, "-");
                puts_(w, "SQRT("), put_int(w, d), puts_(w, x->b ? "))" : ")"), puts_(w, DIVIDE), put_int(w, 2 * x->a);
            }
    }
    if (ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_QUADRATIC_FORMULA)) {
        ss_ipoly(w, &p, var, false), puts_(w, "=0: ");
        if (nr == 0) puts_(w, "NO REAL ROOTS");
        for (int j = 0; j < nr; j++) puts_(w, j ? "; " : ""), puts_(w, name), puts_(w, "="), put_root(w, &W->r[j]);
    }
}

static __attribute__((noinline)) void ss_walk(ctx *c, out_t *w, sstep_t *S, int var, bool eq, const char *typed,
                                              const rat *co, const ipoly *f0, const solve_ws *s)
{
    char name[2] = {(char)('A' + var), '\0'};
    ref wr = grab(c, sizeof(sswork));
    sswork *W;
    int zero = 0, nf = s->nfac;
    bool same = true;
    if (!wr) return;
    W = (sswork *)(void *)nd(c, wr);
    {
        /* moved to one side and multiplied out, unless that is the text as typed */
        out_t t = {W->t, SLOT, 0, false};
        size_t n = strlen(typed);
        ss_rpoly(&t, co, var), puts_(&t, "=0");
        if (!t.full && strcmp(W->t, typed) != 0 && (eq || strncmp(W->t, typed, n) != 0 || strcmp(W->t + n, "=0") != 0) &&
            ss_hit(S, w, 0, CASIM_STEP_RESULT, eq ? CASIM_RULE_BOTH_SIDES : CASIM_RULE_POLYNOMIAL))
            puts_(w, W->t);
        else if (t.full && ss_hit(S, w, 0, CASIM_STEP_RESULT, eq ? CASIM_RULE_BOTH_SIDES : CASIM_RULE_POLYNOMIAL))
            ss_rpoly(w, co, var), puts_(w, "=0");
    }
    if (f0->n >= 1) {
        for (int k = 0; k <= SOLVE_DEG; k++)
            if (co[k].q != 1 || co[k].p != (k <= f0->n ? f0->a[k] : 0)) same = false;
        if (!same && ss_hit(S, w, 0, CASIM_STEP_TEMPLATE, CASIM_RULE_INTEGER_COEFFICIENTS)) {
            rat m, l;
            if (!rat_make(&m, co[f0->n].q, co[f0->n].p) || !rat_make(&l, f0->a[f0->n], 1) || !rat_mul(&m, &m, &l)) {
                c->st = CASIM_NO_ROOM;              /* f0 = co × m, m too big for a rat */
                return;
            }
            puts_(w, "("), ss_rpoly(w, co, var), puts_(w, ")");
            if (m.q == 1) puts_(w, TIMES), ss_val(w, m.p);
            else if (m.p == 1) puts_(w, DIVIDE), put_int(w, m.q);
            else puts_(w, TIMES), puts_(w, "("), put_int(w, m.p), puts_(w, DIVIDE), put_int(w, m.q), puts_(w, ")");
            puts_(w, "=0");
        }
        if (!same && ss_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_INTEGER_COEFFICIENTS))
            ss_ipoly(w, f0, var, false), puts_(w, "=0");
        W->g = *f0;
        while (W->g.n >= 1 && W->g.a[0] == 0)
            memmove(W->g.a, W->g.a + 1, (size_t)W->g.n * sizeof W->g.a[0]), W->g.n--, zero++;
        if (zero && W->g.n >= 1 && ss_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_COMMON_FACTOR))
            ss_product(w, var, zero, W->fs, 0, &W->g, nf == 0);
        memcpy(W->fs, s->fac, (size_t)nf * sizeof W->fs[0]);
        for (int i = 1; i < nf; i++) {
            ifac t = W->fs[i];
            int j = i;
            while (j > 0 && ss_before(&t, &W->fs[j - 1])) W->fs[j] = W->fs[j - 1], j--;
            W->fs[j] = t;
        }
        /* each factor divided out, to its multiplicity */
        W->cur = W->g;
        for (int i = 0; i < nf; i++) {
            const ifac *x = &W->fs[i];
            ipoly p;
            int rule = x->deg == 1 ? CASIM_RULE_LINEAR_FACTOR : CASIM_RULE_QUADRATIC_FACTOR;
            bool whole;
            ss_fac_poly(x, &p);
            whole = x->mult == 1 && W->cur.n == p.n && memcmp(W->cur.a, p.a, (size_t)(p.n + 1) * sizeof p.a[0]) == 0;
            W->before = W->cur;
            for (int m = 0; m < x->mult; m++) {
                if (!(x->deg == 1 ? div_lin(&W->cur, x->a, x->b, &W->q) : div_quad(&W->cur, x->a, x->b, x->c, &W->q))) {
                    c->st = CASIM_NO_ROOM;          /* not reached: solve divided each of them out */
                    return;
                }
                W->cur = W->q;
            }
            if (whole) continue;                    /* what was left is this factor: the line before shows it */
            if (ss_hit(S, w, 0, CASIM_STEP_TEMPLATE, rule)) {
                ss_ipoly(w, &W->before, var, true), puts_(w, DIVIDE), ss_ipoly(w, &p, var, true);
                if (x->mult > 1) puts_(w, "^"), put_int(w, x->mult);
                puts_(w, "=");
                if (i == nf - 1) ss_left(w, &W->cur, var, false);     /* the last: what is left as the next line has it */
                else ss_ipoly(w, &W->cur, var, false);
            }
            if (ss_hit(S, w, 0, CASIM_STEP_RESULT, rule)) ss_product(w, var, zero, W->fs, i + 1, &W->cur, i == nf - 1);
        }
        /* each factor = 0, in the same order */
        /* X^k = 0, k > 1 (X alone: the line above shows its root 0, and the answer has it) */
        if (zero > 1 && ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_LINEAR)) {
            puts_(w, name), puts_(w, "^"), put_int(w, zero), puts_(w, "=0: "), puts_(w, name);
            puts_(w, "=0 (MULT "), put_int(w, zero), puts_(w, ")");
        }
        for (int i = 0; i < nf; i++) {
            const ifac *x = &W->fs[i];
            int nr = 0;
            if (x->deg == 2) {
                bool solved = quad_roots(W->r, &nr, x->a, x->b, x->c, x->mult);
                ss_quadratic(S, w, W, x, var, solved, nr);
            } else if (ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_LINEAR)) {
                ipoly p;
                ss_fac_poly(x, &p);
                rational_root(W->r, &nr, x->b, x->a, x->mult);
                ss_ipoly(w, &p, var, false), puts_(w, "=0: "), puts_(w, name), puts_(w, "="), put_root(w, &W->r[0]);
            }
        }
        if (W->cur.n >= 3 && ss_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_LEFT))
            ss_left(w, &W->cur, var, false), puts_(w, "=0: DEGREE "), put_int(w, W->cur.n), puts_(w, " LEFT");
    }
    ss_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_ANSWER);
}

/* casim_step for solve, page P->k with P->flags into o: do_solve itself, its answer in a slot at the arena's top
   (plain solve's out and bound), then the walk over what it left in the arena: its coefficients and its factors, at
   the places it grabbed them */
/* do_ssteps keeps its five arguments in registers (the S3 passes six): GCC's IPA-SRA split them into more, two on the
   stack, and casim_step's frame grew 16 bytes; noipa stops it. clang has neither. */
#if defined(__clang__)
#define SS_KEEP_ARGS __attribute__((noinline))
#else
#define SS_KEEP_ARGS __attribute__((noipa))
#endif
static SS_KEEP_ARGS casim_status do_ssteps(const casim_req *rq, walk_t *P, out_t *o, void *arena,
                                                         size_t arena_cap)
{
    int k = P->k, flags = P->flags;
    casim_step_info *info = P->info;
    char *out = o->o;
    size_t cap = o->cap;
    ctx c;
    size_t room = arena_cap > SLOT ? arena_cap - SLOT : 0;
    char *ans = (char *)arena + room;
    casim_status st;
    ref cr, ws;
    ipoly f0;
    sstep_t S;
    if (!rq->eqn || !model_ok(rq->model) || rq->var < 0 || rq->var > 25 || rq->angle < CASIM_DEG ||
        rq->angle > CASIM_GRAD)
        return CASIM_BAD_ARG;
    if (cap == 0 && k >= 0) return CASIM_TOO_LONG;
    st = arena_cap < SLOT ? CASIM_NO_ROOM
                          : do_solve(rq->eqn, rq->var, rq->angle, ans, SLOT, arena, room, rq->model == CASIM_MODEL_STU);
    if (st == CASIM_NO_ROOM) {
        /* the slot took the room solve needed: plain solve's own run says whether it refuses (its status and out),
           or answers, and then only the steps don't fit */
        st = do_solve(rq->eqn, rq->var, rq->angle, out, cap, arena, arena_cap, rq->model == CASIM_MODEL_STU);
        if (cap) out[0] = st == CASIM_OK ? '\0' : out[0];
        return st == CASIM_OK ? CASIM_NO_ROOM : st;
    }
    if (st != CASIM_OK) {
        if (cap > strlen(ans)) memcpy(out, ans, strlen(ans) + 1);      /* what plain solve's out holds */
        return st;
    }
    start(&c, rq->eqn, rq->angle, arena, room);
    cr = held(&c, 2 * (SOLVE_DEG + 1) * sizeof(rat));
    ws = held(&c, sizeof(solve_ws));
    held(&c, SOLVE_DIVS * sizeof(int32_t));
    if (c.st != CASIM_OK || !clear(&c, (rat *)(void *)nd(&c, cr), &f0)) return CASIM_NO_ROOM;   /* not reached */
    S.k = k, S.at = 0, S.one = (flags & CASIM_STEPS_ONE_LINE) != 0, S.cap = cap, S.info = info;
    {
        out_t w = {out, cap, 0, false};
        /* an equation: its = (do_solve parsed it, so it is the one at the top) */
        ss_walk(&c, &w, &S, rq->var, strchr(rq->eqn, '=') != NULL, rq->eqn, (rat *)(void *)nd(&c, cr), &f0,
                (solve_ws *)(void *)nd(&c, ws));
        info->count = S.at;
        if (c.st != CASIM_OK) return c.st;
        if (k < 0) return CASIM_OK;
        if (k >= S.at) return CASIM_BAD_ARG;
        if (info->rule == CASIM_RULE_ANSWER) {
            w.n = 0, w.full = false;
            puts_(&w, ans);
        }
        if (w.full) return out[0] = '\0', CASIM_TOO_LONG;
    }
    return CASIM_OK;
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

/* Integrate's steps (work/steps-integrate.md): after integrate has answered, a walk of the typed text that takes the
   same decisions (irec_sum, irec_term, irec_factor below) records each step in order: the typed subtree it applies
   to, its rule, its line, the sign of a divisor and the call that gives its result. That call runs again when the
   step is printed, so no step keeps an answer, and integrate itself is unchanged with steps off. The records sit at
   the arena's top, which is free once the text is parsed. */
enum { VIA_TERM, VIA_FACTOR, VIA_SUM, VIA_MONO };    /* VIA_MONO: a monomial as integ_term's powers of X take it */

typedef struct {
    ref prev, before;
    uint8_t rule, depth, flags, pad;    /* flags: 1 a result line, 2 a divisor, the call in 4 and 8 */
} istep_t;

static istep_t *istep_at(const ctx *c, ref s)
{
    return (istep_t *)(void *)(c->mem + (size_t)s * 8u);
}

static __attribute__((noinline)) void irecord(ctx *c, int depth, int rule, int line, ref before, int s, int via)
{
    ref at;
    if (c->st != CASIM_OK) return;
    if (c->used + 8 > c->top) {
        c->st = CASIM_NO_ROOM;
        return;
    }
    c->top -= 8;
    at = (ref)(c->top / 8u);
    istep_at(c, at)->prev = c->step_last;
    istep_at(c, at)->before = before;
    istep_at(c, at)->rule = (uint8_t)rule;
    istep_at(c, at)->depth = (uint8_t)depth;
    istep_at(c, at)->flags = (uint8_t)((line == CASIM_STEP_RESULT) | (s < 0) << 1 | via << 2);
    istep_at(c, at)->pad = 0;
    c->step_last = at;
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

static casim_status do_irational(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t arena_cap,
                                 bool rtl);
static casim_status do_iparts(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t arena_cap,
                              bool rtl);

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

/* CAS 013: when the table refuses, the integrand as a rational function (the hook here, not in do_integrate, keeps its
   frame where it was) */
static casim_status do_partfrac(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t arena_cap,
                                bool rtl);

/* One row of ir_cancels, whether its removed value is a hole: what is left (partfrac's answer, in ir_cancels' first
   slot) has a value there. "X=a" is tried as it is. A "p=0" row (a removed factor with no rational root) is no hole
   where it has no real root (abacus #8423: solve's NO REAL ROOTS, as X^2+6), and counts otherwise, as rule 8 did (its
   real roots are irrational, and the search answered none of the shapes tried there). solve's line goes in a third
   slot, below ir_cancels' two */
static __attribute__((noinline)) bool ir_hole(const char *row, int var, int angle, void *arena, size_t arena_cap,
                                              bool rtl)
{
    char *slot = (char *)arena + arena_cap - 2 * SLOT, *val = slot + SLOT;
    if (row[0] == 'A' + var && row[1] == '=')
        return do_subst(slot, var, row + 2, angle, val, SLOT, arena, arena_cap - 2 * SLOT, false, false) != CASIM_UNDEFINED;
    return arena_cap <= 5 * SLOT ||
           do_solve(row, var, angle, slot - SLOT, SLOT, arena, arena_cap - 3 * SLOT, rtl) != CASIM_OK ||
           strncmp(slot - SLOT, "NO REAL ROOTS", 13) != 0;
}

/* CAS 013's rule 8, where it is a reason that holds for every method (abacus #8386): partfrac cancelled a common factor
   and a removed value is a hole, where what is left has a value (X^2-1 over (X-1)(X+2) at 1). A removed value that is
   still a pole ((7X-7) over (X-1)^2 at 1) is no hole, and substitution answers it (3.5×LN(ABS(X^2-2×X+1))). A row
   with no rational root (p=0) is a hole at a real root where what is left has a value (ir_hole). Asked in slots at
   the arena's top, whatever out's cap */
static __attribute__((noinline)) bool ir_cancels(const char *eqn, int var, int angle, void *arena, size_t arena_cap,
                                                  bool rtl)
{
    char *slot = (char *)arena + arena_cap - 2 * SLOT, *rows;
    if (arena_cap <= 4 * SLOT || do_partfrac(eqn, var, angle, slot, SLOT, arena, arena_cap - 2 * SLOT, rtl) != CASIM_OK ||
        !(rows = strchr(slot, ';')))
        return false;
    *rows = '\0';
    for (char *p = rows + 1; p; p = strchr(p + 1, ';')) {
        char *q = p + 1, *e;
        while (*q == ' ') q++;
        if ((e = strchr(q, ';'))) *e = '\0';
        if (ir_hole(q, var, angle, arena, arena_cap, rtl)) return true;
        if (e) *e = ';';
    }
    return false;
}

/* CAS 017 after the rational path refused, unless its rule 8 did (ir_cancels); out of line, so casim_integrate keeps
   its one call there (its frame moved 96 -> 112 with the test inline) */
static __attribute__((noinline)) casim_status ir_parts(const char *eqn, int var, int angle, char *out, size_t cap,
                                                       void *arena, size_t arena_cap, bool rtl)
{
    if (ir_cancels(eqn, var, angle, arena, arena_cap, rtl)) return CASIM_NOT_INTEGRABLE;
    return do_iparts(eqn, var, angle, out, cap, arena, arena_cap, rtl);
}

casim_status casim_integrate(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                             size_t arena_cap)
{
    casim_status st;
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    st = do_integrate(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
    if (st == CASIM_NOT_INTEGRABLE) st = do_irational(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
    /* CAS 017: when both refuse, substitution and parts */
    return st == CASIM_NOT_INTEGRABLE ? ir_parts(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU) : st;
}


/* canon_const's twin for the steps' walk and templates: a second caller of canon_const moved gcc's inlining of it in
   integ_checked (16 bytes off its frame on the S3), and integrate stays as it was with steps off. */
static __attribute__((noinline)) ref steps_const(ctx *c, ref r, int var)
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

/* The walk that records integrate's steps: integ's own decisions taken again on the typed text (integrate answered,
   so each branch here is one it took). own: the level records its result line (a factor that is its whole term
   leaves it to the term); rec false only classifies (a minus through a factor is one step). */
static int irec_term(ctx *c, ref t, int depth, bool checked, bool own, bool rec);

static ref bare(ctx *c, ref r, int *sign);

static int irec_sum(ctx *c, ref r0, int depth, bool own, bool rec)
{
    int sign, m = 0, rule;
    ref r = bare(c, r0, &sign), y = r;
    bool sum = nd(c, r)->kind == K_BADD || nd(c, r)->kind == K_BSUB;
    if (!sum && sign > 0) return irec_term(c, r, depth, true, own, rec);
    if (!enter(c)) return 0;
    rule = sum ? CASIM_RULE_SUM : CASIM_RULE_CONSTANT_MULTIPLE;
    if (rec) irecord(c, depth, rule, CASIM_STEP_TEMPLATE, r0, 1, VIA_SUM);
    if (!sum) irec_term(c, r, depth + 1, true, true, rec);          /* a minus through one term */
    else {
        while (nd(c, y)->kind == K_BADD || nd(c, y)->kind == K_BSUB) m++, y = nd(c, y)->a;
        irec_term(c, y, depth + 1, true, true, rec);
        for (int i = m - 1; i >= 0 && c->st == CASIM_OK; i--) {
            ref z = r;
            for (int j = 0; j < i; j++) z = nd(c, z)->a;
            irec_term(c, nd(c, z)->b, depth + 1, true, true, rec);
        }
    }
    if (rec && own) irecord(c, depth, rule, CASIM_STEP_RESULT, r0, 1, VIA_SUM);
    c->depth--;
    return rule;
}

/* a factor's rule, as integ_factor decides it; its substitution's template and its result recorded */
static int irec_factor(ctx *c, ref f0, int s, int depth, bool own, bool rec)
{
    int negs = 0, rule = CASIM_RULE_POLYNOMIAL;
    ref f = f0;
    const node *x;
    rat *lin = NULL, *p, n;
    bool done = false;
    while (nd(c, f)->kind == K_GROUP || nd(c, f)->kind == K_NEG) {
        if (nd(c, f)->kind == K_NEG) negs++;
        f = nd(c, f)->a;
    }
    x = nd(c, f);
    if (!enter(c)) return 0;
    if (negs % 2) {
        rule = irec_factor(c, f, s, depth, false, false);           /* a minus through: one step */
        if (rec && own) irecord(c, depth, rule, CASIM_STEP_RESULT, f0, s, VIA_FACTOR);
        c->depth--;
        return rule;
    }
    if (s > 0 && (x->kind == K_BADD || x->kind == K_BSUB)) {
        rule = irec_sum(c, f, depth, own, rec);                     /* a constant times a sum */
        c->depth--;
        return rule;
    }
    if (s > 0 && (x->kind == K_BMUL || x->kind == K_BDIV)) {
        rule = irec_term(c, f, depth, false, false, rec);           /* a product inside a minus or parentheses */
        if (rec && own) irecord(c, depth, rule, CASIM_STEP_RESULT, f0, s, VIA_FACTOR);
        c->depth--;
        return rule;
    }
    if (x->kind == K_RFN) {
        int id = x->id;
        done = true;
        if (id == F_SQRT || id == F_SQ || id == F_INV) {
            rat_int(&n, id == F_SQ ? 2 : -1);
            if (id == F_SQRT) rat_make(&n, 1, 2);
            if (s < 0) n.p = -n.p;
            if (linear(c, x->a, c->var, &lin)) rule = rat_is(&n, -1, 1) ? CASIM_RULE_RECIPROCAL : CASIM_RULE_POWER;
        } else if (linear(c, x->a, c->var, &lin)) rule = CASIM_RULE_FUNCTION + id;
    }
    if (!done && x->kind == K_BPOW && nd(c, x->a)->kind != K_BPOW) {
        rat *e;
        if (poly_of(c, x->b, c->var, &e) && pdeg(e) == 0 && linear(c, x->a, c->var, &lin)) {
            n = e[0];
            if (s < 0) n.p = -n.p;
            rule = rat_is(&n, -1, 1) ? CASIM_RULE_RECIPROCAL : CASIM_RULE_POWER;
            done = true;
        }
    }
    if (!done && s < 0 && linear(c, f, c->var, &lin)) rule = CASIM_RULE_RECIPROCAL, done = true;
    if (!done && poly_of(c, f, c->var, &p) && pdeg(p) == 1) rule = CASIM_RULE_POWER, lin = p;
    if (rec && lin && !(rat_is(&lin[1], 1, 1) && rat_is(&lin[0], 0, 1)))
        irecord(c, depth, rule, CASIM_STEP_TEMPLATE, f0, s, VIA_FACTOR);
    if (rec && own) irecord(c, depth, rule, CASIM_STEP_RESULT, f0, s, VIA_FACTOR);
    c->depth--;
    return rule;
}

/* a term's steps, as integ_term (and integ_checked when checked) take it */
static int irec_term(ctx *c, ref t, int depth, bool checked, bool own, bool rec)
{
    int rule, nv = 0, m = 0, s = 1;
    ref y = t, g = 0, k;
    if (!enter(c)) return 0;
    if (!raw_has_var(c, t, c->var)) rule = CASIM_RULE_CONSTANT;
    else {
        rat e;
        bool ns = false, cm;
        while (nd(c, y)->kind == K_BMUL || nd(c, y)->kind == K_BDIV) m++, y = nd(c, y)->a;
        if (raw_has_var(c, y, c->var)) nv++, g = y, s = 1;
        for (int i = m - 1; i >= 0; i--) {              /* the right sides, left to right; the last holding var */
            ref z = t;
            for (int j = 0; j < i; j++) z = nd(c, z)->a;
            if (raw_has_var(c, nd(c, z)->b, c->var)) nv++, g = nd(c, z)->b, s = nd(c, z)->kind == K_BDIV ? -1 : 1;
        }
        k = steps_const(c, t, c->var);
        cm = !is_k(c, k, 1, 1);
        if (nv == 1 && !mono_pow(c, g, c->var, &e, &ns)) {
            if (cm) {
                if (rec) irecord(c, depth, CASIM_RULE_CONSTANT_MULTIPLE, CASIM_STEP_TEMPLATE, t, 1, VIA_TERM);
                irec_factor(c, g, s, depth + 1, true, rec);
                rule = CASIM_RULE_CONSTANT_MULTIPLE;
            } else rule = irec_factor(c, g, s, depth, false, rec);
        } else if (nv == 1) {
            int r1;
            if (s < 0) e.p = -e.p;
            r1 = rat_is(&e, -1, 1) ? CASIM_RULE_RECIPROCAL : CASIM_RULE_POWER;
            if (cm) {
                if (rec) irecord(c, depth, CASIM_RULE_CONSTANT_MULTIPLE, CASIM_STEP_TEMPLATE, t, 1, VIA_TERM);
                if (rec) irecord(c, depth + 1, r1, CASIM_STEP_RESULT, g, s, VIA_MONO);
                rule = CASIM_RULE_CONSTANT_MULTIPLE;
            } else rule = r1;
        } else {
            if (rec) irecord(c, depth, CASIM_RULE_POLYNOMIAL, CASIM_STEP_TEMPLATE, t, 1, VIA_TERM);
            rule = CASIM_RULE_POLYNOMIAL;
        }
    }
    if (rec && checked) irecord(c, depth, CASIM_RULE_CHECK, CASIM_STEP_RESULT, t, 1, VIA_TERM);
    if (rec && own) irecord(c, depth, rule, CASIM_STEP_RESULT, t, 1, VIA_TERM);
    c->depth--;
    return rule;
}

/* ---- integrate's steps (work/steps-integrate.md): printed from the records, each result its call run again ---- */

/* A typed subtree as typed: numbers as written, the operators between, parentheses where the tree needs them (a
   lower operation inside a higher, the right side of - and ÷ holding its own kind, a minus not leading a sum, either
   side of a power unless it is a single item). The left side of every integrate step, so the student sees what
   they wrote; answers stay in canonical form. */
static int raw_level(const ctx *c, ref r)
{
    switch (nd(c, r)->kind) {
    case K_BADD:
    case K_BSUB: return 1;
    case K_BMUL:
    case K_BDIV: return 2;
    case K_NEG: return 3;
    case K_BPOW: return 4;
    default: return 5;
    }
}

static void raw_print(ctx *c, out_t *w, ref r, int min);

static __attribute__((noinline)) void raw_side(ctx *c, out_t *w, ref r, int min, bool lead)
{
    bool paren = raw_level(c, r) < min || (nd(c, r)->kind == K_NEG && !lead);
    if (paren) puts_(w, "(");
    raw_print(c, w, r, paren ? 0 : min);
    if (paren) puts_(w, ")");
}

static void raw_print(ctx *c, out_t *w, ref r, int min)
{
    const node *x = nd(c, r);
    static const char *const OP[] = {"+", "-", TIMES, DIVIDE, "^"};
    (void)min;
    if (!enter(c)) return;
    switch (x->kind) {
    case K_BADD:
    case K_BSUB:
    case K_BMUL:
    case K_BDIV:
    case K_BPOW: {
        int lv = raw_level(c, r), op = x->kind - K_BADD;
        raw_side(c, w, x->a, lv == 4 ? 5 : lv, lv == 1);
        puts_(w, OP[op]);
        raw_side(c, w, nd(c, r)->b, lv == 4 ? 5 : lv + 1, false);
        break;
    }
    case K_NEG:
        puts_(w, "-");
        raw_side(c, w, x->a, 3, false);
        break;
    case K_GROUP:
        puts_(w, "(");
        raw_print(c, w, x->a, 0);
        puts_(w, ")");
        break;
    case K_RFN:
        puts_(w, FNAME[x->id]);
        puts_(w, "(");
        raw_print(c, w, x->a, 0);
        puts_(w, ")");
        break;
    case K_LIT:
    case K_VAR:
    case K_PI: print(c, w, r); break;
    default: print(c, w, canon(c, r)); break;          /* an opaque name or function: its canonical text */
    }
    c->depth--;
}

/* ∫(t)dX for a typed t, as typed (a divisor's as 1÷t) */
static void put_raw_integral(ctx *c, out_t *w, ref t, bool divisor)
{
    char d[3] = {'d', (char)('A' + c->var), '\0'};
    puts_(w, "\xe2\x88\xab(");
    if (divisor) {
        puts_(w, "1" DIVIDE);
        raw_side(c, w, t, 5, false);
    } else raw_print(c, w, t, 0);
    puts_(w, ")");
    puts_(w, d);
}

static void put_integral(ctx *c, out_t *w, ref t)
{
    char d[3] = {'d', (char)('A' + c->var), '\0'};
    puts_(w, "\xe2\x88\xab(");
    print(c, w, t);
    puts_(w, ")");
    puts_(w, d);
}

/* the call that gave the record's result, run again (steps off) */
static ref irecompute(ctx *c, const istep_t *t)
{
    const rat *poly = NULL;
    int via = t->flags >> 2 & 3;
    if (via == VIA_SUM) return integ(c, t->before, c->var);
    if (via == VIA_MONO) {
        /* X^e (SQRT(X)^3, INV(X^2): rule 6a) as integ_term's polynomial path integrates it: integ_factor would refuse
           (abacus #6252) */
        rat e, *one;
        bool ns = false;
        ref tr = grab(c, (SOLVE_DEG + 1) * sizeof(rat));
        if (!tr || !mono_pow(c, t->before, c->var, &e, &ns)) return SPARE;
        one = (rat *)(void *)nd(c, tr);
        pzero(one), rat_int(&one[0], 1);
        if (t->flags & 2) e.p = -e.p;
        return e.p ? integ_poly_r(c, one, &e, c->var) : integ_poly(c, one, c->var);
    }
    if (via == VIA_TERM) return integ_term(c, t->before, c->var, &poly);
    return integ_factor(c, t->before, t->flags & 2 ? -1 : 1, c->var);
}

/* a typed subtree with its groups and minuses taken off; *sign the minuses' sign */
static ref bare(ctx *c, ref r, int *sign)
{
    *sign = 1;
    while (nd(c, r)->kind == K_GROUP || nd(c, r)->kind == K_NEG) {
        if (nd(c, r)->kind == K_NEG) *sign = -*sign;
        r = nd(c, r)->a;
    }
    return r;
}

/* SUM's template: each term's integral, with its sign, as integ takes them */
static void put_sum_template(ctx *c, out_t *w, ref r0)
{
    int sign, m = 0;
    ref r = bare(c, r0, &sign), y = r;
    if (nd(c, r)->kind != K_BADD && nd(c, r)->kind != K_BSUB) {     /* a minus through one term */
        puts_(w, "-");
        put_raw_integral(c, w, r, false);
        return;
    }
    while (nd(c, y)->kind == K_BADD || nd(c, y)->kind == K_BSUB) m++, y = nd(c, y)->a;
    if (sign < 0) puts_(w, "-");
    put_raw_integral(c, w, y, false);
    for (int i = m - 1; i >= 0; i--) {
        ref z = r;
        for (int j = 0; j < i; j++) z = nd(c, z)->a;
        puts_(w, (nd(c, z)->kind == K_BSUB ? -sign : sign) < 0 ? "-" : "+");
        put_raw_integral(c, w, nd(c, z)->b, false);
    }
}

/* CONSTANT MULTIPLE's template: k×∫(g)dX, g the term's one factor holding var */
static void put_cm_template(ctx *c, out_t *w, ref t)
{
    ref k = steps_const(c, t, c->var), z = t, g = 0;
    int s = 1;
    while (nd(c, z)->kind == K_BMUL || nd(c, z)->kind == K_BDIV) {
        if (raw_has_var(c, nd(c, z)->b, c->var)) g = nd(c, z)->b, s = nd(c, z)->kind == K_BDIV ? -1 : 1;
        z = nd(c, z)->a;
    }
    if (raw_has_var(c, z, c->var)) g = z, s = 1;
    if (!g) return;
    put_whole(c, w, k, 2);
    puts_(w, TIMES);
    put_raw_integral(c, w, g, s < 0);
}

/* a substitution's template: ∫(H(U))dU÷a, U=aX+b (U, or V when var is U) */
static void put_subst_template(ctx *c, out_t *w, const istep_t *t)
{
    int sign, uid = c->var == 'U' - 'A' ? 'V' - 'A' : 'U' - 'A', id = -1;
    ref f = bare(c, t->before, &sign), u = f, U = leaf(c, K_VAR, uid), H;
    const node *x = nd(c, f);
    rat *lin, n;
    char d[3] = {'d', (char)('A' + uid), '\0'};
    rat_int(&n, t->flags & 2 ? -1 : 1);
    if (x->kind == K_RFN) {
        id = x->id, u = x->a;
        if (id == F_SQRT || id == F_SQ || id == F_INV) {
            if (id == F_SQRT) rat_make(&n, t->flags & 2 ? -1 : 1, 2);
            else rat_int(&n, (id == F_SQ ? 2 : -1) * (t->flags & 2 ? -1 : 1));
            id = -1;
        }
    } else if (x->kind == K_BPOW) {
        rat *e;
        u = x->a;
        if (!poly_of(c, x->b, c->var, &e)) return;
        n = e[0];
        if (t->flags & 2) n.p = -n.p;
    }
    if (!linear(c, u, c->var, &lin)) return;
    H = id >= 0 ? fn(c, id, U) : power(c, U, num(c, &n));
    if (rat_is(&lin[1], -1, 1)) puts_(w, "-");              /* ÷(-1) as a minus */
    puts_(w, "\xe2\x88\xab(");
    print(c, w, H);
    puts_(w, ")");
    puts_(w, d);
    if (!rat_is(&lin[1], 1, 1) && !rat_is(&lin[1], -1, 1)) {
        puts_(w, DIVIDE);
        put_whole(c, w, num(c, &lin[1]), 5);
    }
    puts_(w, ", ");
    puts_(w, d + 1);
    puts_(w, "=");
    print(c, w, canon(c, u));
}

/* step k's text */
static __attribute__((noinline)) void put_istep(ctx *c, out_t *w, const istep_t *t)
{
    ref F;
    if (t->rule == CASIM_RULE_CHECK) {          /* d/dX(F) = f */
        char d[6] = {'d', '/', 'd', (char)('A' + c->var), '(', '\0'};
        F = irecompute(c, t);
        puts_(w, d);
        print(c, w, F);
        puts_(w, ") = ");
        raw_print(c, w, t->before, 0);
        return;
    }
    put_raw_integral(c, w, t->before, t->flags & 2);
    puts_(w, " = ");
    if (t->flags & 1) {
        print(c, w, irecompute(c, t));
        return;
    }
    switch (t->rule) {
    case CASIM_RULE_SUM: put_sum_template(c, w, t->before); break;
    case CASIM_RULE_CONSTANT_MULTIPLE:
        if ((t->flags >> 2 & 3) == VIA_SUM) put_sum_template(c, w, t->before);
        else put_cm_template(c, w, t->before);
        break;
    case CASIM_RULE_POLYNOMIAL:
        c->sub_var = -1;
        put_integral(c, w, expand_fully(c, canon(c, t->before)));
        break;
    default: put_subst_template(c, w, t);
    }
}

/* the arena back to where parsing left it, the constants cached since dropped */
static void unparse(ctx *c, size_t parsed)
{
    c->used = parsed;
    for (int j = 0; j < SMALL; j++)
        if (c->small[j] >= parsed / 8u) c->small[j] = 0;
    if (c->pi >= parsed / 8u) c->pi = 0;
}

static casim_status irs_refused(const casim_req *rq, int k, casim_step_info *info, const out_t *o, ctx *tc);
static casim_status ib_steps(const casim_req *rq, int k, casim_step_info *info, char *out, size_t cap, void *arena,
                             size_t arena_cap);
static casim_status ib_walk(const casim_req *rq, int k, casim_step_info *info, char *out, size_t cap, void *arena,
                            size_t arena_cap);

static casim_status do_isteps(const casim_req *rq, int k, int flags, casim_step_info *info, char *out, size_t cap,
                              void *arena, size_t arena_cap)
{
    ctx c;
    ref n = SPARE, F = SPARE, at;
    out_t w = {out, cap, 0, false};
    int count = 0, i;
    size_t parsed;
    memset(info, 0, sizeof *info);
    if (cap) out[0] = '\0';
    if (!rq->eqn || !model_ok(rq->model) || rq->var < 0 || rq->var > 25 || rq->angle < CASIM_DEG ||
        rq->angle > CASIM_GRAD)
        return CASIM_BAD_ARG;
    if (cap == 0 && k >= 0) return CASIM_TOO_LONG;
    start(&c, rq->eqn, rq->angle, arena, arena_cap);
    c.rtl = rq->model == CASIM_MODEL_STU;
    c.opaque = true;
    c.var = rq->var;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;
    parsed = c.used;
    if (c.st == CASIM_OK) F = integ(&c, n, rq->var);       /* integrate itself: its answer, or its status */
    if (c.st == CASIM_OK && (F == SPARE || F == 0)) c.st = CASIM_NO_ROOM;
    /* CAS 013: the table refused, so the rational walk (k, the five arguments and this one expression measured to keep
       casim_step's frame at 240 on the S3; flags rides in info's line) */
    if (c.st != CASIM_OK)
        return c.st == CASIM_NOT_INTEGRABLE ? (info->line = flags, irs_refused(rq, k, info, &w, &c))
                                            : cap ? finish(&c, F, out, cap) : c.st;
    /* a step points only at the typed tree (below parsed) and its records (at the top): the run's nodes go, and the
       recording walk's after it, so each page's call runs again in the arena the run had, less 8 bytes a step */
    unparse(&c, parsed);
    irec_sum(&c, n, 0, true, true);
    unparse(&c, parsed);
    if (c.st != CASIM_OK) return c.st;
    for (at = c.step_last; at; at = istep_at(&c, at)->prev)
        count += !(flags & CASIM_STEPS_ONE_LINE) || istep_at(&c, at)->flags & 1;
    info->count = count;
    if (k < 0) return CASIM_OK;
    if (k >= count) return CASIM_BAD_ARG;
    /* the records link backwards: step k is count - 1 - k back from the last */
    for (at = c.step_last, i = count - 1;; at = istep_at(&c, at)->prev) {
        if ((flags & CASIM_STEPS_ONE_LINE) && !(istep_at(&c, at)->flags & 1)) continue;
        if (i-- == k) break;
    }
    {
        istep_t t = *istep_at(&c, at);          /* a copy: printing allocates, and nothing may move it, but be sure */
        info->depth = t.depth;
        info->line = t.flags & 1 ? CASIM_STEP_RESULT : CASIM_STEP_TEMPLATE;
        info->rule = t.rule;
        put_istep(&c, &w, &t);
    }
    if (c.st == CASIM_OK && w.full) c.st = CASIM_TOO_LONG;
    if (c.st != CASIM_OK && cap) out[0] = '\0';
    return c.st;
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
    pw_refused(&c, n, var);
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
    CASIM_FRAME_PLANT
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
            if (fi->n < 0) {
                /* 0÷den: all of den cancels (the gcd of 0 and den is den's), so 0 and den's rows (CAS 012, abacus
                   #6437: NO_ROOM here named a resource that had not run out) */
                puts_(&w, "0");
                goto rows;
            }
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
        rows: {
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

/* ---- partial fractions over Q (unit CAS 012) ---- */

/* n÷d's terms over d's factors over Q: the polynomial part, then for each factor f to the k a numerator of degree below
   deg f over each f^j, j = 1..k (rule 3: extended Euclid, then the digits in base f). The arithmetic runs only after
   the input's parse has returned (rule 11): do_partfrac reads, pf_work computes, so a 44-deep input never carries
   pf_work's frame. */

#define PF_N (SOLVE_DEG + 1)

/* A test build (tests/pfcheck_check.c) defines this to spoil a numerator before rule 7's check, which must then refuse:
   the check guards a computation that is right, so nothing else reaches it. */
#ifndef CASIM_PF_PLANT
#define CASIM_PF_PLANT(pool, used) ((void)0)
#endif

/* kept at the arena's top while factor runs below it: the input cleared, and factor's texts */
typedef struct {
    ipoly n, d;                 /* primitive integer polynomials */
    rat kn, kd;                 /* their contents: the input is (kn n) ÷ (kd d) */
    char dtext[SLOT], ftext[SLOT];
} pfkeep;

typedef struct {
    int64_t f[PF_N];            /* the factor, primitive with a positive lead (int64: 8 KB holds 12 of them) */
    int deg;
    int k;                      /* its multiplicity in d, after the cancelling */
    int gone;                   /* how many times the gcd took it out (rule 2) */
    int off;                    /* its numerators in the pool: over f^j, deg f coefficients from off + (j-1) deg f */
} pffac;

/* below the kept block, once factor is done with the arena */
typedef struct {
    rat n[PF_N], d[PF_N], poly[PF_N], r[PF_N], P[PF_N], Q[PF_N], A[PF_N], t[8][PF_N];
    pffac fac[SOLVE_DEG];
    rat pool[SOLVE_DEG];        /* every numerator's coefficients: there are deg d of them */
    int nf;
} pfwork;

/* o += a b (sub: o -= a b); false on an overflow. One call for every product the section sums: at -O2 the rationals'
   add and multiply would otherwise be inlined at each of its loops (CAS 012 rule 11, the flash) */
/* the rationals' make, multiply, add and compare, out of line for this section: inlined at its many call sites they
   cost kilobytes of flash (rule 11) */
static __attribute__((noinline)) bool pf_rmake(rat *r, int64_t p, int64_t q) { return rat_make(r, p, q); }
static __attribute__((noinline)) bool pf_rmul(rat *r, const rat *x, const rat *y) { return rat_mul(r, x, y); }
static __attribute__((noinline)) bool pf_radd(rat *r, const rat *x, const rat *y) { return rat_add(r, x, y); }
static __attribute__((noinline)) int pf_rcmp(const rat *x, const rat *y) { return rat_cmp(x, y); }
static __attribute__((noinline)) void pf_puts(out_t *w, const char *s) { puts_(w, s); }
static __attribute__((noinline)) void pf_int(out_t *w, int64_t v) { put_int(w, v); }

/* o = a, a polynomial */
static void pf_copy(rat *o, const rat *a)
{
    memmove(o, a, PF_N * sizeof(rat));
}

static __attribute__((noinline)) bool pf_fma(rat *o, const rat *a, const rat *b, bool sub)
{
    rat t;
    if (a->p == 0 || b->p == 0) return true;
    if (!pf_rmul(&t, a, b)) return false;
    if (sub) t.p = -t.p;
    return pf_radd(o, o, &t);
}

/* o = a b; false past degree 12 or on an overflow */
static bool pf_mul(rat *o, const rat *a, const rat *b)
{
    int da = rdeg(a), db = rdeg(b);
    pzero(o);
    if (da < 0 || db < 0) return true;
    if (da + db > SOLVE_DEG) return false;
    for (int i = 0; i <= da; i++)
        for (int j = 0; j <= db; j++)
            if (!pf_fma(&o[i + j], &a[i], &b[j], false)) return false;
    return true;
}

/* o = a - b (o may be a or b) */
static bool pf_sub(rat *o, const rat *a, const rat *b)
{
    rat one = {1, 1};
    for (int i = 0; i < PF_N; i++) {
        rat t = b[i];
        o[i] = a[i];
        if (!pf_fma(&o[i], &one, &t, true)) return false;
    }
    return true;
}

/* a = q b + r, deg r < deg b (b nonzero; r is not a or b) */
static bool pf_div(const rat *a, const rat *b, rat *q, rat *r)
{
    int na = rdeg(a), nb = rdeg(b);
    rat inv;
    if (nb < 0) return false;
    pzero(q);
    memmove(r, a, PF_N * sizeof(rat));
    if (!pf_rmake(&inv, b[nb].q, b[nb].p)) return false;
    for (int i = na; i >= nb; i--) {
        if (r[i].p == 0) continue;
        if (!pf_rmul(&q[i - nb], &r[i], &inv)) return false;
        for (int j = 0; j <= nb; j++) {
            if (!pf_fma(&r[i - nb + j], &q[i - nb], &b[j], true)) return false;
        }
    }
    return true;
}

/* o = a b mod P for deg b < deg P, by Horner in a's coefficients: o X mod P, plus a_i b; every step stays below deg P,
   so a degree-11 remainder times an inverse never passes 12 (o is not a or b) */
static bool pf_mulmod(rat *o, const rat *a, const rat *b, const rat *P)
{
    int np = rdeg(P);
    rat inv, lead;
    pzero(o);
    if (!pf_rmake(&inv, P[np].q, P[np].p)) return false;
    for (int i = rdeg(a); i >= 0; i--) {
        lead = o[np - 1];
        for (int x = np - 1; x > 0; x--) o[x] = o[x - 1];               /* o X, its X^np term in lead */
        rat_int(&o[0], 0);
        if (lead.p != 0) {
            if (!pf_rmul(&lead, &lead, &inv)) return false;
            for (int x = 0; x < np; x++) {                               /* minus lead/P_np P */
                if (!pf_fma(&o[x], &lead, &P[x], true)) return false;
            }
        }
        if (a[i].p != 0)
            for (int x = 0; x < np; x++) {
                if (!pf_fma(&o[x], &a[i], &b[x], false)) return false;
            }
    }
    return true;
}

/* s with s Q = 1 mod P and deg s < deg P, by extended Euclid; false when P and Q are not coprime. t: 6 polynomials */
static bool pf_inverse(const rat *Q, const rat *P, rat *s, rat (*t)[PF_N])
{
    rat *r0 = t[0], *r1 = t[1], *s0 = t[2], *s1 = t[3], *q = t[4], *x = t[5], inv;
    pf_copy(r0, P);
    if (!pf_div(Q, P, q, r1)) return false;
    pzero(s0), pzero(s1), rat_int(&s1[0], 1);
    while (rdeg(r1) > 0) {
        /* r1 and s1 made monic in r1 first: the invariant s Q = r mod P holds for both, and the numbers stay small */
        if (!pf_rmake(&inv, r1[rdeg(r1)].q, r1[rdeg(r1)].p)) return false;
        for (int i = 0; i < PF_N; i++)
            if ((r1[i].p && !pf_rmul(&r1[i], &r1[i], &inv)) || (s1[i].p && !pf_rmul(&s1[i], &s1[i], &inv))) return false;
        if (!pf_div(r0, r1, q, x)) return false;
        pf_copy(r0, r1), pf_copy(r1, x);
        if (!pf_mul(x, q, s1) || !pf_sub(x, s0, x)) return false;
        pf_copy(s0, s1), pf_copy(s1, x);
    }
    if (rdeg(r1) < 0 || !pf_rmake(&inv, r1[0].q, r1[0].p)) return false;
    for (int i = 0; i < PF_N; i++)
        if (s1[i].p && !pf_rmul(&s1[i], &s1[i], &inv)) return false;
    return pf_div(s1, P, q, s);
}

/* an integer polynomial, descending, as a student writes it */
static void pf_ipoly(out_t *w, const rat *p, int var)
{
    char v[2] = {(char)('A' + var), 0};
    bool first = true;
    for (int i = rdeg(p); i >= 0; i--) {
        int64_t a = p[i].p;
        if (a == 0) continue;
        if (a < 0) pf_puts(w, "-"), a = -a;
        else if (!first) pf_puts(w, "+");
        first = false;
        if (i == 0 || a != 1) pf_int(w, a);
        if (i > 0 && a != 1) pf_puts(w, TIMES);
        if (i > 0) pf_puts(w, v);
        if (i > 1) pf_puts(w, "^"), pf_int(w, i);
    }
    if (first) pf_puts(w, "0");
}

/* the polynomial part, descending, each coefficient an integer or p÷q after its power (rule 4: 3×X÷2+9÷2) */
static void pf_rpoly(out_t *w, const rat *p, int var)
{
    char v[2] = {(char)('A' + var), 0};
    bool first = true;
    for (int i = rdeg(p); i >= 0; i--) {
        int64_t a = p[i].p;
        if (a == 0) continue;
        if (a < 0) pf_puts(w, "-"), a = -a;
        else if (!first) pf_puts(w, "+");
        first = false;
        if (i == 0 || a != 1) pf_int(w, a);
        if (i > 0 && a != 1) pf_puts(w, TIMES);
        if (i > 0) pf_puts(w, v);
        if (i > 1) pf_puts(w, "^"), pf_int(w, i);
        if (p[i].q != 1) pf_puts(w, DIVIDE), pf_int(w, p[i].q);
    }
}

/* num ÷ f^j as rule 4 writes it: c×(N)÷(m×(F)^j), N primitive with a positive lead, in parentheses only when it is a
   sum, the sign out front, c and m dropped when 1. ip: room for N. */
static bool pf_term(out_t *w, const rat *num, const rat *f, int j, int var, bool first, rat *ip)
{
    int64_t l = 1, g = 0;
    int dn = rdeg(num), terms = 0;
    rat k;
    for (int i = 0; i <= dn; i++)
        if (num[i].p != 0 && !mul64(&l, l, num[i].q / gcd64(l, num[i].q))) return false;
    pzero(ip);
    for (int i = 0; i <= dn; i++) {
        if (num[i].p != 0 && !mul64(&ip[i].p, num[i].p, l / num[i].q)) return false;
        g = gcd64(g, ip[i].p);
    }
    if (ip[dn].p < 0) g = -g;
    for (int i = 0; i <= dn; i++) ip[i].p /= g, terms += ip[i].p != 0;
    if (!pf_rmake(&k, g, l)) return false;                       /* num = k N */
    if (k.p < 0) pf_puts(w, "-"), k.p = -k.p;
    else if (!first) pf_puts(w, "+");
    if (dn == 0) pf_int(w, k.p);
    else {
        if (k.p != 1) pf_int(w, k.p), pf_puts(w, TIMES);
        if (terms > 1) pf_puts(w, "(");
        pf_ipoly(w, ip, var);
        if (terms > 1) pf_puts(w, ")");
    }
    pf_puts(w, DIVIDE);
    if (k.q != 1) pf_puts(w, "("), pf_int(w, k.q), pf_puts(w, TIMES);
    if (rdeg(f) == 1 && f[0].p == 0 && f[1].p == 1) pf_ipoly(w, f, var);         /* X alone */
    else pf_puts(w, "("), pf_ipoly(w, f, var), pf_puts(w, ")");
    if (j > 1) pf_puts(w, "^"), pf_int(w, j);
    if (k.q != 1) pf_puts(w, ")");
    return true;
}

/* numerator j of F, from the pool */
static void pf_get(const pfwork *W, const pffac *F, int j, rat *o)
{
    pzero(o);
    for (int i = 0; i < F->deg; i++) o[i] = W->pool[F->off + (j - 1) * F->deg + i];
}

/* F's factor as rationals */
static rat *pf_f(const pffac *F, rat *o)
{
    pzero(o);
    for (int i = 0; i <= F->deg; i++) o[i].p = F->f[i];
    return o;
}

/* rule 5's order: linear factors by root ascending (X is root 0), quadratics by (a, b, c), then the rest by degree,
   then by their coefficients from the lead down */
static int pf_cmp(const pffac *x, const pffac *y)
{
    if (x->deg != y->deg) return x->deg < y->deg ? -1 : 1;
    if (x->deg == 1) {
        rat rx, ry;
        pf_rmake(&rx, -x->f[0], x->f[1]), pf_rmake(&ry, -y->f[0], y->f[1]);
        return pf_rcmp(&rx, &ry);
    }
    for (int i = x->deg; i >= 0; i--)
        if (x->f[i] != y->f[i]) return x->f[i] < y->f[i] ? -1 : 1;
    return 0;
}

/* factor's printed product, read back into W's factors: a chain of × of F, (F) or (F)^k. co: room for one polynomial */
static bool pf_factors(ctx *c, ref t, int var, pfwork *W, rat *co)
{
    pffac *F;
    while (nd(c, t)->kind == K_GROUP) t = nd(c, t)->a;
    if (nd(c, t)->kind == K_BMUL)
        return pf_factors(c, nd(c, t)->a, var, W, co) && pf_factors(c, nd(c, t)->b, var, W, co);
    if (W->nf >= SOLVE_DEG) return false;
    F = &W->fac[W->nf++];
    F->k = 1;
    if (nd(c, t)->kind == K_BPOW) {
        pw(c, nd(c, t)->b, var, co);
        if (c->st != CASIM_OK || rdeg(co) != 0 || co[0].q != 1 || co[0].p < 2 || co[0].p > SOLVE_DEG) return false;
        F->k = (int)co[0].p;
        t = nd(c, t)->a;
    }
    pw(c, t, var, co);
    if (c->st != CASIM_OK || (F->deg = rdeg(co)) < 1 || co[F->deg].p < 0) return false;
    for (int i = 0; i <= SOLVE_DEG; i++) {
        if (co[i].q != 1) return false;                         /* factor's factors are integer polynomials */
        F->f[i] = co[i].p;
    }
    return true;
}

/* an ipoly as rationals, times k */
static bool pf_rats(rat *o, const ipoly *f, const rat *k)
{
    pzero(o);
    for (int i = 0; i <= f->n; i++) {
        rat_int(&o[i], 0);
        o[i].p = f->a[i];
        if (!pf_rmul(&o[i], &o[i], k)) return false;
    }
    return true;
}

/* everything after the input's parse: factor d, the numerators, the check (rule 7), the text */
static __attribute__((noinline)) casim_status pf_work(pfkeep *K, int var, int angle, char *out, size_t ocap, void *arena,
                                                      size_t room)
{
    ctx c;
    ref t, wr;
    out_t w = {out, ocap, 0, false};
    pfwork *W;
    casim_status fs = CASIM_OK;
    int used = 0;
    bool first = true;
    K->ftext[0] = '\0';
    if (K->d.n >= 1) {
        /* d (primitive, its lead positive) factored by factor, called whole: its code and frames are factor's */
        out_t dt = {K->dtext, SLOT, 0, false};
        rat *ip = (rat *)arena;                                  /* scratch until factor starts (8-aligned) */
        pzero(ip);
        for (int i = 0; i <= K->d.n; i++) ip[i].p = K->d.a[i];
        pf_ipoly(&dt, ip, var);
        if (dt.full) return CASIM_NO_ROOM;
        fs = do_factor(K->dtext, var, angle, K->ftext, SLOT, arena, room, false);
        if (fs != CASIM_OK && fs != CASIM_PARTIAL) return fs == CASIM_TOO_LONG ? CASIM_NO_ROOM : fs;
    }
    start(&c, K->ftext[0] ? K->ftext : "1", angle, arena, room);
    c.opaque = true;
    wr = grab(&c, sizeof(pfwork));
    t = c.st == CASIM_OK ? parse(&c) : SPARE;
    if (c.st != CASIM_OK) return CASIM_NO_ROOM;
    W = (pfwork *)(void *)nd(&c, wr);
    if (!pf_rats(W->n, &K->n, &K->kn) || !pf_rats(W->d, &K->d, &K->kd)) return CASIM_NO_ROOM;
    if (K->d.n >= 1) {
        if (!pf_factors(&c, t, var, W, W->t[0])) return CASIM_NO_ROOM;
        for (int i = 1; i < W->nf; i++)
            for (int j = i; j > 0 && pf_cmp(&W->fac[j - 1], &W->fac[j]) > 0; j--) {
                pffac x = W->fac[j];
                W->fac[j] = W->fac[j - 1], W->fac[j - 1] = x;
            }
        /* rule 2: the gcd out first, a factor at a time while it divides n exactly (0 is divided by all of d) */
        for (int i = 0; i < W->nf; i++) {
            pffac *F = &W->fac[i];
            rat *f = pf_f(F, W->t[7]);
            F->gone = 0;
            while (F->k > 0) {
                if (!pf_div(W->n, f, W->t[0], W->t[1])) return CASIM_NO_ROOM;
                if (rdeg(W->t[1]) >= 0) break;
                pf_copy(W->n, W->t[0]);
                if (!pf_div(W->d, f, W->t[0], W->t[1]) || rdeg(W->t[1]) >= 0) return CASIM_NO_ROOM;
                pf_copy(W->d, W->t[0]);
                F->k--, F->gone++;
            }
        }
    }
    if (!pf_div(W->n, W->d, W->poly, W->r)) return CASIM_NO_ROOM;
    if (rdeg(W->d) >= 1) {
        /* each factor power P = f^k against Q = d ÷ P: A = r s mod P with s Q = 1 mod P, then A's digits in base f */
        for (int i = 0; i < W->nf; i++) {
            pffac *F = &W->fac[i];
            int df = F->deg;
            rat *f = pf_f(F, W->t[7]);
            F->off = used;
            if (F->k == 0) continue;                             /* all of it cancelled */
            pzero(W->P), rat_int(&W->P[0], 1);
            for (int m = 0; m < F->k; m++) {
                if (!pf_mul(W->A, W->P, f)) return CASIM_NO_ROOM;
                pf_copy(W->P, W->A);
            }
            if (!pf_div(W->d, W->P, W->Q, W->A) || rdeg(W->A) >= 0) return CASIM_NO_ROOM;
            if (!pf_inverse(W->Q, W->P, W->t[6], W->t) || !pf_mulmod(W->A, W->r, W->t[6], W->P)) return CASIM_NO_ROOM;
            if (used + F->k * df > SOLVE_DEG) return CASIM_NO_ROOM;
            for (int j = F->k; j >= 1; j--) {
                if (!pf_div(W->A, f, W->t[0], W->t[1])) return CASIM_NO_ROOM;
                pf_copy(W->A, W->t[0]);
                for (int x = 0; x < df; x++) W->pool[used + (j - 1) * df + x] = W->t[1][x];
            }
            used += F->k * df;
        }
        CASIM_PF_PLANT(W->pool, used);
    }
    /* rule 7, before anything prints, for every answer: n = poly d + Σ num_j d ÷ f^j exactly. Checked in an equivalent
       form whose numbers stay small (the sum's terms have unrelated denominators, and summed they pass 2^62 where the
       answer does not): n = poly d + r, and for each factor power P = f^k with cofactor Q = d ÷ P, the numerators
       rebuilt as A = Σ num_j f^(k-j) satisfy A Q = r mod P. Then r - Σ A Q is divisible by every P, so by d, and its
       degree is below d's: it is 0. */
    if (!pf_mul(W->t[1], W->poly, W->d)) return CASIM_NO_ROOM;
    for (int x = 0; x < PF_N; x++)
        if (!pf_radd(&W->t[1][x], &W->t[1][x], &W->r[x]) || pf_rcmp(&W->t[1][x], &W->n[x]) != 0) return CASIM_NO_ROOM;
    for (int i = 0; i < W->nf; i++) {
        pffac *F = &W->fac[i];
        rat *f = pf_f(F, W->t[7]);
        if (F->k == 0) continue;
        pzero(W->P), rat_int(&W->P[0], 1), pzero(W->A);
        for (int j = 1; j <= F->k; j++) {
            if (!pf_mul(W->t[0], W->P, f) || !pf_mul(W->t[1], W->A, f)) return CASIM_NO_ROOM;
            pf_copy(W->P, W->t[0]);
            pf_get(W, F, j, W->t[6]);
            for (int x = 0; x < PF_N; x++) W->t[6][x].p = -W->t[6][x].p;
            if (!pf_sub(W->A, W->t[1], W->t[6])) return CASIM_NO_ROOM;            /* A = A f + num_j */
        }
        if (!pf_div(W->d, W->P, W->Q, W->t[2]) || rdeg(W->t[2]) >= 0 || !pf_div(W->Q, W->P, W->t[0], W->t[3]) ||
            !pf_mulmod(W->t[4], W->A, W->t[3], W->P) || !pf_div(W->r, W->P, W->t[0], W->t[5]))
            return CASIM_NO_ROOM;
        for (int x = 0; x < PF_N; x++)
            if (pf_rcmp(&W->t[4][x], &W->t[5][x]) != 0) return CASIM_NO_ROOM;     /* never printed unchecked */
    }
    /* rule 4's text: the polynomial part, then each factor's terms by ascending power */
    if (rdeg(W->poly) >= 0) pf_rpoly(&w, W->poly, var), first = false;
    for (int i = 0; i < W->nf; i++)
        for (int j = 1; j <= W->fac[i].k; j++) {
            pf_get(W, &W->fac[i], j, W->t[1]);
            if (rdeg(W->t[1]) < 0) continue;
            if (!pf_term(&w, W->t[1], pf_f(&W->fac[i], W->t[7]), j, var, first, W->t[0])) return out[0] = '\0', CASIM_NO_ROOM;
            first = false;
        }
    if (first) pf_puts(&w, "0");
    /* rule 2: CANCEL's rows for what the gcd removed: X=a for a linear factor, ascending (rule 5's order is by root),
       then the rest as one polynomial, as CANCEL prints it */
    {
        ipoly *rest = &K->n, b;                                  /* n is done with */
        char v[2] = {(char)('A' + var), 0};
        rest->n = 0, rest->a[0] = 1;
        for (int i = 0; i < W->nf; i++) {
            pffac *F = &W->fac[i];
            if (F->gone == 0) continue;
            if (F->deg == 1) {
                pf_puts(&w, "; "), pf_puts(&w, v), pf_puts(&w, "=");
                pf_int(&w, -F->f[0]);
                if (F->f[1] != 1) pf_puts(&w, DIVIDE), pf_int(&w, F->f[1]);
                continue;
            }
            b.n = F->deg;
            for (int x = 0; x <= b.n; x++) b.a[x] = F->f[x];
            for (int m = 0; m < F->gone; m++)
                if (!ipmul(rest, &b)) return out[0] = '\0', CASIM_NO_ROOM;
        }
        if (rest->n >= 1) {
            pf_puts(&w, "; ");
            print(&c, &w, ipoly_tree(&c, rest, var));
            pf_puts(&w, "=0");
        }
    }
    if (c.st != CASIM_OK) return out[0] = '\0', CASIM_NO_ROOM;
    if (w.full) return out[0] = '\0', CASIM_TOO_LONG;
    return fs;
}

/* the reading: n÷d as CANCEL reads it (rule 1), cleared to the kept block at the arena's top; then pf_work */
static __attribute__((noinline)) casim_status do_partfrac(const char *eqn, int var, int angle, char *out, size_t cap,
                                                          void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref n = SPARE, t;
    size_t ocap = cap < SLOT ? cap : SLOT, room, skip = (size_t)(-(uintptr_t)arena & 7u);
    unsigned char *base = (unsigned char *)arena + skip;         /* the arena may have any alignment */
    pfkeep *K;
    rat *co;
    if (cap) out[0] = '\0';
    if (!eqn) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    if (arena_cap < skip + sizeof(pfkeep) + sizeof(pfwork) + 64) return out[0] = '\0', CASIM_NO_ROOM;
    room = (arena_cap - skip - sizeof(pfkeep)) & ~(size_t)7;
    K = (pfkeep *)(void *)(base + room);
    start(&c, eqn, angle, base, room);
    c.rtl = rtl;
    c.opaque = true;
    co = (rat *)(void *)nd(&c, grab(&c, PF_N * sizeof(rat)));
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;          /* an equation */
    if (c.st != CASIM_OK || cap == 0) return finish(&c, SPARE, out, ocap);
    for (t = n; nd(&c, t)->kind == K_GROUP;) t = nd(&c, t)->a;
    /* num÷den, as typed: the top division's two sides (no division: den is 1) */
    pw(&c, nd(&c, t)->kind == K_BDIV ? nd(&c, t)->a : t, var, co);
    if (c.st != CASIM_OK || !clear(&c, co, &K->n)) return finish(&c, SPARE, out, ocap);
    if (K->n.n < 0) rat_int(&K->kn, 0), K->n.n = 0, K->n.a[0] = 1;
    else if (!pf_rmake(&K->kn, 1, K->n.a[K->n.n]) || !pf_rmul(&K->kn, &co[K->n.n], &K->kn))
        return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
    if (nd(&c, t)->kind == K_BDIV) pw(&c, nd(&c, t)->b, var, co);
    else pzero(co), rat_int(&co[0], 1);
    if (c.st != CASIM_OK || !clear(&c, co, &K->d)) return finish(&c, SPARE, out, ocap);
    if (K->d.n < 0) return c.st = CASIM_UNDEFINED, finish(&c, SPARE, out, ocap);   /* ÷0 */
    if (!pf_rmake(&K->kd, 1, K->d.a[K->d.n]) || !pf_rmul(&K->kd, &co[K->d.n], &K->kd))
        return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
    return pf_work(K, var, angle, out, ocap, base, room);
}

casim_status casim_partfrac(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                            size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_partfrac(eqn, var, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- partfrac's steps (work/steps-partfrac.md) ----
   casim_step for CASIM_PARTFRAC: do_partfrac itself, its answer in a slot at the arena's top, then a walk over what it
   left: its kept block and its work area, at the places it put them (held, not zeroed). A page's text is computed only
   for the page asked for; every page is counted on every call. */

typedef struct {
    int k, at;
    bool one;
    size_t cap;
    casim_step_info *info;
} pstep_t;

/* the printer's room, after partfrac's work area */
typedef struct {
    rat cof[PF_N], t[PF_N], u[PF_N];
    int letter[SOLVE_DEG];      /* each unknown's letter, 0-25 (the variable skipped) */
    int fi[SOLVE_DEG], fj[SOLVE_DEG], fe[SOLVE_DEG];     /* its factor, power, and the power of X it multiplies */
    rat val[SOLVE_DEG], cm[SOLVE_DEG];
    bool known[SOLVE_DEG];
    int nu;
} pswork;

static __attribute__((noinline)) bool ps_hit(pstep_t *S, out_t *w, int depth, int line, int rule)
{
    if (line == CASIM_STEP_TEMPLATE && S->one) return false;
    if (S->at++ != S->k) return false;
    S->info->depth = depth, S->info->line = line, S->info->rule = rule;
    if (rule != CASIM_RULE_ANSWER) {
        w->cap = S->cap, w->n = 0, w->full = false;
        if (w->cap) w->o[0] = '\0';
    }
    return true;
}

static __attribute__((noinline)) void ps_letter(out_t *w, int l)
{
    char v[2] = {(char)('A' + l), 0};
    pf_puts(w, v);
}

/* a rational as a student writes it: -p÷q */
static __attribute__((noinline)) void ps_rat(out_t *w, const rat *x)
{
    pf_int(w, x->p);
    if (x->q != 1) pf_puts(w, DIVIDE), pf_int(w, x->q);
}

/* p at a, exactly; false on an overflow */
static __attribute__((noinline)) bool ps_at(const rat *p, const rat *a, rat *v)
{
    rat x = {1, 1};
    rat_int(v, 0);
    for (int m = 0; m <= rdeg(p); m++)
        if (!pf_fma(v, &p[m], &x, false) || !pf_rmul(&x, &x, a)) return false;
    return true;
}

/* (F) or X, then ^j */
static void ps_fac(out_t *w, const pffac *F, int j, rat *tmp, int var)
{
    rat *f = pf_f(F, tmp);
    if (F->deg == 1 && F->f[0] == 0 && F->f[1] == 1) pf_ipoly(w, f, var);
    else pf_puts(w, "("), pf_ipoly(w, f, var), pf_puts(w, ")");
    if (j > 1) pf_puts(w, "^"), pf_int(w, j);
}

/* a polynomial in rule 4's polynomial-part spelling, or 0 */
static void ps_rpoly(out_t *w, const rat *p, int var)
{
    if (rdeg(p) < 0) pf_puts(w, "0");
    else pf_rpoly(w, p, var);
}

/* d = lc×F1^k1×F2^k2..., the factors with a power left */
static __attribute__((noinline)) void ps_factors(out_t *w, const pfwork *W, const rat *lc, rat *tmp, int var)
{
    bool first = true;
    ps_rpoly(w, W->d, var), pf_puts(w, "=");
    if (!(lc->p == 1 && lc->q == 1)) ps_rat(w, lc), pf_puts(w, TIMES);
    for (int i = 0; i < W->nf; i++)
        if (W->fac[i].k) {
            if (!first) pf_puts(w, TIMES);
            ps_fac(w, &W->fac[i], W->fac[i].k, tmp, var), first = false;
        }
}

/* p as one side of a quotient: in parentheses when it is a sum, or (as a divisor) a product */
static void ps_side(out_t *w, const rat *p, int var, bool divisor)
{
    int terms = 0, d = rdeg(p);
    for (int i = 0; i <= d; i++) terms += p[i].p != 0;
    bool bare = terms <= 1 && (d <= 0 ? p[0].q == 1 : (p[d].p == 1 && p[d].q == 1));
    bool paren = terms > 1 || (divisor && !bare);
    if (paren) pf_puts(w, "(");
    ps_rpoly(w, p, var);
    if (paren) pf_puts(w, ")");
}

/* "+p" after another term: a single negative term as "-|p|", anything else "+" and its side */
static __attribute__((noinline)) void ps_plus(out_t *w, const rat *p, int var)
{
    int terms = 0, d = rdeg(p);
    for (int i = 0; i <= d; i++) terms += p[i].p != 0;
    if (terms == 1 && p[d].p < 0) {
        rat *q = (rat *)p;                          /* negated in place and back */
        q[d].p = -q[d].p, pf_puts(w, "-"), ps_side(w, q, var, false), q[d].p = -q[d].p;
    } else pf_puts(w, "+"), ps_side(w, p, var, false);
}

/* c×, as a coefficient: nothing for 1, - for -1 */
static __attribute__((noinline)) void ps_times(out_t *w, const rat *c)
{
    if (c->p == 1 && c->q == 1) return;
    if (c->p == -1 && c->q == 1) pf_puts(w, "-");
    else ps_rat(w, c), pf_puts(w, TIMES);
}

/* the unknown numerator over F^j: U, or (U×X+V), (U×X^2+V×X+W) ... */
static void ps_numer(out_t *w, const pswork *P, int first, int deg, int var)
{
    char v[2] = {(char)('A' + var), 0};
    if (deg > 1) pf_puts(w, "(");
    for (int e = deg - 1, u = first; e >= 0; e--, u++) {
        if (u > first) pf_puts(w, "+");
        ps_letter(w, P->letter[u]);
        if (e >= 1) pf_puts(w, TIMES), pf_puts(w, v);
        if (e >= 2) pf_puts(w, "^"), pf_int(w, e);
    }
    if (deg > 1) pf_puts(w, ")");
}

/* the cofactor d ÷ f_i^j as its factors (lc left out): the other factors to their powers, then f_i^(k-j) */
static __attribute__((noinline)) bool ps_cofactor_text(out_t *w, const pfwork *W, int i, int j, rat *tmp, int var)
{
    bool any = false;
    for (int x = 0; x < W->nf; x++) {
        int p = x == i ? W->fac[x].k - j : W->fac[x].k;
        if (p <= 0) continue;
        pf_puts(w, TIMES), ps_fac(w, &W->fac[x], p, tmp, var);
        any = true;
    }
    return any;
}

static __attribute__((noinline)) void ps_walk(pstep_t *S, out_t *w, pfwork *W, pswork *P, const char *ans, int var)
{
    char v[2] = {(char)('A' + var), 0};
    bool cancelled = false, factored;
    rat lc;
    /* lc: d over the product of its factors' leads */
    {
        rat t;
        lc = W->d[rdeg(W->d) < 0 ? 0 : rdeg(W->d)];
        for (int i = 0; i < W->nf; i++)
            for (int m = 0; m < W->fac[i].k; m++) {
                pf_rmake(&t, 1, W->fac[i].f[W->fac[i].deg]);
                pf_rmul(&lc, &lc, &t);
            }
    }
    for (int i = 0; i < W->nf; i++) cancelled = cancelled || W->fac[i].gone > 0;
    /* COMMON FACTOR: the typed quotient and the reduced one, with CANCEL's rows (they end the answer too) */
    if (cancelled && ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_COMMON_FACTOR)) {
        /* the input, cleared: the reduced n and d times what cancelled (the kept block's n is the rows' room now) */
        pf_copy(P->t, W->n), pf_copy(P->u, W->d);
        for (int i = 0; i < W->nf; i++)
            for (int m = 0; m < W->fac[i].gone; m++) {
                pf_mul(P->cof, P->t, pf_f(&W->fac[i], P->cm)), pf_copy(P->t, P->cof);
                pf_mul(P->cof, P->u, pf_f(&W->fac[i], P->cm)), pf_copy(P->u, P->cof);
            }
        ps_side(w, P->t, var, false), pf_puts(w, DIVIDE), ps_side(w, P->u, var, true), pf_puts(w, "=");
        ps_side(w, W->n, var, false), pf_puts(w, DIVIDE), ps_side(w, W->d, var, true);
        { const char *r = strstr(ans, "; "); if (r) pf_puts(w, r); }
    }
    if (rdeg(W->d) < 1) {
        ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_ANSWER);
        return;
    }
    /* POLYNOMIAL PART */
    if (rdeg(W->poly) >= 0 && ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_POLYNOMIAL_PART)) {
        ps_side(w, W->n, var, false), pf_puts(w, DIVIDE), ps_side(w, W->d, var, true), pf_puts(w, "=");
        ps_rpoly(w, W->poly, var), ps_plus(w, W->r, var), pf_puts(w, DIVIDE), ps_side(w, W->d, var, true);
    }
    /* FACTOR: left out when d is one factor to the first power and lc is 1 */
    {
        int live = 0, kmax = 0;
        for (int i = 0; i < W->nf; i++) live += W->fac[i].k > 0, kmax = W->fac[i].k > kmax ? W->fac[i].k : kmax;
        factored = live > 1 || kmax > 1 || !(lc.p == 1 && lc.q == 1);
        if (live == 1 && kmax == 1) {
            /* one factor to the first power: r÷d is already its own partial fraction (lc's FACTOR line, if any) */
            if (factored && ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_FACTOR)) ps_factors(w, W, &lc, P->t, var);
            ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_ANSWER);
            return;
        }
    }
    if (factored && ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_FACTOR)) ps_factors(w, W, &lc, P->t, var);
    /* the unknowns: letters A, B, ... skipping the variable, factor by factor, power by power, the X's highest first */
    P->nu = 0;
    for (int i = 0, l = 0; i < W->nf; i++)
        for (int j = 1; j <= W->fac[i].k; j++)
            for (int e = W->fac[i].deg - 1; e >= 0; e--) {
                if (l == var) l++;
                P->letter[P->nu] = l++, P->fi[P->nu] = i, P->fj[P->nu] = j, P->fe[P->nu] = e, P->known[P->nu] = false;
                P->nu++;
            }
    /* FORM */
    if (ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_FORM)) {
        ps_side(w, W->r, var, false), pf_puts(w, DIVIDE), ps_side(w, W->d, var, true), pf_puts(w, "=");
        for (int u = 0; u < P->nu; u += W->fac[P->fi[u]].deg) {
            if (u) pf_puts(w, "+");
            ps_numer(w, P, u, W->fac[P->fi[u]].deg, var), pf_puts(w, DIVIDE), ps_fac(w, &W->fac[P->fi[u]], P->fj[u], P->t, var);
        }
    }
    /* CLEARED: r = lc×(Σ numerator × cofactor) */
    if (ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_CLEARED)) {
        bool wrap = !(lc.p == 1 && lc.q == 1) && P->nu > W->fac[P->fi[0]].deg;    /* more than one term */
        ps_rpoly(w, W->r, var), pf_puts(w, "=");
        if (wrap) ps_rat(w, &lc), pf_puts(w, TIMES "(");
        else ps_times(w, &lc);
        for (int u = 0; u < P->nu; u += W->fac[P->fi[u]].deg) {
            if (u) pf_puts(w, "+");
            ps_numer(w, P, u, W->fac[P->fi[u]].deg, var);
            ps_cofactor_text(w, W, P->fi[u], P->fj[u], P->t, var);
        }
        if (wrap) pf_puts(w, ")");
    }
    /* the values, from partfrac's own numerators (the pool), in the unknowns' order */
    for (int u = 0; u < P->nu; u++) {
        pf_get(W, &W->fac[P->fi[u]], P->fj[u], P->t);
        P->val[u] = P->t[P->fe[u]];
    }
    /* COVER-UP: each linear factor's root a leaves only its top power's unknown: r(a) = c U, c the cofactor at a */
    for (int u = 0; u < P->nu; u++) {
        pffac *F = &W->fac[P->fi[u]];
        if (F->deg != 1 || P->fj[u] != F->k) continue;
        P->known[u] = true;
        if (ps_hit(S, w, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_COVER_UP)) {
            rat a, rv, c = lc, fv;
            pf_rmake(&a, -F->f[0], F->f[1]);
            ps_at(W->r, &a, &rv);
            for (int i = 0; i < W->nf; i++) {
                if (i == P->fi[u]) continue;
                ps_at(pf_f(&W->fac[i], P->t), &a, &fv);
                for (int m = 0; m < W->fac[i].k; m++) pf_rmul(&c, &c, &fv);
            }
            pf_puts(w, v), pf_puts(w, "="), ps_rat(w, &a), pf_puts(w, ": "), ps_rat(w, &rv), pf_puts(w, "=");
            ps_times(w, &c), ps_letter(w, P->letter[u]);
        }
        if (ps_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_COVER_UP)) ps_letter(w, P->letter[u]), pf_puts(w, "="), ps_rat(w, &P->val[u]);
    }
    /* COEFFICIENTS: the rest, every power of X from deg d - 1 down, the known values moved across */
    {
        bool rest = false;
        for (int u = 0; u < P->nu; u++) rest = rest || !P->known[u];
        if (rest) {
            for (int m = rdeg(W->d) - 1; m >= 0; m--) {
                if (ps_hit(S, w, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_COEFFICIENTS)) {
                    rat left = W->r[m], *cm = P->cm;
                    bool any = false;
                    for (int u = 0; u < P->nu; u++) {
                        /* the X^m coefficient of lc × U X^e × cofactor */
                        pf_div(W->d, pf_f(&W->fac[P->fi[u]], P->u), P->cof, P->t);    /* d ÷ f */
                        for (int jj = 1; jj < P->fj[u]; jj++) pf_copy(P->t, P->cof), pf_div(P->t, pf_f(&W->fac[P->fi[u]], P->u), P->cof, P->t);
                        rat_int(&cm[u], 0);
                        if (m - P->fe[u] >= 0) cm[u] = P->cof[m - P->fe[u]];
                        if (P->known[u]) pf_fma(&left, &cm[u], &P->val[u], true);
                    }
                    if (m == 0) pf_puts(w, v), pf_puts(w, "^0: ");
                    else if (m == 1) pf_puts(w, v), pf_puts(w, ": ");
                    else pf_puts(w, v), pf_puts(w, "^"), pf_int(w, m), pf_puts(w, ": ");
                    ps_rat(w, &left), pf_puts(w, "=");
                    for (int u = 0; u < P->nu; u++) {
                        rat a = cm[u];
                        if (P->known[u] || a.p == 0) continue;
                        if (a.p < 0) pf_puts(w, "-"), a.p = -a.p;
                        else if (any) pf_puts(w, "+");
                        if (!(a.p == 1 && a.q == 1)) ps_rat(w, &a), pf_puts(w, TIMES);
                        ps_letter(w, P->letter[u]);
                        any = true;
                    }
                    if (!any) pf_puts(w, "0");
                }
            }
            if (ps_hit(S, w, 1, CASIM_STEP_RESULT, CASIM_RULE_COEFFICIENTS)) {
                bool first = true;
                for (int u = 0; u < P->nu; u++) {
                    if (P->known[u]) continue;
                    if (!first) pf_puts(w, "; ");
                    ps_letter(w, P->letter[u]), pf_puts(w, "="), ps_rat(w, &P->val[u]);
                    first = false;
                }
            }
        }
    }
    ps_hit(S, w, 0, CASIM_STEP_RESULT, CASIM_RULE_ANSWER);
}

static SS_KEEP_ARGS casim_status do_psteps(const casim_req *rq, walk_t *Pw, out_t *o, void *arena, size_t arena_cap)
{
    int k = Pw->k, flags = Pw->flags;
    casim_step_info *info = Pw->info;
    char *out = o->o;
    size_t cap = o->cap, room = arena_cap > SLOT ? arena_cap - SLOT : 0, skip, room2;
    char *ans = (char *)arena + room;
    casim_status st;
    ctx c;
    ref wr, pr;
    unsigned char *base;
    pstep_t S;
    if (!rq->eqn || !model_ok(rq->model) || rq->var < 0 || rq->var > 25 || rq->angle < CASIM_DEG || rq->angle > CASIM_GRAD)
        return CASIM_BAD_ARG;
    if (cap == 0 && k >= 0) return CASIM_TOO_LONG;
    st = arena_cap < SLOT ? CASIM_NO_ROOM
                          : do_partfrac(rq->eqn, rq->var, rq->angle, ans, SLOT, arena, room, rq->model == CASIM_MODEL_STU);
    if (st == CASIM_NO_ROOM) {
        st = do_partfrac(rq->eqn, rq->var, rq->angle, out, cap, arena, arena_cap, rq->model == CASIM_MODEL_STU);
        if (cap) out[0] = st == CASIM_OK || st == CASIM_PARTIAL ? '\0' : out[0];
        return st == CASIM_OK || st == CASIM_PARTIAL ? CASIM_NO_ROOM : st;
    }
    if (st != CASIM_OK && st != CASIM_PARTIAL) {
        if (cap > strlen(ans)) memcpy(out, ans, strlen(ans) + 1);
        return st;
    }
    /* what do_partfrac left: its kept block at its arena's top, its work area first below (held: not zeroed) */
    skip = (size_t)(-(uintptr_t)arena & 7u);
    base = (unsigned char *)arena + skip;
    room2 = (room - skip - sizeof(pfkeep)) & ~(size_t)7;
    start(&c, "1", rq->angle, base, room2);
    wr = held(&c, sizeof(pfwork));
    pr = grab(&c, sizeof(pswork));
    if (c.st != CASIM_OK) return CASIM_NO_ROOM;
    S.k = k, S.at = 0, S.one = (flags & CASIM_STEPS_ONE_LINE) != 0, S.cap = cap, S.info = info;
    {
        out_t w = {out, cap, 0, false};
        ps_walk(&S, &w, (pfwork *)(void *)nd(&c, wr), (pswork *)(void *)nd(&c, pr), ans, rq->var);
        info->count = S.at;
        if (k < 0) return st;
        if (k >= S.at) return CASIM_BAD_ARG;
        if (info->rule == CASIM_RULE_ANSWER) {
            w.n = 0, w.full = false;
            pf_puts(&w, ans);
        }
        if (w.full) return out[0] = '\0', CASIM_TOO_LONG;
    }
    return st;                                           /* PARTIAL on every page when partfrac's answer is */
}

/* ---- integrals of rational functions (unit CAS 013) ----
   When integrate's table refuses, casim_integrate tries the integrand as a rational function: partfrac whole (its
   arena fresh: factor needs most of 8 KB), then each term of its result by the forms of rule 4, each checked against
   its own term by closes(); with partfrac's rule-7 identity the term checks prove F' = n÷d. The forms refused (rule
   5): a quadratic with D > 0, a quadratic to a power >= 2, a factor of degree 3 or more. */

static ref ir_rat(ctx *c, int64_t p, int64_t q)
{
    rat r;
    if (!pf_rmake(&r, p, q)) return c->st = CASIM_NO_ROOM, SPARE;
    return num(c, &r);
}

/* ∫ num ÷ F^j dX, its antiderivative's tree; SPARE (status NOT_INTEGRABLE) for a form the table has not */
static __attribute__((noinline)) ref ir_term(ctx *c, const pffac *F, int j, const rat *nm, rat *tmp, int var)
{
    ref ft = poly_tree(c, pf_f(F, tmp), var), x = leaf(c, K_VAR, var);
    int64_t q, bb, ac, D;
    if (F->deg == 1) {
        /* c ÷ (aX+b)^j: c÷a LN|aX+b| for j = 1, c÷(a(1-j)) (aX+b)^(1-j) after */
        if (!mul64(&q, nm[0].q, F->f[1]) || (j > 1 && !mul64(&q, q, 1 - j))) return c->st = CASIM_NO_ROOM, SPARE;
        if (j == 1) return mul2(c, ir_rat(c, nm[0].p, q), fn(c, F_LN, fn(c, F_ABS, ft)));
        return mul2(c, ir_rat(c, nm[0].p, q), power(c, ft, ir_rat(c, 1 - j, 1)));
    }
    if (F->deg == 2 && (!mul64(&bb, F->f[1], F->f[1]) || !mul64(&ac, 4 * F->f[2], F->f[0]) || !add64(&D, bb, -ac)))
        return c->st = CASIM_NO_ROOM, SPARE;
    if (F->deg == 2 && j == 1 && D < 0) {
        /* (B X + C) ÷ (a X^2 + b X + c), D = b^2 - 4ac < 0: B÷(2a) LN f + (C - B b÷(2a)) 2÷√(-D) ATAN((2aX+b)÷√(-D)), f > 0
           everywhere so its LN has no ABS (rule 4); D > 0 is refused before anything is built (rule 5) */
        int64_t a = F->f[2], b = F->f[1], k, r;
        rat B = nm[1], C = nm[0], k1, k2, t;
        ref g, u, root;
        if (!pf_rmake(&t, 1, 2 * a) || !pf_rmul(&k1, &B, &t) || !pf_rmake(&t, -b, 1) || !pf_rmul(&t, &t, &k1) ||
            !pf_radd(&k2, &C, &t))
            return c->st = CASIM_NO_ROOM, SPARE;                     /* k1 = B÷(2a), k2 = C - B b÷(2a) */
        u = add2(c, mul2(c, ir_rat(c, 2 * a, 1), x), ir_rat(c, b, 1));    /* 2aX + b */
        square_free(-D, &k, &r);                                     /* √(-D) = k √r, r square-free */
        root = r == 1 ? ir_rat(c, k, 1) : mul2(c, ir_rat(c, k, 1), power(c, ir_rat(c, r, 1), small(c, C_HALF)));
        g = mul2(c, mul2(c, small(c, C_2), power(c, root, small(c, C_M1))),
                 mul2(c, fn(c, F_ATAN, mul2(c, u, power(c, root, small(c, C_M1)))), angle_factor(c, false)));
        return add2(c, mul2(c, num(c, &k1), fn(c, F_LN, ft)), mul2(c, num(c, &k2), g));
    }
    if (c->st == CASIM_OK) c->st = CASIM_NOT_INTEGRABLE;
    return SPARE;
}

/* A test build (tests/ircheck_check.c) defines these to spoil a term before its check, or to take the check away (so
   rule 5's refusals are seen to stand on their own); otherwise the term as built, and closes() */
#ifndef CASIM_IR_PLANT
#define CASIM_IR_PLANT(c, F) (F)
#endif
#ifndef CASIM_IR_CLOSES
#define CASIM_IR_CLOSES(c, F, f) closes(c, F, f)
#endif

/* what the integration needs of partfrac's work area, moved down over its start (rule: arena) */
typedef struct {
    rat poly[PF_N];
    pffac fac[SOLVE_DEG];
    rat pool[SOLVE_DEG];
    int nf;
} irkeep;

/* the arena back to mark, the constants cached since dropped (unparse's twin: they would point into reused room) */
static void ir_reset(ctx *c, size_t mark)
{
    c->used = mark;
    for (int j = 0; j < SMALL; j++)
        if (c->small[j] >= mark / 8u) c->small[j] = 0;
    if (c->pi >= mark / 8u) c->pi = 0;
}

/* the room the rational path needs to start at arena: its slot, partfrac's block and work area */
static size_t ir_need(const void *arena)
{
    return SLOT + (size_t)(-(uintptr_t)arena & 7u) + sizeof(pfkeep) + sizeof(pfwork) + 64;
}

/* integrate's table refused eqn: ∫ of it as a rational function, through partfrac (run whole, its arena fresh), a
   term at a time, each term's antiderivative checked against the term by integrate's own check (closes) */
static __attribute__((noinline)) casim_status do_irational(const char *eqn, int var, int angle, char *out, size_t cap,
                                                           void *arena, size_t arena_cap, bool rtl)
{
    size_t room = arena_cap > SLOT ? arena_cap - SLOT : 0, skip, room2, mark, ocap = cap < SLOT ? cap : SLOT;
    char *ans = (char *)arena + room;
    casim_status st;
    ctx c;
    ref wr, tr;
    unsigned char *base;
    irkeep *W;
    rat *T;
    out_t w = {out, ocap, 0, false};
    skip = (size_t)(-(uintptr_t)arena & 7u);
    if (arena_cap < ir_need(arena))
        return CASIM_NOT_INTEGRABLE;            /* partfrac can't start: the table's refusal stands (rule 2) */
    base = (unsigned char *)arena + skip;
    room2 = (room - skip - sizeof(pfkeep)) & ~(size_t)7;
    /* where partfrac keeps its block: pf_work clears its ftext first, so a mark left there says the reader refused
       (bounds, letters, the form) and integrate keeps its NOT_INTEGRABLE (rule 2, #6686); a NO_ROOM from the working
       itself (the arena, or int64) stays NO_ROOM */
    ((pfkeep *)(void *)(base + room2))->ftext[0] = '\x01';     /* no factor text starts with it */
    st = do_partfrac(eqn, var, angle, ans, SLOT, arena, room, rtl);
    if (st == CASIM_NO_ROOM && ((pfkeep *)(void *)(base + room2))->ftext[0] != '\x01') return CASIM_NO_ROOM;
    if (st != CASIM_OK && st != CASIM_TOO_LONG) return CASIM_NOT_INTEGRABLE;
    start(&c, "1", angle, base, room2);
    c.var = var;
    {
        /* partfrac's work area, compacted to what is needed: each part moved down (to below where it was) */
        size_t m0 = c.used;
        pfwork *P;
        int nf;
        wr = held(&c, sizeof(pfwork));
        if (c.st != CASIM_OK) return CASIM_NO_ROOM;
        P = (pfwork *)(void *)nd(&c, wr);
        W = (irkeep *)(void *)P;
        nf = P->nf;
        memmove(W->poly, P->poly, sizeof W->poly);
        memmove(W->fac, P->fac, sizeof W->fac);
        memmove(W->pool, P->pool, sizeof W->pool);
        W->nf = nf;
        ir_reset(&c, m0 + ((sizeof(irkeep) + 7u) & ~(size_t)7u));
    }
    /* rule 8 (#6708): a common factor cancelled means the reduced function's antiderivative would be defined where
       the integrand isn't (CAS 006's guarantee: X^2÷X stays refused), so the path refuses */
    for (int i = 0; i < W->nf; i++)
        if (W->fac[i].gone) return CASIM_NOT_INTEGRABLE;
    tr = grab(&c, 3 * PF_N * sizeof(rat));
    if (c.st != CASIM_OK) return CASIM_NO_ROOM;
    T = (rat *)(void *)nd(&c, tr);
    mark = c.used;
    if (cap) out[0] = '\0';
    /* two passes: each term checked against its own term by closes(), the arena back after each; then the checked
       terms built again as one sum, printed in integrate's canonical form (rule 7: one answer style, the whole
       sum's order) */
    for (int pass = 0; pass < 2; pass++) {
        ref acc = SPARE, F, f;
        if (rdeg(W->poly) >= 0) {
            /* the polynomial part: Σ q_k X^(k+1)÷(k+1) */
            pzero(T);
            for (int k = rdeg(W->poly); k >= 0; k--) {
                rat t;
                if (k + 1 > SOLVE_DEG) return CASIM_NOT_INTEGRABLE;     /* an X^13 term: past the bound, as before (rule 2) */
                if (!pf_rmake(&t, 1, k + 1) || !pf_rmul(&T[k + 1], &W->poly[k], &t)) return CASIM_NO_ROOM;
            }
            F = poly_tree(&c, T, var);
            if (pass == 0) {
                F = CASIM_IR_PLANT(&c, F);
                f = poly_tree(&c, W->poly, var);
                if (c.st != CASIM_OK || !CASIM_IR_CLOSES(&c, F, f)) return c.st == CASIM_OK || c.st == CASIM_NOT_INTEGRABLE ? CASIM_NOT_INTEGRABLE : c.st;
                ir_reset(&c, mark);
            } else acc = F;
        }
        for (int i = 0; i < W->nf; i++)
            for (int j = 1; j <= W->fac[i].k; j++) {
                pzero(T);
                for (int x = 0; x < W->fac[i].deg; x++) T[x] = W->pool[W->fac[i].off + (j - 1) * W->fac[i].deg + x];
                if (rdeg(T) < 0) continue;
                F = ir_term(&c, &W->fac[i], j, T, T + PF_N, var);
                if (pass == 0) {
                    F = CASIM_IR_PLANT(&c, F);
                    f = mul2(&c, poly_tree(&c, T, var), power(&c, poly_tree(&c, pf_f(&W->fac[i], T + PF_N), var), ir_rat(&c, -j, 1)));
                    if (c.st != CASIM_OK || !CASIM_IR_CLOSES(&c, F, f))
                        return c.st == CASIM_OK || c.st == CASIM_NOT_INTEGRABLE ? CASIM_NOT_INTEGRABLE : c.st;
                    ir_reset(&c, mark);
                } else acc = acc == SPARE ? F : add2(&c, acc, F);
            }
        if (pass == 1) {
            if (c.st != CASIM_OK) return c.st == CASIM_NOT_INTEGRABLE ? CASIM_NOT_INTEGRABLE : c.st;
            if (acc == SPARE) pf_puts(&w, "0");
            else print(&c, &w, canon(&c, acc));
            if (c.st != CASIM_OK) return out[0] = '\0', c.st;
        }
    }
    if (w.full) return out[0] = '\0', CASIM_TOO_LONG;
    return CASIM_OK;
}

/* ---- show steps for integrals of rational functions (CAS 013's second build, rule 10) ----
   work/013-rational-integrals.md §Steps. When integrate's table refuses and the rational path answers: PARTIAL
   FRACTIONS (the integrand as partfrac writes it, partfrac's own pages a level down), each term's integral, then
   integrate's answer. The polynomial part and each 1÷(aX+b)^j are the table's own walks run on their texts (the
   multiple c÷m taken out first, so each piece is the answer's own); a quadratic's parts are this walk's, each CHECK
   proved by closes() before it prints. Nothing is kept between calls: every call counts every page. */

/* this section's own helpers, so integrate's walk and printers keep their inlining (frames unmoved) */
/* pf_term's twin: partfrac's printers keep pf_term inlined */
static __attribute__((noinline)) bool irs_pterm(out_t *w, const rat *num, const rat *f, int j, int var, bool first, rat *ip)
{
    int64_t l = 1, g = 0;
    int dn = rdeg(num), terms = 0;
    rat k;
    for (int i = 0; i <= dn; i++)
        if (num[i].p != 0 && !mul64(&l, l, num[i].q / gcd64(l, num[i].q))) return false;
    pzero(ip);
    for (int i = 0; i <= dn; i++) {
        if (num[i].p != 0 && !mul64(&ip[i].p, num[i].p, l / num[i].q)) return false;
        g = gcd64(g, ip[i].p);
    }
    if (ip[dn].p < 0) g = -g;
    for (int i = 0; i <= dn; i++) ip[i].p /= g, terms += ip[i].p != 0;
    if (!pf_rmake(&k, g, l)) return false;                       /* num = k N */
    if (k.p < 0) pf_puts(w, "-"), k.p = -k.p;
    else if (!first) pf_puts(w, "+");
    if (dn == 0) pf_int(w, k.p);
    else {
        if (k.p != 1) pf_int(w, k.p), pf_puts(w, TIMES);
        if (terms > 1) pf_puts(w, "(");
        pf_ipoly(w, ip, var);
        if (terms > 1) pf_puts(w, ")");
    }
    pf_puts(w, DIVIDE);
    if (k.q != 1) pf_puts(w, "("), pf_int(w, k.q), pf_puts(w, TIMES);
    if (rdeg(f) == 1 && f[0].p == 0 && f[1].p == 1) pf_ipoly(w, f, var);         /* X alone */
    else pf_puts(w, "("), pf_ipoly(w, f, var), pf_puts(w, ")");
    if (j > 1) pf_puts(w, "^"), pf_int(w, j);
    if (k.q != 1) pf_puts(w, ")");
    return true;
}

static __attribute__((noinline)) void irs_show(ctx *c, out_t *w, ref r)
{
    print(c, w, canon(c, r));
}

static __attribute__((noinline)) void irs_zero(rat *p)
{
    for (int k = 0; k < PF_N; k++) p[k].p = 0, p[k].q = 1;
}

static __attribute__((noinline)) void irs_reset(ctx *c, size_t mark)
{
    c->used = mark;
    for (int j = 0; j < SMALL; j++)
        if (c->small[j] >= mark / 8u) c->small[j] = 0;
    if (c->pi >= mark / 8u) c->pi = 0;
}

/* numerator j of factor i, from the compacted pool (irkeep's layout, not pfwork's: pf_get would read the wrong place) */
static void irs_num(const irkeep *W, int i, int j, rat *o)
{
    irs_zero(o);
    for (int x = 0; x < W->fac[i].deg; x++) o[x] = W->pool[W->fac[i].off + (j - 1) * W->fac[i].deg + x];
}

typedef struct {
    int k, at, flags;
    int want;                   /* the page asked for is an answer line, printed last: 1 SUM's, 2 the root's */
    bool skip;                  /* partfrac changed nothing (S2.2): the one term's walk is the root */
    casim_step_info *info;
    out_t *w;
} irs_t;

/* the next page: true when it is the one asked for (w cleared for it) */
static __attribute__((noinline)) bool irs_hit(irs_t *S, int depth, int line, int rule)
{
    if (line == CASIM_STEP_TEMPLATE && (S->flags & CASIM_STEPS_ONE_LINE)) return false;
    if (S->at++ != S->k) return false;
    S->info->depth = depth, S->info->line = line, S->info->rule = rule;
    S->w->n = 0, S->w->full = false;
    if (S->w->cap) S->w->o[0] = '\0';
    return true;
}

/* a term's last line: when the term is the root (S2.2) it is integrate's answer, printed at the end */
static bool irs_last(irs_t *S, int depth, int rule)
{
    if (!S->skip) return irs_hit(S, depth, CASIM_STEP_RESULT, rule);
    if (irs_hit(S, 0, CASIM_STEP_RESULT, rule)) S->want = 2;
    return false;
}

/* another walk's pages as this walk's, down levels lower: the table's on a term's text, or partfrac's */
static __attribute__((noinline)) casim_status irs_sub(irs_t *S, const casim_req *rq, int down, void *arena, size_t cap)
{
    casim_step_info si;
    casim_status st = casim_step(rq, -1, S->flags, &si, NULL, 0, arena, cap);
    if (st != CASIM_OK) return st;
    if (S->k >= S->at && S->k < S->at + si.count) {
        st = casim_step(rq, S->k - S->at, S->flags, S->info, S->w->o, S->w->cap, arena, cap);
        S->info->depth += down;
        if (st != CASIM_OK) return st;
    }
    S->at += si.count;
    return CASIM_OK;
}

/* ")dX" */
static void irs_dx(out_t *w, int var)
{
    char d[4] = {')', 'd', (char)('A' + var), '\0'};
    pf_puts(w, d);
}

/* ∫(text)dX */
static void irs_int(out_t *w, const char *text, int var)
{
    pf_puts(w, "\xe2\x88\xab("), pf_puts(w, text), irs_dx(w, var);
}

/* (F), or X alone */
static void irs_f(out_t *w, const rat *f, int var)
{
    if (rdeg(f) == 1 && f[0].p == 0 && f[1].p == 1) pf_ipoly(w, f, var);
    else pf_puts(w, "("), pf_ipoly(w, f, var), pf_puts(w, ")");
}

/* f' = 2a X + b, in parentheses when b is not 0 (a > 0: f's lead is positive) */
static void irs_fprime(out_t *w, int64_t a, int64_t b, int var)
{
    char v[2] = {(char)('A' + var), '\0'};
    if (b) pf_puts(w, "(");
    pf_int(w, 2 * a), pf_puts(w, TIMES), pf_puts(w, v);
    if (b) pf_puts(w, b > 0 ? "+" : "-"), pf_int(w, b > 0 ? b : -b), pf_puts(w, ")");
}

/* +|x| or -|x| */
static void irs_signed(out_t *w, const rat *x)
{
    rat a = *x;
    if (a.p < 0) a.p = -a.p, pf_puts(w, "-");
    else pf_puts(w, "+");
    ps_rat(w, &a);
}

/* the term's ∫(term)dX: as partfrac prints the term, or the root's when the term is the root */
static void irs_lhs(irs_t *S, const char *root, const rat *nm, const rat *f, int j, int var, rat *ip)
{
    if (S->skip) irs_int(S->w, root, var);
    else pf_puts(S->w, "\xe2\x88\xab("), irs_pterm(S->w, nm, f, j, var, true, ip), irs_dx(S->w, var);
}

/* each term's integral, with its sign, as partfrac's sum has them (SUM's template) */
static __attribute__((noinline)) bool irs_terms(out_t *w, const irkeep *W, rat *T, int var)
{
    bool first = true;
    if (rdeg(W->poly) >= 0) {
        pf_puts(w, "\xe2\x88\xab("), pf_rpoly(w, W->poly, var), irs_dx(w, var);
        first = false;
    }
    for (int i = 0; i < W->nf; i++)
        for (int j = 1; j <= W->fac[i].k; j++) {
            irs_num(W, i, j, T);
            if (rdeg(T) < 0) continue;
            if (T[rdeg(T)].p < 0) {
                for (int x = 0; x <= rdeg(T); x++) T[x].p = -T[x].p;
                pf_puts(w, "-");
            } else if (!first) pf_puts(w, "+");
            pf_puts(w, "\xe2\x88\xab(");
            if (!irs_pterm(w, T, pf_f(&W->fac[i], T + PF_N), j, var, true, T + 2 * PF_N)) return false;
            irs_dx(w, var);
            first = false;
        }
    return true;
}

/* a linear term c ÷ (m (aX+b)^j) at depth D: CONSTANT MULTIPLE takes c÷m out (none when it is 1), the table's own walk
   on 1÷(aX+b)^j a level down, then canon(c÷m × piece), the answer's own piece. sa, scap: the sub-walk's arena */
static __attribute__((noinline)) casim_status irs_lin(irs_t *S, ctx *c, const pffac *F, int j, const rat *nm, rat *T,
                                                      int D, casim_req *sub, unsigned char *sa, size_t scap,
                                                      const char *root)
{
    rat *f = pf_f(F, T + PF_N);
    bool one = nm[0].p == 1 && nm[0].q == 1;
    out_t tw = {(char *)sub->eqn, SLOT, 0, false};
    casim_status st;
    pf_puts(&tw, "1" DIVIDE), irs_f(&tw, f, c->var);
    if (j > 1) pf_puts(&tw, "^"), pf_int(&tw, j);
    if (tw.full) return CASIM_NO_ROOM;
    if (!one && irs_hit(S, D, CASIM_STEP_TEMPLATE, CASIM_RULE_CONSTANT_MULTIPLE)) {
        irs_lhs(S, root, nm, f, j, c->var, T + 2 * PF_N), pf_puts(S->w, " = ");
        ps_times(S->w, &nm[0]), irs_int(S->w, sub->eqn, c->var);
    }
    if ((st = irs_sub(S, sub, one ? D : D + 1, sa, scap)) != CASIM_OK) return st;
    if (!one && irs_last(S, D, CASIM_RULE_CONSTANT_MULTIPLE)) {
        irs_lhs(S, root, nm, f, j, c->var, T + 2 * PF_N), pf_puts(S->w, " = ");
        irs_show(c, S->w, ir_term(c, F, j, nm, T + 2 * PF_N, c->var));
    }
    return c->st;
}

/* A test build (tests/ircheck_check.c) defines these to spoil a quadratic part's antiderivative before its proof, or
   to take the proof away; otherwise the part as built, and closes() */
#ifndef CASIM_IRS_PLANT
#define CASIM_IRS_PLANT(c, F) (F)
#endif
#ifndef CASIM_IRS_CLOSES
#define CASIM_IRS_CLOSES(c, F, f) closes(c, F, f)
#endif

/* a quadratic term (B X + C) ÷ (m f), f = aX^2 + bX + c with no real root, at depth D: SPLIT into k1 (2aX+b) ÷ f and
   k2 1 ÷ f (with one part, CONSTANT MULTIPLE, or nothing when its multiple is 1), LOG DERIVATIVE, COMPLETE THE SQUARE
   (but for X^2 + k, S2.3), ARCTANGENT. Each part's antiderivative is proved by closes() on every call, before any of
   its lines prints: one that doesn't close refuses the walk (NO_ROOM, softly: integrate's answer stands). */
static __attribute__((noinline)) casim_status irs_quad(irs_t *S, ctx *c, const pffac *F, const rat *nm, rat *T, int D,
                                                       const char *root)
{
    int var = c->var, rs, dn;
    int64_t a = F->f[2], b = F->f[1], bb, ac, Dq, a4;
    rat *f = pf_f(F, T + PF_N), k1, k2, h, kk, t, z[2] = {{1, 1}, {0, 1}};
    char u[2] = {(char)(var == 'U' - 'A' ? 'V' : 'U'), '\0'};
    bool two, unit, square = !(b == 0 && a == 1);
    size_t mark = c->used;
    ref Fp, fp;
    if (!mul64(&bb, b, b) || !mul64(&ac, 4 * a, F->f[0]) || !add64(&Dq, bb, -ac) || !mul64(&a4, 4 * a, a) ||
        !pf_rmake(&t, 1, 2 * a) || !pf_rmul(&k1, &nm[1], &t) || !pf_rmake(&t, -b, 1) || !pf_rmul(&t, &t, &k1) ||
        !pf_radd(&k2, &nm[0], &t) || !pf_rmake(&h, b, 2 * a) || !pf_rmake(&kk, -Dq, a4))
        return CASIM_NO_ROOM;
    two = k1.p && k2.p;
    unit = !two && (k1.p ? k1.p == k1.q : k2.p == k2.q);
    rs = two ? CASIM_RULE_SPLIT : CASIM_RULE_CONSTANT_MULTIPLE;
    dn = unit ? D : D + 1;
    if (!unit && irs_hit(S, D, CASIM_STEP_TEMPLATE, rs)) {
        irs_lhs(S, root, nm, f, 1, var, T + 2 * PF_N), pf_puts(S->w, " = ");
        if (k1.p) {
            ps_times(S->w, &k1), pf_puts(S->w, "\xe2\x88\xab("), irs_fprime(S->w, a, b, var);
            pf_puts(S->w, DIVIDE), irs_f(S->w, f, var), irs_dx(S->w, var);
        }
        if (k2.p) {
            if (k1.p && k2.p > 0) pf_puts(S->w, "+");
            ps_times(S->w, &k2), pf_puts(S->w, "\xe2\x88\xab(1" DIVIDE), irs_f(S->w, f, var), irs_dx(S->w, var);
        }
    }
    if (k1.p) {
        /* ∫ (2aX+b) ÷ f = LN(f), f > 0 everywhere */
        rat *d = T + 2 * PF_N;
        irs_zero(d), d[1].p = 2 * a, d[0].p = b;
        Fp = fn(c, F_LN, poly_tree(c, f, var));
        fp = mul2(c, poly_tree(c, d, var), power(c, poly_tree(c, f, var), small(c, C_M1)));
        Fp = CASIM_IRS_PLANT(c, Fp);
        if (c->st != CASIM_OK) return c->st;
        if (!CASIM_IRS_CLOSES(c, Fp, fp)) return c->st == CASIM_OK || c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;
        if (irs_hit(S, dn, CASIM_STEP_TEMPLATE, CASIM_RULE_LOG_DERIVATIVE)) {
            pf_puts(S->w, "\xe2\x88\xab("), irs_fprime(S->w, a, b, var), pf_puts(S->w, DIVIDE), irs_f(S->w, f, var);
            irs_dx(S->w, var), pf_puts(S->w, " = \xe2\x88\xab(1" DIVIDE), pf_puts(S->w, u), pf_puts(S->w, ")d");
            pf_puts(S->w, u), pf_puts(S->w, ", "), pf_puts(S->w, u), pf_puts(S->w, "="), pf_ipoly(S->w, f, var);
        }
        if (irs_hit(S, dn, CASIM_STEP_RESULT, CASIM_RULE_CHECK)) {
            pf_puts(S->w, "d/d"), ps_letter(S->w, var), pf_puts(S->w, "("), irs_show(c, S->w, Fp);
            pf_puts(S->w, ") = "), irs_fprime(S->w, a, b, var), pf_puts(S->w, DIVIDE), irs_f(S->w, f, var);
        }
        if (unit ? irs_last(S, dn, CASIM_RULE_LOG_DERIVATIVE) : irs_hit(S, dn, CASIM_STEP_RESULT, CASIM_RULE_LOG_DERIVATIVE)) {
            pf_puts(S->w, "\xe2\x88\xab("), irs_fprime(S->w, a, b, var), pf_puts(S->w, DIVIDE), irs_f(S->w, f, var);
            irs_dx(S->w, var), pf_puts(S->w, " = "), irs_show(c, S->w, Fp);
        }
        irs_reset(c, mark);
    }
    if (k2.p) {
        /* ∫ 1 ÷ f, as ir_term builds it (the angle factor in DEG and GRAD) */
        Fp = ir_term(c, F, 1, z, T + 2 * PF_N, var);
        fp = power(c, poly_tree(c, f, var), small(c, C_M1));
        Fp = CASIM_IRS_PLANT(c, Fp);
        if (c->st != CASIM_OK) return c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;
        if (!CASIM_IRS_CLOSES(c, Fp, fp)) return c->st == CASIM_OK || c->st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : c->st;
        if (square && irs_hit(S, dn, CASIM_STEP_RESULT, CASIM_RULE_COMPLETE_SQUARE)) {
            pf_ipoly(S->w, f, var), pf_puts(S->w, "=");
            if (a != 1) pf_int(S->w, a), pf_puts(S->w, TIMES "(");
            if (h.p) pf_puts(S->w, "(");
            ps_letter(S->w, var);
            if (h.p) irs_signed(S->w, &h), pf_puts(S->w, ")");
            pf_puts(S->w, "^2+"), ps_rat(S->w, &kk);
            if (a != 1) pf_puts(S->w, ")");
        }
        if (square && irs_hit(S, dn, CASIM_STEP_TEMPLATE, CASIM_RULE_ARCTANGENT)) {
            pf_puts(S->w, "\xe2\x88\xab(1" DIVIDE), irs_f(S->w, f, var), irs_dx(S->w, var);
            pf_puts(S->w, " = \xe2\x88\xab(1" DIVIDE "("), pf_puts(S->w, u), pf_puts(S->w, "^2+"), ps_rat(S->w, &kk);
            pf_puts(S->w, "))d"), pf_puts(S->w, u);
            if (a != 1) pf_puts(S->w, DIVIDE), pf_int(S->w, a);
            pf_puts(S->w, ", "), pf_puts(S->w, u), pf_puts(S->w, "="), ps_letter(S->w, var);
            if (h.p) irs_signed(S->w, &h);
        }
        if (irs_hit(S, dn, CASIM_STEP_RESULT, CASIM_RULE_CHECK)) {
            pf_puts(S->w, "d/d"), ps_letter(S->w, var), pf_puts(S->w, "("), irs_show(c, S->w, Fp);
            pf_puts(S->w, ") = 1" DIVIDE), irs_f(S->w, f, var);
        }
        if (unit ? irs_last(S, dn, CASIM_RULE_ARCTANGENT) : irs_hit(S, dn, CASIM_STEP_RESULT, CASIM_RULE_ARCTANGENT)) {
            pf_puts(S->w, "\xe2\x88\xab(1" DIVIDE), irs_f(S->w, f, var), irs_dx(S->w, var), pf_puts(S->w, " = ");
            irs_show(c, S->w, Fp);
        }
        irs_reset(c, mark);
    }
    if (!unit && irs_last(S, D, rs)) {
        irs_lhs(S, root, nm, f, 1, var, T + 2 * PF_N), pf_puts(S->w, " = ");
        irs_show(c, S->w, ir_term(c, F, 1, nm, T + 2 * PF_N, var));
    }
    return c->st;
}

/* an answer line, ∫(text)dX = integrate's answer: the answer into out with the whole arena, then moved right for its
   left side (SUM's: partfrac's text, into the arena's top slot; the root's: the integrand as typed) */
static __attribute__((noinline)) casim_status irs_answer(const casim_req *rq, int want, char *out, size_t cap,
                                                         void *arena, size_t arena_cap, char *txt, size_t room)
{
    bool rtl = rq->model == CASIM_MODEL_STU;
    const char *mid = rq->eqn;
    size_t n, m, p;
    casim_status st = do_irational(rq->eqn, rq->var, rq->angle, out, cap, arena, arena_cap, rtl);
    if (st != CASIM_OK) return st;
    if (want == 1) {
        if (do_partfrac(rq->eqn, rq->var, rq->angle, txt, SLOT, arena, room, rtl) != CASIM_OK) return out[0] = '\0', CASIM_NO_ROOM;
        mid = txt;
    }
    n = strlen(out), m = strlen(mid), p = 4 + m + 3 + 3;    /* "∫(" text ")dX" " = " */
    if (p + n + 1 > cap) return out[0] = '\0', CASIM_TOO_LONG;
    memmove(out + p, out, n + 1);
    memcpy(out, "\xe2\x88\xab(", 4);
    memcpy(out + 4, mid, m);
    out[4 + m] = ')', out[5 + m] = 'd', out[6 + m] = (char)('A' + rq->var);
    memcpy(out + 7 + m, " = ", 3);
    return CASIM_OK;
}

/* tc: integrate's walk's context, which refused (do_isteps): its arena is this walk's (aligned, as start() took it); o:
   its out and cap. Six arguments, so casim_step passes none on the stack (its frame unmoved) */
static __attribute__((noinline)) casim_status do_irsteps(const casim_req *rq, int k, int flags, casim_step_info *info,
                                                         const out_t *o, const ctx *tc)
{
    char *out = o->o;
    size_t cap = o->cap, arena_cap = tc->cap;
    void *arena = tc->mem;
    irs_t S;
    out_t w = {out, cap, 0, false};
    casim_req sub = *rq;
    casim_status st;
    size_t room = arena_cap > SLOT ? arena_cap - SLOT : 0, skip, room2;
    char *txt = (char *)arena + room;           /* partfrac's text, then each term's: the arena's top slot */
    bool rtl = rq->model == CASIM_MODEL_STU;
    int var = rq->var, nt, D;
    ctx c;
    irkeep *W;
    rat *T;
    memset(info, 0, sizeof *info);
    if (cap) out[0] = '\0';
    /* integrate's answer, or its refusal, with the whole arena (a slot kept at the top cost 94 DEG answers' walks in
       the prototype); a small out borrows the slot */
    st = cap >= SLOT ? do_irational(rq->eqn, var, rq->angle, out, cap, arena, arena_cap, rtl)
                     : do_irational(rq->eqn, var, rq->angle, txt, SLOT, arena, room, rtl);
    /* CAS 017 rule 8: when the rational path refuses too, substitution and parts' walk */
    if (st == CASIM_NOT_INTEGRABLE && ir_cancels(rq->eqn, var, rq->angle, arena, arena_cap, rtl)) return st;    /* rule 8 */
    if (st != CASIM_OK) return st == CASIM_NOT_INTEGRABLE ? (info->line = flags, ib_walk(rq, k, info, out, cap, arena, arena_cap)) : st;
    if (do_partfrac(rq->eqn, var, rq->angle, txt, SLOT, arena, room, rtl) != CASIM_OK) return CASIM_NO_ROOM;
    S.k = k, S.at = 0, S.flags = flags, S.info = info, S.w = &w, S.want = 0;
    /* S2.2: partfrac changed nothing when its answer and the integrand have one canonical form */
    start(&c, rq->eqn, rq->angle, arena, room);
    c.var = var, c.rtl = rtl, c.opaque = true;
    {
        ref x = canon(&c, parse(&c)), y;
        c.s = txt;
        y = canon(&c, parse(&c));
        if (c.st != CASIM_OK) return CASIM_NO_ROOM;
        S.skip = cmp(&c, x, y) == 0;
    }
    if (!S.skip) {
        if (irs_hit(&S, 0, CASIM_STEP_TEMPLATE, CASIM_RULE_PARTIAL_FRACTIONS))
            irs_int(&w, rq->eqn, var), pf_puts(&w, " = "), irs_int(&w, txt, var);
        sub.op = CASIM_PARTFRAC;
        if ((st = irs_sub(&S, &sub, 1, arena, arena_cap)) != CASIM_OK) return st;
    }
    /* the terms: partfrac whole again (its pages used the arena), its work area compacted as do_irational's */
    if (do_partfrac(rq->eqn, var, rq->angle, txt, SLOT, arena, room, rtl) != CASIM_OK) return CASIM_NO_ROOM;
    skip = (size_t)(-(uintptr_t)arena & 7u);
    room2 = (room - skip - sizeof(pfkeep)) & ~(size_t)7;
    start(&c, "1", rq->angle, (unsigned char *)arena + skip, room2);
    c.var = var, c.rtl = rtl, c.opaque = true;
    {
        size_t m0 = c.used;
        ref wr = held(&c, sizeof(pfwork));
        pfwork *P;
        int nf;
        if (c.st != CASIM_OK) return CASIM_NO_ROOM;
        P = (pfwork *)(void *)nd(&c, wr);
        W = (irkeep *)(void *)P;
        nf = P->nf;
        memmove(W->poly, P->poly, sizeof W->poly);
        memmove(W->fac, P->fac, sizeof W->fac);
        memmove(W->pool, P->pool, sizeof W->pool);
        W->nf = nf;
        irs_reset(&c, m0 + ((sizeof(irkeep) + 7u) & ~(size_t)7u));
        wr = grab(&c, 3 * PF_N * sizeof(rat));
        if (c.st != CASIM_OK) return CASIM_NO_ROOM;
        T = (rat *)(void *)nd(&c, wr);
    }
    nt = rdeg(W->poly) >= 0;
    for (int i = 0; i < W->nf; i++)
        for (int j = 1; j <= W->fac[i].k; j++) {
            irs_num(W, i, j, T);
            nt += rdeg(T) >= 0;
        }
    D = S.skip ? 0 : nt > 1 ? 2 : 1;
    if (!S.skip && nt > 1 && irs_hit(&S, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_SUM))
        irs_int(&w, txt, var), pf_puts(&w, " = "), irs_terms(&w, W, T, var);
    sub.op = CASIM_INTEGRATE, sub.eqn = txt;
    if (rdeg(W->poly) >= 0) {
        out_t tw = {txt, SLOT, 0, false};
        pf_rpoly(&tw, W->poly, var);
        if ((st = irs_sub(&S, &sub, D, c.mem + c.used, c.top - c.used)) != CASIM_OK) return st;
    }
    for (int i = 0; i < W->nf; i++)
        for (int j = 1; j <= W->fac[i].k; j++) {
            size_t mark = c.used;
            irs_num(W, i, j, T);
            if (rdeg(T) < 0) continue;
            if (W->fac[i].deg == 1) st = irs_lin(&S, &c, &W->fac[i], j, T, T, D, &sub, c.mem + c.used, c.top - c.used, rq->eqn);
            else st = irs_quad(&S, &c, &W->fac[i], T, T, D, rq->eqn);
            if (st != CASIM_OK) return st == CASIM_NOT_INTEGRABLE ? CASIM_NO_ROOM : st;
            irs_reset(&c, mark);
        }
    if (!S.skip) {
        if (nt > 1 && irs_hit(&S, 1, CASIM_STEP_RESULT, CASIM_RULE_SUM)) S.want = 1;
        if (irs_hit(&S, 0, CASIM_STEP_RESULT, CASIM_RULE_PARTIAL_FRACTIONS)) S.want = 2;
    }
    info->count = S.at;
    if (k < 0) return CASIM_OK;
    if (k >= S.at) return CASIM_BAD_ARG;
    if (S.want) return irs_answer(rq, S.want, out, cap, arena, arena_cap, txt, room);
    if (w.full) return out[0] = '\0', CASIM_TOO_LONG;
    return CASIM_OK;
}

/* integrate's table refused (do_isteps, inlined in casim_step): this walk. Five arguments, flags riding in info's line
   (do_isteps cleared info and set nothing since), from one expression with finish: measured on the S3, the other
   placements moved casim_step's frame from 240 (six or more arguments 256; a separate line, or this call in finish's
   place, 224) */
static __attribute__((noinline)) casim_status irs_refused(const casim_req *rq, int k, casim_step_info *info,
                                                          const out_t *o, ctx *tc)
{
    return do_irsteps(rq, k, info->line, info, o, tc);
}

/* ---- Taylor polynomials (unit CAS 014) ----
   For k = 0..n: f^(k) valued exactly at a as a definite integral's bound is (check_rewrite, CAS 008's exact values),
   divided by k!; then the next derivative, derive's own (so DEG and GRAD carry their factor by the chain rule). Each
   step compacts the arena down to the derivative and the coefficients so far, so it holds one step at a time. The
   answer is printed in the Taylor spelling (rule 6), not as one canonical sum. */

#define TAYLOR_MAX 12
#define TAYLOR_TEXT 384      /* a derivative's text, twice at the arena's top: past it, NO_ROOM */
#define TAYLOR_TEXT_BIG 1024 /* CAS 016 rule 4: in an arena of TAYLOR_BIG_ARENA or more (the host's and the web's, not
                                the calculator's 8 KB, where a bigger buffer answers fewer: docs/language.md) */
#define TAYLOR_BIG_ARENA 16384

/* the arena back to mark, the constants cached since dropped (they would point into reused room: ir_reset's rule) */
static __attribute__((noinline)) void taylor_drop(ctx *c, size_t mark)
{
    c->used = mark;
    for (int j = 0; j < SMALL; j++)
        if (c->small[j] >= mark / 8u) c->small[j] = 0;
    if (c->pi >= mark / 8u) c->pi = 0;
}

/* one term c×(X-a)^k, canonical, printed after the terms before it ("+" unless it starts with a minus): a 0 term is
   left out. For k = 1 with a != 0, canon would multiply c into (X-a) (2×(X-1) is 2×X-2), so it is printed as
   c×(X-a)^2 with that ^2 taken out of the text */
static __attribute__((noinline)) void taylor_term(ctx *c, out_t *w, ref ck, ref xa, int k, bool at0)
{
    size_t at = w->n;
    ref t;
    if (is_k(c, ck, 0, 1)) return;
    if (k == 0) t = ck;
    else if (k == 1)                    /* num, not small: a new caller of small inlined it into chain (frame) */
        t = canon(c, mul2(c, ck, at0 ? xa : power(c, xa, num(c, &(rat){2, 1}))));
    else t = canon(c, mul2(c, ck, power(c, xa, num(c, &(rat){k, 1}))));
    if (at) puts_(w, "+");
    print(c, w, t);
    if (w->full) return;
    if (k == 1 && !at0) {
        /* the "^2" of the bracketed (X-a): the bracket that opens on the variable, "(X-" or "(X+" (c holds no X, and
           a name's X, as EXP's, follows a letter), to its matching ")" */
        char *s = w->o + at, *e = w->o + w->n;
        for (char *p = s; p + 3 <= e; p++)
            if (p[0] == '(' && p[1] == 'A' + c->var && (p[2] == '-' || p[2] == '+')) {
                int d = 0;
                char *q = p;
                for (; q < e; q++)
                    if (*q == '(') d++;
                    else if (*q == ')' && --d == 0) break;
                if (q + 3 <= e && q[1] == '^' && q[2] == '2' &&
                    (q + 3 == e || ((q[3] < '0' || q[3] > '9') && q[3] != '.'))) {
                    memmove(q + 1, q + 3, (size_t)(e - (q + 3)) + 1);
                    w->n -= 2;
                }
                break;
            }
    }
    if (at && w->o[at + 1] == '-') {                /* "+-c": the term's own minus */
        memmove(w->o + at, w->o + at + 1, w->n - at);
        w->n--;
    }
}

/* a hole in r at a: a function, power or quotient with a put in whose value doesn't exist, though the whole may fold
   it away (0×LN(0) and LN(0)-LN(0) are 0 to canon): a 0 divided, LN, SQRT and the inverse functions outside their
   domains, a fractional power of a negative number, TAN where COS is 0, and SGN of 0, where ABS has its corner
   (derive's ABS' = SGN(u)×u' is right except at u = 0). r is the typed tree as parsed or a canonical f^(k); each node's
   work is dropped after its check, so the arena stays flat */
/* u is exactly 0 at a (CAS 016 rule 2's zero factor); a hole in u is left for taylor_hole to find */
static __attribute__((noinline)) bool taylor_zero(ctx *c, ref u, int var, ref a)
{
    size_t mark = c->used;
    bool z;
    c->sub_var = var, c->sub_val = a;
    u = canon(c, check_rewrite(c, u));
    c->sub_val = 0, c->sub_var = -1;
    z = c->st == CASIM_OK && is_k(c, u, 0, 1);
    if (c->st == CASIM_UNDEFINED) c->st = CASIM_OK;
    taylor_drop(c, mark);
    return z;
}

/* the i-th term or factor of r, for taylor_hole. CAS 016 rule 2: SGN is bounded, so in a product with a factor that is
   0 at a, SGN(u) at u = 0 is 0 there (X×SGN(X): ABS's corner in X×ABS(X)'s derivative), and only u is checked */
static __attribute__((noinline)) ref taylor_part(ctx *c, ref r, int i, int var, ref a)
{
    ref g = list(c, nd(c, r)->a)[i];
    if (nd(c, r)->kind != K_MUL || nd(c, g)->kind != K_FN || nd(c, g)->id != F_SGN) return g;
    for (int j = 0; j < nd(c, r)->n; j++) {
        ref y = list(c, nd(c, r)->a)[j];
        if (!(nd(c, y)->kind == K_FN && nd(c, y)->id == F_SGN) && taylor_zero(c, y, var, a)) return nd(c, g)->a;
    }
    return g;
}

static bool taylor_hole(ctx *c, ref r, int var, ref a)
{
    bool h = false, raw;
    int kind, id;
    if (c->st != CASIM_OK || !enter(c)) return false;
    kind = nd(c, r)->kind, id = nd(c, r)->id, raw = kind >= K_NEG;
    if (((kind == K_FN || kind == K_RFN) && id != F_EXP && id != F_ALOG && id != F_SIN && id != F_COS && id != F_ATAN &&
         id != F_SINH && id != F_COSH && id != F_TANH && id != F_ASINH && id != F_ABS && id != F_SQ) ||
        kind == K_POW || kind == K_BPOW || kind == K_BDIV) {
        size_t mark = c->used;
        bool tan = (kind == K_FN || kind == K_RFN) && id == F_TAN, sgn = (kind == K_FN || kind == K_RFN) && id == F_SGN;
        ref u = tan || sgn ? nd(c, r)->a : r;
        abn x;
        int l = 0, cv;
        c->sub_var = var, c->sub_val = a;           /* a put in: the canonical walk substitutes, canon reads the raw */
        u = canon(c, raw ? u : check_rewrite(c, u));
        if (tan) u = canon(c, fn(c, F_COS, u));                     /* a pole: COS is 0 there */
        c->sub_val = 0, c->sub_var = -1;
        if (c->st == CASIM_UNDEFINED) c->st = CASIM_OK, h = true;
        else if (c->st == CASIM_OK && (tan || sgn)) h = is_k(c, u, 0, 1);
        else if (c->st == CASIM_OK) {
            while (l < 26 && !contains(c, u, l)) l++;
            if (l == 26) {                              /* no letters left: its value, by the core's arithmetic */
                cv = const_val(c, u, &x);
                h = cv == CV_UNDEF || cv == CV_NOTREAL;
            }
        }
        taylor_drop(c, mark);
    }
    if (!h && (kind == K_FN || kind == K_RFN || kind == K_NEG || kind == K_GROUP)) h = taylor_hole(c, nd(c, r)->a, var, a);
    else if (!h && (kind == K_POW || kind == K_BPOW || kind == K_BDIV || kind == K_BMUL || kind == K_BADD || kind == K_BSUB))
        h = taylor_hole(c, nd(c, r)->a, var, a) || taylor_hole(c, nd(c, r)->b, var, a);
    else if (!h && (kind == K_ADD || kind == K_MUL))
        for (int i = 0; i < nd(c, r)->n && !h; i++) h = taylor_hole(c, taylor_part(c, r, i, var, a), var, a);
    c->depth--;
    return h;
}

/* r is SGN(u) with u 0 at a: a corner */
static bool sgn_at0(ctx *c, ref r, int var, ref a)
{
    return nd(c, r)->kind == K_FN && nd(c, r)->id == F_SGN && taylor_zero(c, nd(c, r)->a, var, a);
}

/* the corners in a canonical r (a guarded walk) */
static int sgn_count(ctx *c, ref r, int var, ref a)
{
    const node *x = nd(c, r);
    int k = 0;
    if (r == 0 || r == SPARE || !enter(c)) return 0;
    if (x->kind == K_FN) k = sgn_at0(c, r, var, a) + sgn_count(c, nd(c, r)->a, var, a);
    else if (x->kind == K_POW) k = sgn_count(c, x->a, var, a) + sgn_count(c, nd(c, r)->b, var, a);
    else if (x->kind == K_ADD || x->kind == K_MUL)
        for (int i = 0; i < x->n; i++) k += sgn_count(c, list(c, nd(c, r)->a)[i], var, a);
    c->depth--;
    return k;
}

/* u changes sign at a: its first derivative that isn't 0 there has an odd order (1), an even one (0), or it can't
   tell within four (-1) */
static __attribute__((noinline)) int taylor_turns(ctx *c, ref u, int var, ref a)
{
    for (int m = 1; m <= 4 && c->st == CASIM_OK; m++) {
        ref v;
        u = canon(c, deriv(c, u));
        c->sub_var = var, c->sub_val = a;
        v = canon(c, check_rewrite(c, u));
        c->sub_val = 0, c->sub_var = -1;
        if (c->st != CASIM_OK) break;
        if (!is_k(c, v, 0, 1)) return const_sign(c, v) ? m % 2 : -1;
    }
    return -1;
}

/* rule 7: f (the typed tree as parsed, or a canonical f^(k)) has a value at a, else UNDEFINED */
static __attribute__((noinline)) void taylor_check(ctx *c, ref f, int var, ref a)
{
    size_t mark = c->used;
    if (c->st == CASIM_OK && taylor_hole(c, f, var, a)) c->st = CASIM_UNDEFINED;
    if (c->st == CASIM_OK) taylor_drop(c, mark);
}

/* r has a hole at a, through taylor_check (so taylor_hole keeps one caller shape and its frame) */
static __attribute__((noinline)) bool taylor_has_hole(ctx *c, ref r, int var, ref a)
{
    taylor_check(c, r, var, a);
    if (c->st != CASIM_UNDEFINED) return false;
    c->st = CASIM_OK;
    return true;
}

/* a hole in f at a outside the corner g (in its term t): another term, another factor of t, or g's argument */
static __attribute__((noinline)) bool taylor_rest_hole(ctx *c, ref f, ref t, ref g, int var, ref a)
{
    if (taylor_has_hole(c, nd(c, g)->a, var, a)) return true;
    if (nd(c, f)->kind == K_ADD)
        for (int i = 0; i < nd(c, f)->n; i++)
            if (list(c, nd(c, f)->a)[i] != t && taylor_has_hole(c, list(c, nd(c, f)->a)[i], var, a)) return true;
    if (nd(c, t)->kind == K_MUL)
        for (int i = 0; i < nd(c, t)->n; i++)
            if (list(c, nd(c, t)->a)[i] != g && taylor_has_hole(c, list(c, nd(c, t)->a)[i], var, a)) return true;
    return false;
}

/* a 0÷0 in r at a: a product with a factor that is 0 there beside a negative power of something 0 there (a guarded
   walk). In a derivative's formula it says only that the formula can't tell (X×SQRT(X^2)'s derivative holds
   X^2×(X^2)^(-1÷2)): abacus #7951 */
static int taylor_00(ctx *c, ref r, int var, ref a)
{
    const node *x = nd(c, r);
    int z = 0;
    if (r == 0 || r == SPARE || !enter(c)) return 0;
    if (x->kind == K_FN) z = taylor_00(c, x->a, var, a);
    else if (x->kind == K_POW) z = taylor_00(c, x->a, var, a) || taylor_00(c, nd(c, r)->b, var, a);
    else if (x->kind == K_ADD || x->kind == K_MUL) {
        int under = -1, zero = -1;
        for (int i = 0; i < nd(c, r)->n && !z; i++) {
            ref y = list(c, nd(c, r)->a)[i];
            const node *p = nd(c, y);
            if (nd(c, r)->kind == K_MUL && under < 0 && p->kind == K_POW && is_num(c, p->b) && val(c, p->b)->p < 0 &&
                taylor_zero(c, p->a, var, a)) under = i;
            else if (nd(c, r)->kind == K_MUL && zero < 0 && taylor_zero(c, y, var, a)) zero = i;
            z = taylor_00(c, y, var, a);
        }
        z = z || (under >= 0 && zero >= 0);
    }
    c->depth--;
    return z;
}

/* a hole in f at a (CAS 016 rule 2): UNDEFINED, but UNDECIDED for an SGN(0) that is not a lone corner. A lone corner is
   f's one SGN, to the first power, a term of f or a factor of one beside factors whose value at a is decided and not
   0, its argument changing sign at a: there f has a jump, so the derivative before it has no value (ABS(X) to order 1,
   X×ABS(X) to order 2) */
static __attribute__((noinline)) casim_status taylor_corner(ctx *c, ref f, int var, ref a, int k)
{
    size_t mark = c->used;
    int n = sgn_count(c, f, var, a), lone = 0;
    ref t = f, g = 0;
    if (n == 0) {
        /* no corner: UNDEFINED, but a 0÷0 in a derivative's formula (not the typed input's: rule 7) is UNDECIDED */
        lone = k > 0 && taylor_00(c, f, var, a);
        if (c->st == CASIM_UNDEFINED) c->st = CASIM_OK;
        if (c->st != CASIM_OK) return c->st;
        taylor_drop(c, mark);
        return lone ? CASIM_UNDECIDED : CASIM_UNDEFINED;
    }
    if (n == 1 && nd(c, f)->kind == K_ADD)
        for (int i = 0; i < nd(c, f)->n; i++)
            if (sgn_count(c, list(c, nd(c, f)->a)[i], var, a)) t = list(c, nd(c, f)->a)[i];
    if (n == 1 && sgn_at0(c, t, var, a)) g = t, lone = 1;
    else if (n == 1 && nd(c, t)->kind == K_MUL) {
        lone = 1;
        for (int i = 0; i < nd(c, t)->n && lone; i++) {
            ref y = list(c, nd(c, t)->a)[i];
            if (sgn_at0(c, y, var, a)) g = y;
            else {
                c->sub_var = var, c->sub_val = a;
                y = canon(c, check_rewrite(c, y));
                c->sub_val = 0, c->sub_var = -1;
                lone = c->st == CASIM_OK && !is_k(c, y, 0, 1) && const_sign(c, y) != 0;
            }
        }
    }
    /* and nothing else in f has a hole, which could cancel the jump (ABS(X)-SQRT(X^2) is 0: review) */
    if (lone && g) lone = !taylor_rest_hole(c, f, t, g, var, a) && taylor_turns(c, nd(c, g)->a, var, a) == 1;
    if (c->st == CASIM_UNDEFINED) c->st = CASIM_OK;
    if (c->st != CASIM_OK) return c->st;
    taylor_drop(c, mark);
    return lone && g ? CASIM_UNDEFINED : CASIM_UNDECIDED;
}

static __attribute__((noinline)) casim_status do_taylor(const char *eqn, int var, const char *point, int n, int angle,
                                                        char *out, size_t cap, void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref f = SPARE, a, xa;
    char *tb;
    abn av;
    rat kf = {1, 1};
    size_t ocap = cap < SLOT ? cap : SLOT, base, tt;
    int chars = 0;
    bool at0;
    out_t w = {out, ocap, 0, false};
    if (cap) out[0] = '\0';
    if (!eqn || !point || n < 0 || n > TAYLOR_MAX) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.var = var;                        /* the names derive refuses are refused here too (not opaque): it derives */
    c.sub_var = -1;
    if (c.st == CASIM_OK) f = parse(&c);
    if (c.st == CASIM_OK && nd(&c, f)->kind == K_REQ) return CASIM_BAD_ARG;        /* an equation */
    for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    {
        ref raw = f;
        a = c.st == CASIM_OK ? read_bound(&c, point, chars + 1, &av) : SPARE;    /* rule 3 */
        c.s = eqn;
        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
        /* rule 7: the typed f decides. Each part of the tree as typed is valued at a before canon could cancel or fold
           it (X÷X at 0 is 0÷0, 0×LN(X) has LN(0)); the derivatives come from the canonical f */
        taylor_check(&c, raw, var, a);
        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
        f = canon(&c, raw);
        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    }
    at0 = is_k(&c, a, 0, 1);
    xa = at0 ? leaf(&c, K_VAR, var) : canon(&c, add2(&c, leaf(&c, K_VAR, var), neg(&c, a)));
    /* two text buffers at the arena's top, out of the nodes' reach: each derivative is printed into one, the arena goes
       back to base, and it is read again (a canonical form prints exactly), so a step holds one derivative and its
       work; c.s is the buffer the current tree was read from (a K_LIT keeps its text there), so they alternate */
    tt = arena_cap >= TAYLOR_BIG_ARENA ? TAYLOR_TEXT_BIG : TAYLOR_TEXT;
    if (c.top < c.used + 2 * tt + 64) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
    c.top -= 2 * tt;
    tb = (char *)c.mem + c.top;
    base = c.used;
    c.opaque = c.own_text = true;               /* from here the parser reads only the derivatives casim printed */
    for (int k = 0; k <= n && c.st == CASIM_OK; k++) {
        ref v;
        rat inv;
        if (k > 0 && !rat_mul(&kf, &kf, &(rat){k, 1})) return c.st = CASIM_NO_ROOM, finish(&c, SPARE, out, ocap);
        c.sub_var = var, c.sub_val = a;
        v = check_rewrite(&c, f);                           /* f^(k)(a), exact */
        c.sub_val = 0, c.sub_var = -1;
        taylor_check(&c, f, var, a);                        /* rule 7: it has a value there */
        if (c.st == CASIM_UNDEFINED) c.st = CASIM_OK, c.st = taylor_corner(&c, f, var, a, k);   /* CAS 016 rule 2 */
        if (c.st != CASIM_OK) break;
        rat_make(&inv, kf.q, kf.p);
        taylor_term(&c, &w, canon(&c, mul2(&c, v, num(&c, &inv))), xa, k, at0);    /* c_k's term, into out */
        if (w.full || k == n || c.st != CASIM_OK) break;
        {
            char *buf = tb + (k % 2) * tt;
            out_t tw = {buf, tt, 0, false};
            print(&c, &tw, canon(&c, deriv(&c, f)));
            if (c.st != CASIM_OK) break;
            if (tw.full) {
                if (ocap) out[0] = '\0';
                return c.st = CASIM_NO_ROOM;
            }
            taylor_drop(&c, base);
            c.s = buf;
            f = canon(&c, parse(&c));
        }
    }
    c.s = eqn;
    if (c.st == CASIM_OK && !w.full && w.n == 0) puts_(&w, "0");
    if ((c.st != CASIM_OK || w.full) && ocap) out[0] = '\0';
    if (c.st != CASIM_OK) return c.st;
    return w.full ? CASIM_TOO_LONG : CASIM_OK;
}

casim_status casim_taylor(const char *eqn, int var, const char *point, int n, int model, int angle, char *out,
                          size_t cap, void *arena, size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_taylor(eqn, var, point, n, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- integration by substitution and by parts (unit CAS 017) ----
   When the table and the rational path refuse (work/017-substitution-and-parts.md). The canonical integrand, term by
   term: the table and the rational path on each term's printed form, read back as typed; then substitution (rule 3)
   and parts (rule 4), whose pieces are integrated by this whole procedure, at most IB_DEPTH deep and IB_TRIES tries an
   integrand. Every answer is proven from checked pieces before it is kept: the table's and the rational path's their
   own way, a substitution's by G checked in U and G(u)×u' against the term, a part's by (uv)' = term + v×u' with
   ∫v×u' checked where it was made, the loop's and the cycle's whole. */

#define IB_DEPTH 4
#define IB_TRIES 160
#define IB_TEXT 384                     /* a term's printed form, read back by the table or the rational path */
#define IB_CANDS 12
#define IB_WHOLE 1000000                /* S2.6: the whole form's largest scale */

/* the integrand's own choice, for its steps (rule 8): the plan (ib_choose) replays ib's order at depth 0, and r holds
   u, g (substitution); u, v, w, and u2, v2, k (parts, the cycle); k (a constant factor). Only substitution's u and g
   are noted during the search (ib_inner), as a note in ib_parts moved ib's frame (240 -> 256 on the host, measured) */
enum { IBH_NONE, IBH_SUB, IBH_PARTS, IBH_CYCLE, IBH_CONST };

typedef struct {
    int tries;
    bool ranout;                        /* a try anywhere ran out (the rational path at any depth, the arena, the depth
                                           guard): NO_ROOM when nothing answers, never NOT_INTEGRABLE (abacus #8359) */
    int how;                            /* the choice at depth 0 */
    ref r[6];
} ib_t;

static ref ib(ctx *c, ib_t *B, ref t, int var, int d);

/* the choice at depth 0, for the steps: out of line, so that ib's frame stays as it was */
static __attribute__((noinline)) void ib_rec(ib_t *B, int how, int at, ref a, ref b, ref c)
{
    B->how = how, B->r[at] = a, B->r[at + 1] = b, B->r[at + 2] = c;
}

/* canon, called once: the section's calls through this one leave gcc's inlining of canon elsewhere as it was (an
   out-of-line canon appeared when they called it directly) */
static __attribute__((noinline)) ref ib_canon(ctx *c, ref r)
{
    return canon(c, r);
}

/* neg, called once, for the same reason: chain's frame moved 64 -> 48 on the S3 when the section called neg itself
   (measured: wrapping canon alone left it at 48, neg alone gave 64) */
static __attribute__((noinline)) ref ib_neg(ctx *c, ref r)
{
    return neg(c, r);
}

/* 1÷r, canonical; out of line so that ib_keeps's frame stays as it was (80 -> 112 on the host with power inline) */
static __attribute__((noinline)) ref ib_inv(ctx *c, ref r)
{
    return ib_canon(c, power(c, r, small(c, C_M1)));
}

/* the arena back to mark after a try that refused or ran out: room run out deep in the search (the arena, or the
   depth guard) is a refusal there, as rule 6 keeps NO_ROOM for a term of the input's own sum (ib_rational's) */
static bool ib_fail(ctx *c, size_t mark)
{
    c->st = CASIM_OK;
    c->starved = false;
    taylor_drop(c, mark);
    return true;
}

/* t printed into a buffer under the arena's top; NOT_INTEGRABLE when it doesn't fit */
static char *ib_print(ctx *c, ref t, size_t len)
{
    char *buf;
    out_t tw;
    if (c->top < c->used + len + 256) return c->st = CASIM_NO_ROOM, NULL;
    c->top -= len;
    buf = (char *)c->mem + c->top;
    tw = (out_t){buf, IB_TEXT, 0, false};
    print(c, &tw, t);
    if (c->st == CASIM_OK && tw.full) c->st = CASIM_NOT_INTEGRABLE;
    return c->st == CASIM_OK ? buf : NULL;
}

/* the table (CAS 006), as typed, on t's printed form */
static ref ib_table(ctx *c, ref t, int var)
{
    size_t top = c->top;
    const char *s = c->s;
    int v0 = c->var;
    char *buf = ib_print(c, t, IB_TEXT);
    ref F = SPARE;
    c->var = var;
    if (buf) {
        c->s = buf;
        F = integ(c, parse(c), var);
        if (c->st == CASIM_OK) F = ib_canon(c, F);
    }
    c->var = v0, c->s = s, c->top = top;
    return c->st == CASIM_OK ? F : SPARE;
}

/* the product of t's factors free of var that aren't numbers (π, LN(2), A, 180÷π from DEG's ATAN), or 0 when none:
   the rational path takes only rational coefficients */
static ref ib_const(ctx *c, ref t, int var)
{
    ref k = 0;
    if (nd(c, t)->kind != K_MUL) return 0;
    for (int i = 0; i < nd(c, t)->n; i++) {
        ref f = list(c, nd(c, t)->a)[i];
        if (!is_num(c, f) && !contains(c, f, var)) k = k ? mul2(c, k, f) : f;
    }
    return k;
}

/* t as partfrac reads a rational function of var: numbers, var, sums, products and whole powers (no other letter,
   no function, no π) */
static __attribute__((noinline)) bool ib_ratfn(ctx *c, ref t, int var)
{
    const node *x = nd(c, t);
    if (x->kind == K_NUM) return true;
    if (x->kind == K_VAR) return x->id == var;
    if (x->kind == K_POW) return is_num(c, x->b) && val(c, x->b)->q == 1 && ib_ratfn(c, x->a, var);
    if (x->kind != K_ADD && x->kind != K_MUL) return false;
    for (int i = 0; i < x->n; i++)
        if (!ib_ratfn(c, list(c, x->a)[i], var)) return false;
    return true;
}

/* the rational path (CAS 013) on t's printed form, in the arena's free part, its answer read back; a constant factor
   ib_const finds comes out first and goes back on the answer. Its NO_ROOM is a refusal here: other rules may still
   answer, and at any depth it is noted, so that when nothing answers the whole is NO_ROOM (abacus #8359) */
static ref ib_rational(ctx *c, ib_t *B, ref t, int var)
{
    size_t top = c->top;
    const char *s = c->s;
    ref k = ib_const(c, t, var), u = k ? ib_canon(c, mul2(c, t, ib_inv(c, k))) : t;
    char *buf = ib_print(c, u, 2 * IB_TEXT), *ob = buf + IB_TEXT;
    casim_status st;
    ref F = SPARE;
    if (buf && c->st == CASIM_OK) {
        /* where partfrac can't start in the room left, the rational path's refusal is the room's for a rational u
           (abacus #8413: SIN(X)+1÷(3×X^2+2×X+8) at 8 KB), and only the form's for any other */
        if (c->top - c->used < ir_need(c->mem + c->used) && ib_ratfn(c, u, var)) st = CASIM_NO_ROOM;
        else st = do_irational(buf, var, c->angle, ob, IB_TEXT, c->mem + c->used, c->top - c->used, c->rtl);
        if (st == CASIM_OK) {
            c->s = ob;
            F = ib_canon(c, parse(c));
            if (k && nd(c, F)->kind == K_ADD) {        /* term by term: -90×LN(…)÷π+ATAN(X+1), not -90×(LN(…)-…)÷π */
                ref acc = small(c, C_0);
                for (int i = 0; i < nd(c, F)->n; i++) acc = add2(c, acc, mul2(c, k, list(c, nd(c, F)->a)[i]));
                F = ib_canon(c, acc);
            } else if (k) F = ib_canon(c, mul2(c, k, F));
        } else {
            if (st == CASIM_NO_ROOM) B->ranout = true;
            c->st = CASIM_NOT_INTEGRABLE;
        }
    }
    c->s = s, c->top = top;
    return c->st == CASIM_OK ? F : SPARE;
}

/* r with every subtree equal to u replaced by the letter U, canonical */
static ref ib_replace(ctx *c, ref r, ref u, int U)
{
    const node *x = nd(c, r);
    ref v = r;
    if (c->st != CASIM_OK || !enter(c)) return SPARE;
    if (cmp(c, r, u) == 0) v = leaf(c, K_VAR, U);
    else if (x->kind == K_FN) v = fn(c, x->id, ib_replace(c, x->a, u, U));
    else if (x->kind == K_POW) {
        ref b = ib_replace(c, x->a, u, U), e = ib_replace(c, nd(c, r)->b, u, U);
        v = power(c, b, e);
    } else if (x->kind == K_ADD || x->kind == K_MUL) {
        int n = x->n, kind = x->kind;
        ref l = grab(c, (size_t)n * sizeof(ref));
        if (l) {
            for (int i = 0; i < n; i++) {
                ref y = ib_replace(c, list(c, nd(c, r)->a)[i], u, U);
                list(c, l)[i] = y;
            }
            v = kind == K_ADD ? add(c, list(c, l), n) : mul(c, list(c, l), n);
        }
    }
    c->depth--;
    return v;
}

/* rule 3's candidates for u: each function holding var, its argument and each power's base, in tree order */
static void ib_cands(ctx *c, ref r, int var, ref *cs, int *n)
{
    const node *x = nd(c, r);
    if (*n >= IB_CANDS || !contains(c, r, var) || x->kind == K_VAR || !enter(c)) return;
    if (x->kind == K_FN) {
        cs[(*n)++] = r;
        ib_cands(c, x->a, var, cs, n);
        if (*n < IB_CANDS && nd(c, x->a)->kind != K_VAR) cs[(*n)++] = x->a;
    } else if (x->kind == K_POW) {
        ib_cands(c, x->a, var, cs, n);
        if (*n < IB_CANDS && nd(c, x->a)->kind != K_VAR) cs[(*n)++] = x->a;
    } else if (x->kind == K_ADD || x->kind == K_MUL)
        for (int i = 0; i < nd(c, r)->n && *n < IB_CANDS; i++) ib_cands(c, list(c, nd(c, r)->a)[i], var, cs, n);
    c->depth--;
}

static bool ib_den(ctx *c, ref t, ref b);

/* r is never 0 or negative for real letters: a positive constant, π, an even whole power, a product of these, and a sum
   of such terms with a positive constant among them (EXP(X)×SIN(A×X)'s A^2+1, in DEG A^2×π^2+32400) */
static bool ib_semi(ctx *c, ref r)
{
    const node *x = nd(c, r);
    if (x->kind == K_NUM) return val(c, r)->p > 0;
    if (x->kind == K_PI) return true;
    if (x->kind == K_POW) return is_num(c, x->b) && val(c, x->b)->q == 1 && val(c, x->b)->p % 2 == 0;
    if (x->kind == K_MUL) {
        for (int i = 0; i < x->n; i++)
            if (!ib_semi(c, list(c, nd(c, r)->a)[i])) return false;
        return true;
    }
    return false;
}

static bool ib_positive(ctx *c, ref r)
{
    const node *x = nd(c, r);
    bool k = false;
    if (x->kind != K_ADD) return x->kind == K_NUM ? val(c, r)->p > 0 : x->kind == K_PI;
    for (int i = 0; i < x->n; i++) {
        ref y = list(c, nd(c, r)->a)[i];
        if (!ib_semi(c, y)) return false;
        k = k || nd(c, y)->kind == K_NUM || nd(c, y)->kind == K_PI;
    }
    return k;
}

/* rule 2: every expression of another letter that r divides by (one without r's var) is one t divides by already:
   a parameter may be 0, and the answer must have a value wherever the integrand has one (SIN(A×X)×COS(X)'s cycle
   divides by A^2-1, X×EXP(A×X^2)'s substitution by A) */
static bool ib_letters_kept(ctx *c, ref r, int var, ref t)
{
    const node *x = nd(c, r);
    bool ok = true;
    if (!enter(c)) return false;
    if (x->kind == K_POW && nd(c, x->b)->kind == K_NUM && val(c, x->b)->p < 0 && !contains(c, x->a, var)) {
        bool letter = false;
        for (int l = 0; l < 26 && !letter; l++) letter = contains(c, x->a, l);
        if (letter && !ib_positive(c, x->a)) ok = ib_den(c, t, x->a);
    }
    if (ok && (x->kind == K_POW || x->kind == K_FN))
        ok = ib_letters_kept(c, x->a, var, t) && (x->kind != K_POW || ib_letters_kept(c, nd(c, r)->b, var, t));
    else if (ok && (x->kind == K_ADD || x->kind == K_MUL))
        for (int i = 0; i < x->n && ok; i++) ok = ib_letters_kept(c, list(c, nd(c, r)->a)[i], var, t);
    c->depth--;
    return ok;
}

/* r with each SGN(u)×ABS(u)^-1 written u^-1, as the check writes it (both have no value at u = 0): LN(ABS(u))'s
   derivative, so a typed LN(ABS(X)) and the table's own answers can be integrated again (the review) */
static ref ib_sgnabs(ctx *c, ref r)
{
    const node *x = nd(c, r);
    ref v = r;
    if (c->st != CASIM_OK || !enter(c)) return SPARE;
    if (x->kind == K_FN) v = fn(c, x->id, ib_sgnabs(c, x->a));
    else if (x->kind == K_POW) {
        ref b = ib_sgnabs(c, x->a), e = ib_sgnabs(c, nd(c, r)->b);
        v = power(c, b, e);
    } else if (x->kind == K_ADD || x->kind == K_MUL) {
        int n = x->n, kind = x->kind;
        ref l = grab(c, (size_t)n * sizeof(ref));
        if (l) {
            for (int i = 0; i < n; i++) {
                ref y = ib_sgnabs(c, list(c, nd(c, r)->a)[i]);
                list(c, l)[i] = y;
            }
            for (int i = 0; kind == K_MUL && i < n; i++) {
                const node *g = nd(c, list(c, l)[i]);
                if (g->kind != K_FN || g->id != F_SGN) continue;
                for (int j = 0; j < n; j++) {
                    const node *h = nd(c, list(c, l)[j]);
                    if (j == i || h->kind != K_POW || !is_k(c, h->b, -1, 1) || nd(c, h->a)->kind != K_FN ||
                        nd(c, h->a)->id != F_ABS || cmp(c, nd(c, h->a)->a, g->a) != 0)
                        continue;
                    list(c, l)[i] = power(c, g->a, small(c, C_M1));
                    list(c, l)[j] = small(c, C_1);
                    break;
                }
            }
            v = kind == K_ADD ? add(c, list(c, l), n) : mul(c, list(c, l), n);
        }
    }
    c->depth--;
    return v;
}

/* u' (in c->var, set by the caller), canonical, with ib_sgnabs's rewrite */
static __attribute__((noinline)) ref ib_deriv(ctx *c, ref u)
{
    return ib_canon(c, ib_sgnabs(c, ib_canon(c, deriv(c, u))));
}

/* rule 2: what the procedure can't take. A literal canon kept as typed (1.2.0, past 2^62): its text is the input's,
   and a printed copy read back would point into a buffer that goes. And, holding var, the functions derive gives no
   true derivative where it matters: SGN, ABS but in LN(ABS(...)) (the table's own form, smooth where it has a value),
   and the opaque names (IP, FP), whose derivative derive takes as 0: a part or a check would treat them as constants */
static bool ib_bad(ctx *c, ref r, int var)
{
    const node *x = nd(c, r);
    bool b = false;
    if (!enter(c)) return true;
    if (x->kind == K_LIT) b = true;
    else if ((x->kind == K_OFN || (x->kind == K_FN && (x->id == F_SGN || x->id == F_ABS))) && contains(c, r, var)) b = true;
    else if (x->kind == K_POW && is_num(c, x->a) && val(c, x->a)->p <= 0 && contains(c, x->b, var)) b = true;   /* 0^X */
    else if (x->kind == K_FN && x->id == F_LN && nd(c, x->a)->kind == K_FN && nd(c, x->a)->id == F_ABS)
        b = ib_bad(c, nd(c, x->a)->a, var);
    else if (x->kind == K_FN || x->kind == K_POW || x->kind == K_OFN)
        b = ib_bad(c, x->a, var) || ((x->kind == K_POW || (x->kind == K_OFN && nd(c, r)->b)) && ib_bad(c, nd(c, r)->b, var));
    else if (x->kind == K_ADD || x->kind == K_MUL)
        for (int i = 0; i < x->n && !b; i++) b = ib_bad(c, list(c, nd(c, r)->a)[i], var);
    c->depth--;
    return b;
}

/* a - b expands to 0 */
static bool ib_same(ctx *c, ref a, ref b)
{
    ref t = add2(c, a, ib_neg(c, b));
    c->sub_var = -1;
    t = expand_fully(c, check_rewrite(c, t));
    return c->st == CASIM_OK && is_k(c, t, 0, 1);
}

/* r with var replaced by v, canonical (check_rewrite's substitution) */
static ref ib_at(ctx *c, ref r, int var, ref v)
{
    c->sub_var = var, c->sub_val = v;
    r = check_rewrite(c, r);
    c->sub_val = 0, c->sub_var = -1;
    return ib_canon(c, r);
}

/* rule 3's back-substitution: u = a×X^k + b, so X is ((U - b)÷a)^(1÷k) in g; the caller refuses when X is left */
static ref ib_back(ctx *c, ref g, ref u, int var, int U)
{
    const node *x = nd(c, u);
    ref a = small(c, C_1), b = small(c, C_0), xk = SPARE;
    if (x->kind != K_ADD) return SPARE;
    for (int i = 0; i < x->n; i++) {
        ref o = list(c, nd(c, u)->a)[i];
        if (!contains(c, o, var)) b = add2(c, b, o);
        else if (xk == SPARE) xk = o;
        else return SPARE;
    }
    if (xk != SPARE && nd(c, xk)->kind == K_MUL) {      /* a × X^k: canon puts the constant first */
        ref k0 = list(c, nd(c, xk)->a)[0];
        if (nd(c, xk)->n != 2 || contains(c, k0, var)) return SPARE;
        a = k0, xk = list(c, nd(c, xk)->a)[1];
    }
    if (xk == SPARE || nd(c, xk)->kind != K_POW || nd(c, nd(c, xk)->a)->kind != K_VAR || nd(c, nd(c, xk)->a)->id != var)
        return SPARE;
    return ib_at(c, g, var, power(c, mul2(c, add2(c, leaf(c, K_VAR, U), ib_neg(c, b)), power(c, a, small(c, C_M1))),
                                  power(c, nd(c, xk)->b, small(c, C_M1))));
}

/* S2.6: whether a×X^2+b×X+c (whole) has no real root, b^2 < 4ac; false when either side overflows (the cold review:
   a wrapped product dropped an ABS) */
static bool ib_noroot(int64_t a, int64_t b, int64_t c)
{
    int64_t b2, ac, ac4;
    return !__builtin_mul_overflow(b, b, &b2) && !__builtin_mul_overflow(a, c, &ac) &&
           !__builtin_mul_overflow(ac, (int64_t)4, &ac4) && b2 < ac4;
}

/* S2.6: u (a sum) with whole coefficients, none in common, its highest power's positive (5×X^3+2, not X^3+0.4), or
   SPARE when u isn't a polynomial in var; *none when it is a quadratic with no real root, *scale the w÷u. u is read as
   printed, as pw reads typed trees */
static __attribute__((noinline)) ref ib_whole(ctx *c, ref u, int var, bool *none, rat *scale)
{
    size_t top = c->top;
    const char *s0 = c->s;
    char *buf;
    rat *p, r;
    int64_t l = 1, g = 0;
    int d;
    ref w = SPARE, raw;
    *none = false;
    if (nd(c, u)->kind != K_ADD || (buf = ib_print(c, u, IB_TEXT)) == NULL) return c->st = CASIM_OK, c->top = top, SPARE;
    c->s = buf;
    raw = parse(c);
    if (c->st == CASIM_OK && poly_of(c, raw, var, &p) && (d = pdeg(p)) >= 1) {
        for (int k = 0; k <= d && l; k++)                   /* |p|, q and l up to IB_WHOLE: no product overflows */
            if (p[k].p) l = p[k].p > IB_WHOLE || p[k].p < -IB_WHOLE ? 0 : l / gcd64(l, p[k].q) * p[k].q,
                        l = l > IB_WHOLE ? 0 : l;
        for (int k = 0; k <= d && l; k++)
            if (p[k].p) g = gcd64(g, p[k].p * (l / p[k].q));
        if (l && g) {                       /* the scale l÷g, made here: rat_make's callers stay as they were (lm_sum's frame) */
            int64_t h = gcd64(l, g);
            r.p = (p[d].p < 0 ? -l : l) / h, r.q = g / h;
            bool ok = true;
            for (int k = 0; k <= d && ok; k++) ok = rat_mul(&p[k], &p[k], &r) && p[k].q == 1;
            *scale = r;
            if (ok && d == 2) *none = ib_noroot(p[2].p, p[1].p, p[0].p);
            if (ok) w = ib_canon(c, poly_tree(c, p, var));
        }
    }
    if (c->st != CASIM_OK) c->st = CASIM_OK, w = SPARE;
    c->s = s0, c->top = top;
    return w;
}

/* S2.6: a sum with no real root, each term >= 0 (an even power, EXP or COSH, a product of those) and one > 0 */
static bool ib_rootless(ctx *c, ref u)
{
    const node *x = nd(c, u);
    bool pos = false;
    if (x->kind != K_ADD) return false;
    for (int i = 0; i < x->n; i++) {
        ref y = list(c, nd(c, u)->a)[i];
        const node *z = nd(c, y);
        if (z->kind == K_FN && (z->id == F_EXP || z->id == F_COSH)) pos = true;
        else if (!ib_semi(c, y)) return false;
        else pos = pos || z->kind == K_NUM || z->kind == K_PI;
    }
    return pos;
}

/* S2.6: each LN(ABS(a)) in F, a a sum, as LN(ABS(w)), w a's whole form (the two differ by a constant), or LN(w) where w
   has no real root, as CAS 013 writes its answers */
static __attribute__((noinline)) ref ib_lnform(ctx *c, ref r, int var)
{
    const node *x = nd(c, r);
    ref v = r;
    if (c->st != CASIM_OK || !enter(c)) return SPARE;
    if (x->kind == K_FN && x->id == F_LN && nd(c, x->a)->kind == K_FN && nd(c, x->a)->id == F_ABS &&
        nd(c, nd(c, x->a)->a)->kind == K_ADD) {
        ref a = nd(c, nd(c, r)->a)->a, w;
        bool none = false;
        rat sc;
        w = ib_whole(c, a, var, &none, &sc);
        if (w == SPARE) w = a;
        none = none || ib_rootless(c, a);
        v = fn(c, F_LN, none ? w : fn(c, F_ABS, w));
    } else if (x->kind == K_FN) v = fn(c, x->id, ib_lnform(c, x->a, var));
    else if (x->kind == K_POW) {
        ref b = ib_lnform(c, x->a, var), e = ib_lnform(c, nd(c, r)->b, var);
        v = power(c, b, e);
    } else if (x->kind == K_ADD || x->kind == K_MUL) {
        int n = x->n, kind = x->kind;
        ref l = grab(c, (size_t)n * sizeof(ref));
        if (l) {
            for (int i = 0; i < n; i++) {
                ref y = ib_lnform(c, list(c, nd(c, r)->a)[i], var);
                list(c, l)[i] = y;
            }
            v = kind == K_ADD ? add(c, list(c, l), n) : mul(c, list(c, l), n);
        }
    }
    c->depth--;
    return v;
}

/* A test build (tests/ibcheck_check.c) defines this to spoil G before it is integrated, so that substitution's check
   (G(u)×u' against the term), which no input reaches as built, is seen to refuse */
#ifndef CASIM_IB_PLANT
#define CASIM_IB_PLANT(c, g) (g)
#endif

/* rule 3: ∫ t by substitution, t = G(u)×u' with G integrated in a new letter U */
static const char ib_pick[] = "UVWTSRQ";              /* substitution's letters, in order */

/* ib_fail in the search: a try that ran out is noted (abacus #8359) */
static __attribute__((noinline)) bool ib_failr(ctx *c, ib_t *B, size_t mark)
{
    if (c->st == CASIM_NO_ROOM) B->ranout = true;
    return ib_fail(c, mark);
}

/* ib_fail after a refused substitution, its note at depth 0 dropped too: the note points into what the arena gives back
   (out of line, as a store in ib moved its frame 240 -> 256) */
static __attribute__((noinline)) bool ib_unnote(ctx *c, ib_t *B, size_t mark, int d)
{
    if (d == 0) B->how = IBH_NONE;
    return ib_failr(c, B, mark);
}

/* substitution's G integrated a level down, its choice noted first at depth 0 (the last one noted is the one kept):
   out of line, so that the note leaves ib's frame as it was */
static __attribute__((noinline)) ref ib_inner(ctx *c, ib_t *B, ref g, int U, int d, ref u)
{
    if (d == 1) ib_rec(B, IBH_SUB, 0, u, g, SPARE);
    return ib(c, B, g, U, d);
}

static ref ib_sub(ctx *c, ib_t *B, ref t, int var, int d)
{
    const char *pick = ib_pick;
    ref cs[IB_CANDS];
    int n = 0, U = -1, v0 = c->var;
    size_t mark;
    for (int i = 0; pick[i] && U < 0; i++)
        if (pick[i] - 'A' != var && !contains(c, t, pick[i] - 'A')) U = pick[i] - 'A';
    if (U < 0) return c->st = CASIM_NOT_INTEGRABLE, SPARE;
    ib_cands(c, t, var, cs, &n);
    mark = c->used;
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref u = cs[i], du, q, g, H, F;
        c->var = var;
        du = ib_deriv(c, u);
        if (c->st == CASIM_OK && contains(c, du, var)) {             /* a u' without var is the table's linear case */
            q = ib_canon(c, mul2(c, t, power(c, du, small(c, C_M1))));
            g = ib_canon(c, ib_replace(c, q, u, U));
            if (c->st == CASIM_OK && contains(c, g, var)) g = ib_back(c, g, u, var, U);
            if (c->st == CASIM_OK && g != SPARE && !contains(c, g, var) &&
                ib_letters_kept(c, g, U, t)) {
                g = CASIM_IB_PLANT(c, g);
                H = ib_inner(c, B, g, U, d + 1, u);
                if (c->st == CASIM_OK) {
                    F = ib_at(c, H, U, u);
                    q = ib_at(c, g, U, u);
                    c->var = var;
                    if (c->st == CASIM_OK && ib_same(c, mul2(c, q, du), t)) {
                        if (c->st == CASIM_OK) return c->var = v0, F;
                    }
                }
            }
        }
        c->var = v0;
        if (!ib_failr(c, B, mark)) return SPARE;
    }
    c->var = v0;
    if (c->st == CASIM_OK) c->st = CASIM_NOT_INTEGRABLE;
    return SPARE;
}

/* rule 4's LIATE: 5 a log or a power of one, 4 an inverse function, 3 algebraic, 2 trigonometric, 1 exponential, 0 no
   var */
static int ib_class(ctx *c, ref f, int var)
{
    const node *x = nd(c, f);
    if (!contains(c, f, var)) return 0;
    if (x->kind == K_POW && nd(c, x->a)->kind == K_FN && (nd(c, x->a)->id == F_LN || nd(c, x->a)->id == F_LOG)) return 5;
    if (x->kind != K_FN) return 3;
    switch (x->id) {
    case F_LN: case F_LOG: return 5;
    case F_ATAN: case F_ASIN: case F_ACOS: case F_ASINH: case F_ACOSH: case F_ATANH: return 4;
    case F_SIN: case F_COS: return 2;
    case F_EXP: case F_ALOG: case F_SINH: case F_COSH: return 1;
    default: return 0;
    }
}

/* rule 4: ∫ t by parts */
static ref ib_parts(ctx *c, ib_t *B, ref t, int var, int d)
{
    const node *x = nd(c, t);
    ref u = SPARE, v, du, uv, w, W, F;
    int best = 0, other = 0;
    size_t mark = c->used;
    if (x->kind == K_MUL) {
        for (int i = 0; i < x->n; i++) {
            int k = ib_class(c, list(c, nd(c, t)->a)[i], var);
            if (k > best) best = k, u = list(c, nd(c, t)->a)[i];
        }
        for (int i = 0; i < nd(c, t)->n; i++) {
            int k = ib_class(c, list(c, nd(c, t)->a)[i], var);
            if (list(c, nd(c, t)->a)[i] != u && k) other = other ? -1 : k;
        }
        if (other == 0 && best < 4) goto no;                    /* one factor holds var: nothing to part */
    } else if ((best = ib_class(c, t, var)) >= 4) u = t;
    if (u == SPARE || best == 0) goto no;
    v = ib(c, B, ib_canon(c, mul2(c, t, power(c, u, small(c, C_M1)))), var, d + 1);
    if (c->st != CASIM_OK) goto no;
    c->var = var;
    if (best == 3 && (other == 1 || other == 2)) {
        /* a polynomial against EXP, SIN, COS, SINH or COSH: round until u^(j) is 0, the whole answer checked */
        ref acc = small(c, C_0), uj = u, V = v;
        for (int j = 0; j <= SOLVE_DEG && c->st == CASIM_OK; j++) {
            acc = add2(c, acc, j % 2 ? ib_neg(c, mul2(c, uj, V)) : mul2(c, uj, V));
            uj = ib_deriv(c, uj);
            if (is_k(c, uj, 0, 1)) {
                F = ib_canon(c, acc);
                if (c->st == CASIM_OK && closes(c, F, t)) return F;
                goto no;
            }
            V = ib(c, B, V, var, d + 1);
            c->var = var;
        }
        goto no;
    }
    du = ib_deriv(c, u);
    uv = ib_canon(c, mul2(c, u, v));
    w = ib_canon(c, mul2(c, v, du));
    if (best <= 2 && other >= 1 && other <= 2) {
        /* EXP × SIN: a second part, u2 of u's class; back to k × t, ∫ t = (uv - u2v2) ÷ (1 - k), checked before the
           division as (uv - u2v2)' = (1 - k) t (expand doesn't clear a constant denominator: DEG's 1 + π^2÷32400) */
        ref u2 = SPARE, v2, k, a, omk;
        const node *y = nd(c, w);
        if (y->kind == K_MUL)
            for (int i = 0; i < y->n; i++)
                if (ib_class(c, list(c, nd(c, w)->a)[i], var) == best) u2 = list(c, nd(c, w)->a)[i];
        if (u2 == SPARE) goto no;
        v2 = ib(c, B, ib_canon(c, mul2(c, w, power(c, u2, small(c, C_M1)))), var, d + 1);
        if (c->st != CASIM_OK) goto no;
        c->var = var;
        k = ib_canon(c, mul2(c, ib_canon(c, mul2(c, v2, ib_deriv(c, u2))), power(c, t, small(c, C_M1))));
        if (c->st != CASIM_OK || contains(c, k, var) || is_k(c, k, 1, 1)) goto no;
        a = ib_canon(c, add2(c, uv, ib_neg(c, ib_canon(c, mul2(c, u2, v2)))));
        omk = ib_canon(c, add2(c, small(c, C_1), ib_neg(c, k)));
        if (c->st != CASIM_OK || !closes(c, a, ib_canon(c, mul2(c, omk, t)))) goto no;
        F = ib_canon(c, mul2(c, a, power(c, omk, small(c, C_M1))));
        if (c->st == CASIM_OK) return F;
        goto no;
    }
    W = ib(c, B, w, var, d + 1);
    if (c->st != CASIM_OK) goto no;
    c->var = var;
    if (!closes(c, uv, add2(c, t, w))) goto no;                 /* (uv)' = t + v u', and ∫ v u' was checked */
    F = ib_canon(c, add2(c, uv, ib_neg(c, W)));
    if (c->st == CASIM_OK) return F;
no:
    if (ib_failr(c, B, mark)) c->st = CASIM_NOT_INTEGRABLE;
    return SPARE;
}

/* ∫ t dvar: a constant, a sum term by term, the table, the rational path, then substitution and parts a level down.
   d is -1 for the integrand itself, whose table and rational path the wrapper ran already (a canonical form may suit
   the table where the typed text didn't, so it runs again; the rational path doesn't) */
static ref ib(ctx *c, ib_t *B, ref t, int var, int d)
{
    size_t mark = c->used;
    ref F;
    if (c->st != CASIM_OK) return SPARE;
    if (!contains(c, t, var)) return ib_canon(c, mul2(c, t, leaf(c, K_VAR, var)));
    if (++B->tries > IB_TRIES || ib_bad(c, t, var)) return c->st == CASIM_OK ? c->st = CASIM_NOT_INTEGRABLE, SPARE : SPARE;
    if (nd(c, t)->kind == K_ADD) {
        ref acc = small(c, C_0);
        for (int i = 0; i < nd(c, t)->n && c->st == CASIM_OK; i++)
            acc = add2(c, acc, ib(c, B, list(c, nd(c, t)->a)[i], var, d < 0 ? 0 : d));
        return c->st == CASIM_OK ? ib_canon(c, acc) : SPARE;
    }
    F = ib_table(c, t, var);
    if (c->st == CASIM_OK || !ib_failr(c, B, mark)) return F;
    if (d >= 0 || ib_const(c, t, var)) {             /* the wrapper's rational path refused a constant factor */
        F = ib_rational(c, B, t, var);
        if (c->st == CASIM_OK || !ib_failr(c, B, mark)) return F;
    }
    if (d >= IB_DEPTH) return c->st = CASIM_NOT_INTEGRABLE, SPARE;
    /* a level of substitution or parts counts twice against the depth guard: its frames are about two walks' */
    if (!enter(c)) return SPARE;
    if (!enter(c)) return c->depth--, SPARE;
    if (d < 0) d = 0;
    F = ib_sub(c, B, t, var, d);
    if (c->st != CASIM_OK && ib_unnote(c, B, mark, d)) F = ib_parts(c, B, t, var, d);
    c->depth -= 2;
    if (c->st == CASIM_OK || !ib_failr(c, B, mark)) return F;
    return c->st = CASIM_NOT_INTEGRABLE, SPARE;
}

/* rule 2: whether b (canonical) is a denominator's base anywhere in t */
static bool ib_den(ctx *c, ref t, ref b)
{
    const node *x = nd(c, t);
    bool h = false;
    if (!enter(c)) return false;
    if (x->kind == K_POW && cmp(c, x->a, b) == 0 && nd(c, x->b)->kind == K_NUM && val(c, x->b)->p < 0) h = true;
    else if (x->kind == K_POW || x->kind == K_FN) h = ib_den(c, x->a, b) || (x->kind == K_POW && ib_den(c, nd(c, t)->b, b));
    else if (x->kind == K_ADD || x->kind == K_MUL)
        for (int i = 0; i < x->n && !h; i++) h = ib_den(c, list(c, nd(c, t)->a)[i], b);
    c->depth--;
    return h;
}

/* rule 2: d, a divisor as typed: each factor of its canonical form holding var (a zero of d, or of a divisor inside
   it) must still be a denominator in t, or canon cancelled a hole (X^2÷X). EXP and ALOG are never 0. The bases come
   from 1÷d's canonical form, whose sums carry no constant factor, as t's don't (3×X^2+3 is 3×(X^2+1) there) */
static bool ib_keeps(ctx *c, ref d, ref t, int var)
{
    ref dc = ib_inv(c, ib_canon(c, d));
    const node *x = nd(c, dc);
    int n = x->kind == K_MUL ? x->n : 1;
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref f = x->kind == K_MUL ? list(c, nd(c, dc)->a)[i] : dc, b = f;
        if (!contains(c, f, var)) continue;
        if (nd(c, f)->kind == K_POW) b = nd(c, f)->a;
        if (nd(c, b)->kind == K_FN && (nd(c, b)->id == F_EXP || nd(c, b)->id == F_ALOG)) continue;
        if (!ib_den(c, t, b)) return false;
    }
    return c->st == CASIM_OK;
}

/* whether t holds s (canonical) as a subtree, or (pow) s as a base to a power that isn't a whole number */
static bool ib_holds(ctx *c, ref t, ref s, bool pow)
{
    const node *x = nd(c, t);
    bool h = false;
    if (!enter(c)) return false;
    if (!pow && cmp(c, t, s) == 0) h = true;
    else if (pow && x->kind == K_POW && cmp(c, x->a, s) == 0) {
        const node *e = nd(c, x->b);
        h = e->kind != K_NUM || val(c, x->b)->q != 1;
    }
    if (!h && (x->kind == K_POW || x->kind == K_FN)) h = ib_holds(c, x->a, s, pow) || (x->kind == K_POW && ib_holds(c, nd(c, t)->b, s, pow));
    else if (!h && (x->kind == K_ADD || x->kind == K_MUL))
        for (int i = 0; i < x->n && !h; i++) h = ib_holds(c, list(c, nd(c, t)->a)[i], s, pow);
    c->depth--;
    return h;
}

/* rule 2: a typed function that limits var's domain (LN, LOG, ASIN, ACOS, ATANH, ACOSH; TAN's poles), or SQRT or a power
   that isn't a whole number, must still be in t: canon drops LN(X)-LN(X), 0×LN(X), EXP(LN(X)), TAN(X)^0 and
   X^(-A)×X^A, and the answer would have a value where the integrand has none */
static bool ib_domain_kept(ctx *c, ref r, ref t, int var)
{
    const node *x = nd(c, r);
    /* only where the argument or base still holds var once canon has it: (X-X)^4.5 is 0^4.5, defined everywhere */
    if (x->kind == K_RFN && raw_has_var(c, x->a, var) && contains(c, ib_canon(c, x->a), var)) {
        x = nd(c, r);
        switch (x->id) {
        case F_LN: case F_LOG: case F_ASIN: case F_ACOS: case F_ATANH: case F_ACOSH: case F_TAN:
            return ib_holds(c, t, ib_canon(c, r), false);
        case F_SQRT: return ib_holds(c, t, ib_canon(c, x->a), true) || ib_holds(c, t, ib_canon(c, r), false);
        default: return true;
        }
    }
    if (x->kind == K_BPOW && !raw_has_var(c, x->a, var) && raw_has_var(c, x->b, var)) {
        ref b = ib_canon(c, nd(c, r)->a);               /* 0^X, (-2)^X as typed: canon may fold 0^X×0^(-X) to 1 */
        return c->st == CASIM_OK && !(is_num(c, b) && val(c, b)->p <= 0);
    }
    x = nd(c, r);
    if (x->kind == K_BPOW && raw_has_var(c, x->a, var) && !raw_has_var(c, x->b, var) &&
        contains(c, ib_canon(c, nd(c, r)->a), var)) {
        ref e = ib_canon(c, nd(c, r)->b);
        if (nd(c, e)->kind != K_NUM || val(c, e)->q != 1) return ib_holds(c, t, ib_canon(c, nd(c, r)->a), true);
    }
    return c->st == CASIM_OK;
}

/* rule 2: every divisor in the typed tree r (÷'s right side, INV's argument, a base to a constant negative power),
   the ones nested in them too, kept in t */
static bool ib_typed_ok(ctx *c, ref r, ref t, int var)
{
    const node *x = nd(c, r);
    bool ok = true;
    if (r == 0 || r == SPARE || !raw_has_var(c, r, var) || !enter(c)) return true;
    if (x->kind == K_BDIV) ok = ib_keeps(c, x->b, t, var);
    else if (x->kind == K_RFN && x->id == F_INV) ok = ib_keeps(c, x->a, t, var);
    else if (x->kind == K_BPOW && !raw_has_var(c, x->b, var)) {
        ref e = ib_canon(c, nd(c, r)->b);
        if (nd(c, e)->kind == K_NUM && val(c, e)->p < 0) ok = ib_keeps(c, nd(c, r)->a, t, var);
    }
    if (ok) ok = ib_domain_kept(c, r, t, var);
    x = nd(c, r);
    if (ok && (x->kind == K_RFN || x->kind == K_NEG || x->kind == K_GROUP)) ok = ib_typed_ok(c, x->a, t, var);
    else if (ok && x->kind >= K_BADD && x->kind <= K_BPOW)
        ok = ib_typed_ok(c, x->a, t, var) && ib_typed_ok(c, nd(c, r)->b, t, var);
    c->depth--;
    return ok;
}

/* S2.6's LN form on the answer, once (not inside the search, whose arena it would take: CAS 017's cold review); the
   answer as it was when the form doesn't fit */
static __attribute__((noinline)) ref ib_lnfinal(ctx *c, ref F, int var)
{
    size_t mark = c->used;
    ref G = ib_canon(c, ib_lnform(c, F, var));
    if (c->st == CASIM_OK) return G;
    c->st = CASIM_OK, c->starved = false;
    taylor_drop(c, mark);
    return F;
}

/* rule 6: the sum's terms free of var dropped (an antiderivative is one up to a constant) */
static ref ib_drop(ctx *c, ref F, int var)
{
    ref acc = small(c, C_0);
    if (nd(c, F)->kind != K_ADD) return F;
    for (int i = 0; i < nd(c, F)->n; i++)
        if (contains(c, list(c, nd(c, F)->a)[i], var)) acc = add2(c, acc, list(c, nd(c, F)->a)[i]);
    return ib_canon(c, acc);
}

static __attribute__((noinline)) casim_status do_iparts(const char *eqn, int var, int angle, char *out, size_t cap,
                                                         void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ib_t B = {0, false, IBH_NONE, {0}};
    ref n = SPARE, t, F = SPARE;
    if (cap) out[0] = '\0';
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.opaque = true;
    c.var = var;
    c.sub_var = -1;
    if (c.st == CASIM_OK) n = parse(&c);
    if (c.st == CASIM_OK && nd(&c, n)->kind == K_REQ) return CASIM_BAD_ARG;
    t = c.st == CASIM_OK ? ib_canon(&c, n) : SPARE;
    if (c.st == CASIM_OK && !ib_typed_ok(&c, n, t, var) && c.st == CASIM_OK) c.st = CASIM_NOT_INTEGRABLE;
    if (c.st == CASIM_OK) {
        c.own_text = true;                      /* from here the parser reads only what casim printed */
        F = ib(&c, &B, t, var, -1);
        /* rule 6 as amended (abacus #8359): when nothing answers, NO_ROOM where a try ran out, else NOT_INTEGRABLE */
        if (c.st == CASIM_NO_ROOM || c.st == CASIM_NOT_INTEGRABLE) c.st = B.ranout ? CASIM_NO_ROOM : CASIM_NOT_INTEGRABLE;
        if (c.st == CASIM_OK) F = ib_lnfinal(&c, ib_drop(&c, F, var), var);
        if (c.st == CASIM_OK && !ib_letters_kept(&c, F, var, t)) c.st = CASIM_NOT_INTEGRABLE;
        c.s = eqn;
    }
    return finish(&c, F, out, cap < SLOT ? cap : SLOT);
}

/* ---- the working pages (rule 8's second build, work/017 §Steps) ----
   The search's own choice for the integrand, its pieces walked by integrate's steps on their printed text (CAS 013's
   composition): a sum by its terms, SUBSTITUTION, BY PARTS, the CYCLE, or a constant factor out. A walk nests at
   most IB_WALKS deep (each piece's walk is a fresh call, with its own depth guard), and below that a piece is rule 8's
   one page; a walk that doesn't fit is that one page too, so the steps never refuse what integrate answers. */

#define IB_SLAB 1536                    /* the texts a walk prints, before they are moved to the arena's top */
#define IB_NS 12
#define IB_LEVEL 16                     /* flags: a walk's level, in casim_step's bits above CASIM_STEPS_ONE_LINE */
#define IB_WALKS 3
#define IB_LIM 256                      /* flags: the page cap a piece's lines must fit, in the bits above the level */
#define IB_LIMMAX 65536

typedef struct {
    int how, U, n;                      /* n: the texts in s */
    bool sum;                           /* s holds a sum's terms */
    size_t used;                        /* their bytes */
    const char *s[IB_NS];               /* a sum's terms; else f, F, then the choice's pieces (ib_plan_make) */
} ib_plan;

/* the search's choice for t at depth 0 (ib's order: the table, the rational path with a constant factor, substitution,
   parts), into B; IBH_NONE when it is the table on t's canonical form. Substitution from its note (ib_inner), taken
   when its G integrates again and G(u)×u' is t (a try that failed leaves its note; tests/ibcheck_check.c spoils G);
   parts' pieces made as ib_parts makes them. Only functions ib's section already has out of line are called here:
   another call to ib_table, ib_rational or ib_same moved ib's frame (240 -> 224, measured) */
#ifndef CASIM_IB_REPLAY_PLANT
#define CASIM_IB_REPLAY_PLANT(c, g) (g)
#endif

static __attribute__((noinline)) void ib_choose(ctx *c, ib_t *B, ref t, int var)
{
    ib_t B2 = {0, false, IBH_NONE, {0}};
    size_t mark = c->used, top = c->top;
    const char *s0 = c->s;
    ref u = B->r[0], g = B->r[1], k, v;
    int how = B->how, best = 0, other = 0, U = -1;
    char *buf;
    const node *x;
    B->how = IBH_NONE;
    if ((buf = ib_print(c, t, IB_TEXT)) != NULL) {       /* the table, as ib_table */
        c->s = buf;
        integ(c, parse(c), var);
    }
    c->s = s0, c->top = top;
    if (c->st == CASIM_OK || !ib_fail(c, mark)) return;
    if ((k = ib_const(c, t, var)) != 0 &&                /* the rational path, as ib_rational */
        (buf = ib_print(c, ib_canon(c, mul2(c, t, ib_inv(c, k))), 2 * IB_TEXT)) != NULL &&
        do_irational(buf, var, c->angle, buf + IB_TEXT, IB_TEXT, c->mem + c->used, c->top - c->used, c->rtl) == CASIM_OK) {
        c->top = top;
        B->how = IBH_CONST, B->r[0] = k;
        return;
    }
    c->top = top;
    ib_fail(c, mark);
    for (int i = 0; ib_pick[i] && U < 0; i++)
        if (ib_pick[i] - 'A' != var && !contains(c, t, ib_pick[i] - 'A')) U = ib_pick[i] - 'A';
    if (how == IBH_SUB && U >= 0) {
        ref q, z;
        g = CASIM_IB_REPLAY_PLANT(c, g);
        ib(c, &B2, g, U, 1);
        c->var = var;
        c->sub_var = U, c->sub_val = u;                 /* G(u)×u' = t, as ib_sub checks it (ib_at, ib_same) */
        q = ib_canon(c, check_rewrite(c, g));
        c->sub_val = 0, c->sub_var = -1;
        z = expand_fully(c, check_rewrite(c, add2(c, mul2(c, q, ib_deriv(c, u)), ib_neg(c, t))));
        if (c->st == CASIM_OK && is_k(c, z, 0, 1)) {
            B->how = IBH_SUB, B->r[0] = u, B->r[1] = g, B->r[2] = (ref)U;
            return;
        }
        ib_fail(c, mark);                       /* the search's substitution, not replayed here (the arena): one page, */
        return;                                 /* not parts the search never made (the cold review's round 2) */
    }
    u = SPARE;                                          /* parts: u by LIATE, as ib_parts chooses it */
    x = nd(c, t);
    if (x->kind == K_MUL) {
        for (int i = 0; i < x->n; i++) {
            int cl = ib_class(c, list(c, nd(c, t)->a)[i], var);
            if (cl > best) best = cl, u = list(c, nd(c, t)->a)[i];
        }
        for (int i = 0; i < nd(c, t)->n; i++) {
            int cl = ib_class(c, list(c, nd(c, t)->a)[i], var);
            if (list(c, nd(c, t)->a)[i] != u && cl) other = other ? -1 : cl;
        }
        if (other == 0 && best < 4) return;            /* as ib_parts: one factor holds var */
    } else if ((best = ib_class(c, t, var)) >= 4) u = t;
    if (u == SPARE || best == 0) return;
    v = ib(c, &B2, ib_canon(c, mul2(c, t, power(c, u, small(c, C_M1)))), var, 1);
    c->var = var;
    if (c->st != CASIM_OK) {
        ib_fail(c, mark);
        return;
    }
    B->how = IBH_PARTS, B->r[0] = u, B->r[1] = v, B->r[2] = ib_canon(c, mul2(c, v, ib_deriv(c, u)));
    if (best <= 2 && other >= 1 && other <= 2) {       /* the cycle: u2, v2 = ∫ w÷u2, k = v2×u2' ÷ t */
        const node *y = nd(c, B->r[2]);
        B->how = IBH_CYCLE, B->r[3] = SPARE;
        for (int i = 0; y->kind == K_MUL && i < y->n; i++)
            if (ib_class(c, list(c, nd(c, B->r[2])->a)[i], var) == best) B->r[3] = list(c, nd(c, B->r[2])->a)[i];
        if (B->r[3] != SPARE) {
            B->r[4] = ib(c, &B2, ib_canon(c, mul2(c, B->r[2], power(c, B->r[3], small(c, C_M1)))), var, 1);
            c->var = var;
            B->r[5] = ib_canon(c, mul2(c, ib_canon(c, mul2(c, B->r[4], ib_deriv(c, B->r[3]))),
                                       power(c, t, small(c, C_M1))));
        }
        if (B->r[3] == SPARE || c->st != CASIM_OK) {
            ib_fail(c, mark);
            B->how = IBH_NONE;
        }
    }
}

/* the walk's texts from integrate's own search on rq, NUL-separated in slab: a sum's terms (sum), or
   f and F and the choice's pieces: substitution u, g; parts u, v, w, f÷u; the cycle u2, v2, -k, w÷u2, 1-k, then
   parts'; a constant factor k, f÷k */
static __attribute__((noinline)) casim_status ib_plan_make(const casim_req *rq, ib_plan *P, char *slab, void *arena,
                                                            size_t arena_cap)
{
    ctx c;
    ib_t B = {0, false, IBH_NONE, {0}};
    ref n = SPARE, t, F = SPARE, x[IB_NS];
    int var = rq->var, nx = 0;
    out_t w = {slab, IB_SLAB, 0, false};
    memset(P, 0, sizeof *P);
    start(&c, rq->eqn, rq->angle, arena, arena_cap);
    c.rtl = rq->model == CASIM_MODEL_STU;
    c.opaque = true;
    c.var = var;
    c.sub_var = -1;
    if (c.st == CASIM_OK) n = parse(&c);
    t = c.st == CASIM_OK ? ib_canon(&c, n) : SPARE;
    if (c.st == CASIM_OK && !ib_typed_ok(&c, n, t, var) && c.st == CASIM_OK) c.st = CASIM_NOT_INTEGRABLE;
    if (c.st == CASIM_OK) {
        c.own_text = true;
        F = ib(&c, &B, t, var, -1);
        if (c.st == CASIM_OK) F = ib_lnfinal(&c, ib_drop(&c, F, var), var);
    }
    if (c.st != CASIM_OK) return c.st;
    c.var = var;
    if (nd(&c, t)->kind == K_ADD) {
        if (nd(&c, t)->n > IB_NS - 1) return CASIM_NO_ROOM;
        P->sum = true;
        for (int i = 0; i < nd(&c, t)->n; i++) x[nx++] = list(&c, nd(&c, t)->a)[i];
        x[nx++] = F;                                    /* F last: the answer line's measure (abacus #8227) */
    } else {
        ref k;
        ib_choose(&c, &B, t, var);
        if (c.st != CASIM_OK) return c.st;
        k = B.r[5];
        P->how = B.how, P->U = B.how == IBH_SUB ? (int)B.r[2] : 0;
        if (B.how == IBH_SUB) {             /* U as the answer's LN has it (S2.6): w = s×u, G(U÷s)÷s */
            bool none;
            rat sc = {1, 1};
            ref wf = ib_whole(&c, B.r[0], var, &none, &sc), is;
            if (wf != SPARE && !(sc.p == 1 && sc.q == 1)) {
                rat inv = {sc.p < 0 ? -sc.q : sc.q, sc.p < 0 ? -sc.p : sc.p};        /* 1÷s, by hand: not rat_make */
                is = num(&c, &inv);
                c.sub_var = P->U, c.sub_val = ib_canon(&c, mul2(&c, leaf(&c, K_VAR, P->U), is));
                B.r[1] = ib_canon(&c, mul2(&c, check_rewrite(&c, B.r[1]), is));
                c.sub_val = 0, c.sub_var = -1;
                B.r[0] = wf;
                if (c.st != CASIM_OK) return c.st;
            }
        }
        x[nx++] = t, x[nx++] = F;
        if (B.how == IBH_SUB) x[nx++] = B.r[0], x[nx++] = B.r[1];
        if (B.how == IBH_CYCLE) {
            x[nx++] = B.r[3], x[nx++] = B.r[4], x[nx++] = ib_neg(&c, k);
            x[nx++] = ib_canon(&c, mul2(&c, B.r[2], power(&c, B.r[3], small(&c, C_M1))));
            x[nx++] = ib_canon(&c, add2(&c, small(&c, C_1), ib_neg(&c, k)));
        }
        if (B.how == IBH_PARTS || B.how == IBH_CYCLE) {
            x[nx++] = B.r[0], x[nx++] = B.r[1], x[nx++] = B.r[2];
            x[nx++] = ib_canon(&c, mul2(&c, t, power(&c, B.r[0], small(&c, C_M1))));
        }
        if (B.how == IBH_CONST)
            x[nx++] = B.r[0], x[nx++] = ib_canon(&c, mul2(&c, t, power(&c, B.r[0], small(&c, C_M1))));
    }
    for (int i = 0; i < nx && c.st == CASIM_OK; i++) {
        P->s[i] = slab + w.n;
        print(&c, &w, x[i]);
        if (w.full || w.n + 1 >= w.cap) return CASIM_NO_ROOM;
        slab[w.n++] = '\0';
    }
    P->n = nx, P->used = w.n;
    return c.st;
}

/* whether a printed text is a sum at its top (a + or - outside brackets, not a sign) */
static bool ib_sum_text(const char *s)
{
    int depth = 0;
    for (int i = 0; s[i]; i++) {
        unsigned char b = (unsigned char)s[i], a = i ? (unsigned char)s[i - 1] : 0;
        if (s[i] == '(') depth++;
        else if (s[i] == ')') depth--;
        else if (depth == 0 && i > 0 && (s[i] == '+' || s[i] == '-') && a != '(' && a != '^' && a != 0x97 && a != 0xb7 &&
                 !(a == 'E' && i > 1 && s[i - 2] >= '0' && s[i - 2] <= '9'))
            return true;
        (void)b;
    }
    return false;
}

/* a factor of a product line: bracketed when it is a sum, or a signed factor after the first */
static void ib_wfac(out_t *w, const char *s, bool first)
{
    bool b = ib_sum_text(s) || (!first && s[0] == '-');
    if (b) pf_puts(w, "(");
    pf_puts(w, s);
    if (b) pf_puts(w, ")");
}

/* a divisor: bracketed unless it is a number or a name alone (÷(10÷9), ÷(π^2÷32400+1)) */
static void ib_wdiv(out_t *w, const char *s)
{
    bool b = s[0] == '-' || strpbrk(s, "+-^(") != NULL || strstr(s, TIMES) != NULL || strstr(s, "\xc3\xb7") != NULL;
    if (b) pf_puts(w, "(");
    pf_puts(w, s);
    if (b) pf_puts(w, ")");
}

/* "a×b", each a factor */
static void ib_wprod(out_t *w, const char *a, const char *b)
{
    ib_wfac(w, a, true), pf_puts(w, TIMES), ib_wfac(w, b, false);
}

/* ", L=text" */
static void ib_wlet(out_t *w, int letter, const char *text, int var)
{
    char d[5] = {',', ' ', (char)('A' + letter), '=', '\0'};
    pf_puts(w, d);
    if (var >= 0) irs_int(w, text, var);
    else pf_puts(w, text);
}

/* a label's letter for BY PARTS's line: the first of U, V, W, T, S, R, Q that isn't var, isn't skip, and isn't a
   letter of text (one standing alone: not in a function's name) */
static int ib_label(const char *text, int var, int skip)
{
    static const char pick[] = "UVWTSRQ";
    for (int i = 0; pick[i]; i++) {
        int l = pick[i] - 'A';
        bool used = l == var || l == skip;
        for (int j = 0; text[j] && !used; j++)
            used = text[j] == pick[i] && !(j && text[j - 1] >= 'A' && text[j - 1] <= 'Z') &&
                   !(text[j + 1] >= 'A' && text[j + 1] <= 'Z') && text[j + 1] != '(';
        if (!used) return l;
    }
    for (int l = 25; l >= 0; l--) {                    /* the list taken: any letter free in text */
        bool used = l == var || l == skip;
        for (int j = 0; text[j] && !used; j++)
            used = text[j] == 'A' + l && !(j && text[j - 1] >= 'A' && text[j - 1] <= 'Z') &&
                   !(text[j + 1] >= 'A' && text[j + 1] <= 'Z') && text[j + 1] != '(';
        if (!used) return l;
    }
    return 'U' - 'A';
}

/* irs_sub for a piece: a page too long for out is noted (*tl) and the walk counted on, so that TOO_LONG comes back with
   the whole count, as casim_step's contract has it (the cold review: BAD_ARG for a k below the count) */
static __attribute__((noinline)) casim_status ib_piece(irs_t *S, const casim_req *rq, int down, void *arena, size_t cap,
                                                        bool *tl)
{
    casim_step_info si;
    casim_status st = casim_step(rq, -1, S->flags, &si, NULL, 0, arena, cap);
    if (st != CASIM_OK) return st;
    if (S->k >= S->at && S->k < S->at + si.count) {
        st = casim_step(rq, S->k - S->at, S->flags, S->info, S->w->o, S->w->cap, arena, cap);
        S->info->depth += down;
        if (st == CASIM_TOO_LONG) *tl = true;
        else if (st != CASIM_OK) return st;
    }
    S->at += si.count;
    return CASIM_OK;
}

/* rule 8's one page, also when counted with no out (a piece's walk): integrate's answer into the arena's top */
static casim_status ib_one(const casim_req *rq, int k, casim_step_info *info, char *out, size_t cap, void *arena,
                           size_t arena_cap)
{
    if (cap < IB_TEXT && k < 0 && arena_cap > 2 * IB_TEXT) {
        casim_status st = ib_steps(rq, k, info, (char *)arena + arena_cap - IB_TEXT, IB_TEXT, arena, arena_cap - IB_TEXT);
        /* the bytes the text took may be the ones integrate lacked: LN(X+1) at 8,192 bytes and cap 192 (abacus #8227) */
        if ((st != CASIM_NOT_INTEGRABLE && st != CASIM_NO_ROOM) || !cap) return st;
    }
    return ib_steps(rq, k, info, out, cap, arena, arena_cap);
}

/* a walk under way: ib_body's one argument */
typedef struct {
    irs_t S;
    const casim_req *rq;
    const ib_plan *P;
    void *arena;
    size_t room;                        /* the pieces' arena */
    int x, lu, lv, rule;
    bool tl;                            /* a piece's page was past cap */
    bool measure;                       /* the walk's own lines only, the pieces left out */
} ib_wk;

/* a piece's walk, left out when the walk only measures its own lines */
static casim_status ib_part(ib_wk *K, const casim_req *sub, int down)
{
    return K->measure ? CASIM_OK : ib_piece(&K->S, sub, down, K->arena, K->room, &K->tl);
}

/* the walk's lines and its pieces, page S.k into S.w; measuring, page S.k counts this walk's own lines only, and the
   answer line is written out (F is the plan's) */
static __attribute__((noinline)) casim_status ib_body(ib_wk *K)
{
    irs_t *S = &K->S;
    out_t *w = S->w;
    const ib_plan *P = K->P;
    const casim_req *rq = K->rq;
    casim_req sub = *rq;
    casim_status st = CASIM_OK;
    int var = rq->var, x = K->x, lu = K->lu, lv = K->lv, nt = P->n - 1;
    K->rule = CASIM_RULE_SUM;
    if (P->sum) {
        if (irs_hit(S, 0, CASIM_STEP_TEMPLATE, K->rule)) {
            irs_int(w, rq->eqn, var), pf_puts(w, " = ");
            for (int i = 0; i < nt; i++) {
                if (i) pf_puts(w, "+");
                irs_int(w, P->s[i], var);
            }
        }
        for (int i = 0; i < nt && st == CASIM_OK; i++) sub.eqn = P->s[i], st = ib_part(K, &sub, 1);
    } else if (P->how == IBH_SUB) {
        K->rule = CASIM_RULE_SUBSTITUTION;
        if (irs_hit(S, 0, CASIM_STEP_TEMPLATE, K->rule))
            irs_int(w, rq->eqn, var), pf_puts(w, " = "), irs_int(w, P->s[3], P->U), ib_wlet(w, P->U, P->s[2], -1);
        sub.eqn = P->s[3], sub.var = P->U;
        st = ib_part(K, &sub, 1);
    } else if (P->how == IBH_CONST) {
        K->rule = CASIM_RULE_CONSTANT_MULTIPLE;
        if (irs_hit(S, 0, CASIM_STEP_TEMPLATE, K->rule))
            irs_int(w, rq->eqn, var), pf_puts(w, " = "), ib_wfac(w, P->s[2], true), pf_puts(w, TIMES),
                irs_int(w, P->s[3], var);
        sub.eqn = P->s[3];
        st = ib_part(K, &sub, 1);
    } else {
        K->rule = CASIM_RULE_BY_PARTS;
        if (irs_hit(S, 0, CASIM_STEP_TEMPLATE, K->rule)) {
            irs_int(w, rq->eqn, var), pf_puts(w, " = "), ib_wprod(w, P->s[x + 2], P->s[x + 3]), pf_puts(w, "-");
            irs_int(w, P->s[x + 4], var), ib_wlet(w, lu, P->s[x + 2], -1), ib_wlet(w, lv, P->s[x + 5], var);
        }
        sub.eqn = P->s[x + 5];
        st = ib_part(K, &sub, 1);
        if (P->how == IBH_PARTS && st == CASIM_OK) sub.eqn = P->s[x + 4], st = ib_part(K, &sub, 1);
        if (P->how == IBH_CYCLE) {
            /* ∫(w)dX = u2×v2 + (-k)×∫(f)dX, its v2's walk, then ∫(f)dX = (u×v - u2×v2)÷(1-k) */
            if (irs_hit(S, 1, CASIM_STEP_TEMPLATE, CASIM_RULE_BY_PARTS)) {
                irs_int(w, P->s[x + 4], var), pf_puts(w, " = "), ib_wprod(w, P->s[2], P->s[3]);
                if (strcmp(P->s[4], "1") == 0) pf_puts(w, "+");
                else if (strcmp(P->s[4], "-1") == 0) pf_puts(w, "-");
                else pf_puts(w, P->s[4][0] == '-' ? "" : "+"), ib_wfac(w, P->s[4], true), pf_puts(w, TIMES);
                irs_int(w, rq->eqn, var), ib_wlet(w, lu, P->s[2], -1), ib_wlet(w, lv, P->s[5], var);
            }
            sub.eqn = P->s[5];
            if (st == CASIM_OK) st = ib_part(K, &sub, 2);
            if (irs_hit(S, 1, CASIM_STEP_RESULT, CASIM_RULE_CYCLE)) {
                irs_int(w, rq->eqn, var), pf_puts(w, " = ("), ib_wprod(w, P->s[x + 2], P->s[x + 3]), pf_puts(w, "-");
                ib_wprod(w, P->s[2], P->s[3]), pf_puts(w, ")\xc3\xb7");
                ib_wdiv(w, P->s[6]);
            }
        }
    }
    if (st != CASIM_OK) return st;
    if (!P->sum && irs_hit(S, 1, CASIM_STEP_RESULT, CASIM_RULE_CHECK)) {
        char d[7] = {'d', '/', 'd', (char)('A' + var), '(', '\0'};
        pf_puts(w, d), pf_puts(w, P->s[1]), pf_puts(w, ") = "), pf_puts(w, rq->eqn);
    }
    if (irs_hit(S, 0, CASIM_STEP_RESULT, K->rule)) {
        if (K->measure) irs_int(w, rq->eqn, var), pf_puts(w, " = "), pf_puts(w, P->s[P->sum ? nt : 1]);
        else S->want = 2;
    }
    return CASIM_OK;
}

/* the walk (do_irsteps, when the rational path refuses too), with ib_steps's arguments, the flags riding in info's line
   (do_irsteps's frame and irs_refused's stay as they were). Its own lines must each fit the page cap, or it is rule 8's
   one page (abacus #8227: a walk is never refused where integrate answers); a piece's walk has the cap in its flags */
static __attribute__((noinline)) casim_status ib_walk(const casim_req *rq, int k, casim_step_info *info, char *out,
                                                       size_t cap0, void *arena, size_t cap)
{
    ib_plan P;
    ib_wk K;
    out_t w = {out, cap0, 0, false}, m;
    casim_status st;
    size_t room = cap > 2 * IB_SLAB ? (cap - IB_SLAB) & ~(size_t)7 : 0, keep, lim;
    char *slab = (char *)arena + room, *to;
    int flags = info->line;
    memset(info, 0, sizeof *info);
    if (cap0) out[0] = '\0';
    if (flags % IB_LIM / IB_LEVEL >= IB_WALKS || !room) return ib_one(rq, k, info, out, cap0, arena, cap);
    /* integrate's own answer first: the walk is shown only where it answers (the plan doesn't repeat each of its
       checks: the letter rule refuses SIN(A×X)×COS(X), whose parts the plan would make) */
    if ((st = ib_one(rq, -1, info, out, cap0, arena, cap)) != CASIM_OK) return st;
    memset(info, 0, sizeof *info);
    st = ib_plan_make(rq, &P, slab, arena, room);
    if (st != CASIM_OK || (P.how == IBH_NONE && !P.sum)) return ib_one(rq, k, info, out, cap0, arena, cap);
    keep = (P.used + 7) & ~(size_t)7;                   /* the texts to the arena's top, the rest for the pieces */
    to = (char *)arena + cap - keep;
    memmove(to, slab, P.used);
    for (int i = 0; i < P.n; i++) P.s[i] = to + (P.s[i] - slab);
    room = (cap - keep) & ~(size_t)7;
    lim = cap0 ? cap0 : (size_t)(flags / IB_LIM);
    if (lim > IB_LIMMAX) lim = IB_LIMMAX;
    K.rq = rq, K.P = &P, K.arena = arena, K.room = room, K.tl = false;
    K.S.flags = flags % IB_LIM + IB_LEVEL + (int)lim * IB_LIM, K.S.info = info, K.S.want = 0, K.S.skip = false;
    K.x = P.how == IBH_CYCLE ? 5 : 0;                   /* parts' u, v, w, f÷u at x + 2 */
    K.lu = ib_label(rq->eqn, rq->var, -1), K.lv = ib_label(rq->eqn, rq->var, K.lu);
    if (lim) {                                          /* each own line, measured in the arena (free till the pieces) */
        m = (out_t){(char *)arena, lim < room ? lim : room, 0, false};
        K.measure = true, K.S.w = &m;
        for (int j = 0;; j++) {
            K.S.k = j, K.S.at = 0;
            ib_body(&K);
            if (K.S.at <= j) break;
            if (m.full) return ib_one(rq, k, info, out, cap0, arena, cap);
        }
        memset(info, 0, sizeof *info);
    }
    K.measure = false, K.S.w = &w, K.S.k = k, K.S.at = 0;
    if (ib_body(&K) != CASIM_OK) return ib_one(rq, k, info, out, cap0, arena, cap);
    info->count = K.S.at;
    if (k < 0) return CASIM_OK;
    if (k >= K.S.at) return CASIM_BAD_ARG;
    if (K.tl) return out[0] = '\0', CASIM_TOO_LONG;
    if (K.S.want) {                                     /* the last page: integrate's answer, byte for byte */
        st = ib_steps(rq, 0, info, out, cap0, arena, cap);
        info->count = K.S.at, info->rule = K.rule;
        return st;
    }
    if (w.full) return out[0] = '\0', CASIM_TOO_LONG;
    return CASIM_OK;
}

/* rule 8: integrate's one page for an integrand only this path answers, ∫(f)dX = F under ANSWER (the working pages are
   the unit's second build) */
static __attribute__((noinline)) casim_status ib_steps(const casim_req *rq, int k, casim_step_info *info, char *out,
                                                        size_t cap, void *arena, size_t arena_cap)
{
    size_t n, m = strlen(rq->eqn), p = 4 + m + 3 + 3;          /* "∫(" text ")dX" " = " */
    casim_status st;
    memset(info, 0, sizeof *info);
    if (cap) out[0] = '\0';
    if (cap == 0 && k >= 0) return CASIM_TOO_LONG;
    st = do_iparts(rq->eqn, rq->var, rq->angle, out, cap, arena, arena_cap, rq->model == CASIM_MODEL_STU);
    if (st == CASIM_OK) info->count = 1, info->line = CASIM_STEP_RESULT, info->rule = CASIM_RULE_ANSWER;
    if (st != CASIM_OK || k < 0 || k >= 1) {
        if (cap) out[0] = '\0';
        return st != CASIM_OK ? st : k < 0 ? CASIM_OK : CASIM_BAD_ARG;
    }
    n = strlen(out);
    if (p + n + 1 > cap) return out[0] = '\0', CASIM_TOO_LONG;
    memmove(out + p, out, n + 1);
    memcpy(out, "\xe2\x88\xab(", 4);
    memcpy(out + 4, rq->eqn, m);
    out[4 + m] = ')', out[5 + m] = 'd', out[6 + m] = (char)('A' + rq->var);
    memcpy(out + 7 + m, " = ", 3);
    return CASIM_OK;
}

/* ---- limits (unit CAS 015) ----
   lim f as var -> a, from a side, or as var -> ±∞ (work/015-limits.md). An extended-real walk over the canonical tree:
   a constant; f's exact value where it has one at a (continuity); sums, products and powers by their parts; functions
   by continuity and by their ends; 0÷0 and ∞÷∞ by L'Hôpital on derive, at most 12 steps a call; 0×∞ as a quotient;
   b^e with var in e as EXP(e×LN(b)); ∞-∞ by the leading terms or a conjugate; the sign near a from the first derivative
   that isn't 0 there, or at ∞ from the leading term. At ∞ the values are native (X, EXP, LN, powers, a polynomial's
   leading term); -∞ is +∞ of f(-X). A two-sided limit at a is both one-sided ones. */

enum { L_FIN, L_PINF, L_NINF, L_OSC, L_NONE, L_UND };    /* L_OSC: SIN/COS/TAN of an argument going to ∞ */

typedef struct {
    int k;
    ref v;
} lv;

typedef struct {
    int var, side;              /* side: 1 from above, -1 from below (one-sided at a finite point) */
    ref a;                      /* the finite point, or 0 at +∞ */
    int steps;                  /* L'Hôpital steps taken this call */
} lwalk;

#define LIM_STEPS 12

static lv lm(int k, ref v) { lv r = {k, v}; return r; }

/* a power of an exact 0 left in a value (0^K: canon folds those whose exponent's sign it knows), which no limit is */
static bool lm_zpow(ctx *c, ref v)
{
    bool z = false;
    if (!enter(c)) return false;
    if (nd(c, v)->kind == K_POW) z = is_k(c, nd(c, v)->a, 0, 1) || lm_zpow(c, nd(c, v)->a) || lm_zpow(c, nd(c, v)->b);
    else if (nd(c, v)->kind == K_FN) z = lm_zpow(c, nd(c, v)->a);
    else if (nd(c, v)->kind == K_ADD || nd(c, v)->kind == K_MUL)
        for (int i = 0; i < nd(c, v)->n && !z; i++) z = lm_zpow(c, list(c, nd(c, v)->a)[i]);
    c->depth--;
    return z;
}

/* f's exact value at the finite point, or false where it has none (the arena's status kept): every part valued, as
   taylor's rule 7 does it, so a hole the whole folds away ((X-π÷2)×TAN(X) at π÷2 is 0×TAN(π÷2)) is not continuity */
static __attribute__((noinline)) bool lm_at(ctx *c, ref f, const lwalk *L, ref *v)
{
    abn x;
    taylor_check(c, f, L->var, L->a);                   /* a hole: UNDEFINED, which here is only "not continuous" */
    if (c->st == CASIM_UNDEFINED) return c->st = CASIM_OK, false;
    if (c->st != CASIM_OK) return false;
    c->sub_var = L->var, c->sub_val = L->a;
    *v = check_rewrite(c, f);
    c->sub_val = 0, c->sub_var = -1;
    if (c->st == CASIM_UNDEFINED) return c->st = CASIM_OK, false;
    if (c->st != CASIM_OK) return false;
    for (int l = 0; l < 26; l++)
        if (contains(c, *v, l)) return !lm_zpow(c, *v);  /* other letters: a value, kept as it is, but not 0^K */
    return const_val(c, *v, &x) == CV_OK;
}

/* a constant's sign: 1, -1, 0, or 2 when it has none here */
static __attribute__((noinline)) int lm_sgn(ctx *c, ref v)
{
    abn x;
    for (int l = 0; l < 26; l++)
        if (contains(c, v, l)) return 2;
    if (const_val(c, v, &x) != CV_OK) return 2;
    return abn_is_zero(&x) ? 0 : abn_is_negative(&x) ? -1 : 1;
}

/* the leading term of f at +∞ for an algebraic f: f ~ coef × var^deg (coef a constant, not 0); false when it isn't
   algebraic there, or the leading terms of a sum cancel */
static bool lm_lead(ctx *c, ref f, int var, ref *coef, rat *deg);

static __attribute__((noinline)) bool lm_lead_in(ctx *c, ref f, int var, ref *coef, rat *deg)
{
    const node *x = nd(c, f);
    if (!contains(c, f, var)) return *coef = f, rat_int(deg, 0), true;
    if (x->kind == K_VAR && x->id == var) return *coef = small(c, C_1), rat_int(deg, 1), true;
    if (x->kind == K_POW && is_num(c, x->b)) {
        rat e = *val(c, x->b), d;
        ref k;
        if (!lm_lead(c, nd(c, f)->a, var, &k, &d) || !rat_mul(deg, &d, &e)) return false;
        if (e.q != 1 && lm_sgn(c, k) != 1) return false;          /* a root of a negative leading term */
        *coef = canon(c, power(c, k, nd(c, f)->b));
        return c->st == CASIM_OK;
    }
    if (x->kind == K_FN && x->id == F_SQRT) {
        rat d, h = {1, 2};
        ref k;
        if (!lm_lead(c, nd(c, f)->a, var, &k, &d) || lm_sgn(c, k) != 1 || !rat_mul(deg, &d, &h)) return false;
        *coef = canon(c, fn(c, F_SQRT, k));
        return c->st == CASIM_OK;
    }
    if (x->kind == K_MUL) {
        ref k = small(c, C_1);
        rat d = {0, 1};
        for (int i = 0; i < nd(c, f)->n; i++) {
            ref ki;
            rat di;
            if (!lm_lead(c, list(c, nd(c, f)->a)[i], var, &ki, &di) || !rat_add(&d, &d, &di)) return false;
            k = mul2(c, k, ki);
        }
        *coef = canon(c, k), *deg = d;
        return c->st == CASIM_OK;
    }
    if (x->kind == K_ADD) {
        ref k = 0;
        rat d = {0, 1};
        bool any = false;
        for (int i = 0; i < nd(c, f)->n; i++) {
            ref ki;
            rat di;
            if (!lm_lead(c, list(c, nd(c, f)->a)[i], var, &ki, &di)) return false;
            if (!any || rat_cmp(&di, &d) > 0) k = ki, d = di, any = true;
            else if (rat_cmp(&di, &d) == 0) k = add2(c, k, ki);
        }
        if (!any) return false;
        k = canon(c, k);
        if (c->st != CASIM_OK || lm_sgn(c, k) == 0) return false;   /* the leading terms cancel */
        *coef = k, *deg = d;
        return true;
    }
    return false;
}

static __attribute__((noinline)) bool lm_lead(ctx *c, ref f, int var, ref *coef, rat *deg)
{
    bool r;
    if (!enter(c)) return false;
    r = lm_lead_in(c, f, var, coef, deg);
    c->depth--;
    return r;
}

/* the sign of d near the point on L's side, where d's limit there is 0 (2: not decided): at a finite point, the first
   derivative that isn't 0 there; at ∞, the leading term */
static __attribute__((noinline)) int lm_sign_near(ctx *c, ref d, const lwalk *L)
{
    ref v;
    if (!L->a) {
        rat deg;
        return lm_lead(c, d, L->var, &v, &deg) ? lm_sgn(c, v) : 2;
    }
    for (int m = 1; m <= LIM_STEPS && c->st == CASIM_OK; m++) {
        int s;
        d = canon(c, deriv(c, d));
        if (!lm_at(c, d, L, &v)) return 2;
        s = lm_sgn(c, v);
        if (s == 2) return 2;
        if (s == 0) continue;
        return L->side > 0 || m % 2 == 0 ? s : -s;
    }
    return 2;
}

static lv lm_walk(ctx *c, ref f, lwalk *L);

/* f's factors over and under the line (under: a power with a negative number for its exponent), as n and d */
static __attribute__((noinline)) bool lm_split(ctx *c, ref f, ref *n, ref *d)
{
    const node *x = nd(c, f);
    int cnt = x->kind == K_MUL ? x->n : 1;
    ref nn = small(c, C_1), dd = small(c, C_1);
    bool under = false;
    for (int i = 0; i < cnt; i++) {
        ref g = nd(c, f)->kind == K_MUL ? list(c, nd(c, f)->a)[i] : f;
        const node *y = nd(c, g);
        if (y->kind == K_POW && is_num(c, y->b) && val(c, y->b)->p < 0) {
            rat e = *val(c, y->b);
            e.p = -e.p;
            dd = mul2(c, dd, power(c, y->a, num(c, &e)));
            under = true;
        } else nn = mul2(c, nn, g);
    }
    *n = canon(c, nn), *d = canon(c, dd);
    return under;
}

/* n ÷ d by the limits of each; 0÷0 and ∞÷∞ by L'Hôpital (the quotient of the derivatives put in canonical form and
   split again, so it simplifies between steps), or at ∞ by the leading terms */
static __attribute__((noinline)) lv lm_quot(ctx *c, ref n, ref d, lwalk *L)
{
    for (;;) {
        lv ln = lm_walk(c, n, L), ld = lm_walk(c, d, L);
        ref q;
        if (c->st != CASIM_OK) return lm(L_UND, 0);
        if (ln.k >= L_OSC || ld.k >= L_OSC) return lm(L_UND, 0);
        if (ld.k == L_FIN) {
            int sd = lm_sgn(c, ld.v);
            if (sd != 0) {
                if (ln.k == L_FIN) return lm(L_FIN, canon(c, mul2(c, ln.v, power(c, ld.v, small(c, C_M1)))));
                if (sd == 2) return lm(L_UND, 0);
                return lm(ln.k == L_PINF ? (sd > 0 ? L_PINF : L_NINF) : (sd > 0 ? L_NINF : L_PINF), 0);
            }
            if (ln.k != L_FIN || lm_sgn(c, ln.v) != 0) {        /* nonzero ÷ 0 */
                int sn = ln.k == L_FIN ? lm_sgn(c, ln.v) : ln.k == L_PINF ? 1 : -1, s = lm_sign_near(c, d, L);
                if (sn == 2 || s == 2) return lm(L_UND, 0);
                return lm(sn * s > 0 ? L_PINF : L_NINF, 0);
            }
        } else if (ln.k == L_FIN) {
            return lm(L_FIN, small(c, C_0));                    /* finite ÷ ∞ */
        }
        if (!L->a) {
            /* 0÷0 or ∞÷∞ at ∞: the leading terms, when both are algebraic (negative degrees too) */
            ref kn, kd;
            rat dn, dg;
            if (lm_lead(c, n, L->var, &kn, &dn) && lm_lead(c, d, L->var, &kd, &dg)) {
                int cmp = rat_cmp(&dn, &dg), s = lm_sgn(c, kn) * lm_sgn(c, kd);
                if (cmp < 0) return lm(L_FIN, small(c, C_0));
                if (cmp == 0) return lm(L_FIN, canon(c, mul2(c, kn, power(c, kd, small(c, C_M1)))));
                if (s != 1 && s != -1) return lm(L_UND, 0);
                return lm(s > 0 ? L_PINF : L_NINF, 0);
            }
            if (c->st != CASIM_OK) return lm(L_UND, 0);
        }
        /* 0÷0 or ∞÷∞: L'Hôpital */
        if (++L->steps > LIM_STEPS || c->st != CASIM_OK) return lm(L_UND, 0);
        q = canon(c, mul2(c, canon(c, deriv(c, n)), power(c, canon(c, deriv(c, d)), small(c, C_M1))));
        if (c->st != CASIM_OK) return lm(L_UND, 0);
        if (!lm_split(c, q, &n, &d)) return lm_walk(c, q, L);
    }
}

/* a square root, alone or as a factor (-SQRT(u) is -1 × SQRT(u)) */
static bool lm_root(ctx *c, ref r)
{
    const node *x = nd(c, r);
    if ((x->kind == K_FN && x->id == F_SQRT) || (x->kind == K_POW && is_k(c, x->b, 1, 2))) return true;
    if (x->kind == K_MUL)
        for (int i = 0; i < x->n; i++) {
            const node *y = nd(c, list(c, x->a)[i]);
            if ((y->kind == K_FN && y->id == F_SQRT) || (y->kind == K_POW && is_k(c, y->b, 1, 2))) return true;
        }
    return false;
}

/* CAS 016 rule 3: p + m (going to +∞ and -∞) over one denominator, n_1÷d_1 + n_2÷d_2 + ... as (n_1×d_2×... + d_1×n_2×...)
   ÷ (d_1×d_2×...), walked as a quotient (1÷X-1÷SIN(X) is (SIN(X)-X)÷(X×SIN(X))); the sum's finite terms added back.
   Undecided when no term has a denominator (the quotient would be the sum again). It comes after the test of which
   term dominates, whose work (arena from mark, L'Hôpital steps past steps) it gives back first */
static __attribute__((noinline)) lv lm_combine(ctx *c, ref p, ref m, ref fin, lwalk *L, size_t mark, int steps)
{
    ref s, num_, den;
    bool under = false;
    int cnt;
    lv r;
    if (c->st == CASIM_NO_ROOM) c->st = CASIM_OK;
    if (c->st != CASIM_OK) return lm(L_UND, 0);
    taylor_drop(c, mark), L->steps = steps;
    s = canon(c, add2(c, p, m)), num_ = small(c, C_0), den = small(c, C_1);
    cnt = nd(c, s)->kind == K_ADD ? nd(c, s)->n : 1;
    for (int i = 0; i < cnt && c->st == CASIM_OK; i++) {
        ref t = nd(c, s)->kind == K_ADD ? list(c, nd(c, s)->a)[i] : s, n, d;
        under |= lm_split(c, t, &n, &d);
        num_ = add2(c, mul2(c, num_, d), mul2(c, n, den));         /* a÷b + n÷d = (a×d + n×b)÷(b×d) */
        den = mul2(c, den, d);
    }
    if (!under || c->st != CASIM_OK) return lm(L_UND, 0);
    r = lm_quot(c, canon(c, num_), canon(c, den), L);
    if (r.k == L_FIN) r.v = canon(c, add2(c, r.v, fin));
    return r;
}

/* p (1 + m÷p): which term dominates; at a finite point, when neither does, one denominator (rule 3) */
static __attribute__((noinline)) lv lm_dominant(ctx *c, ref p, ref m, ref fin, lwalk *L)
{
    size_t mark = c->used;
    int steps = L->steps;
    lv r = lm_quot(c, m, p, L);
    if (r.k == L_PINF || r.k == L_NINF) return lm_walk(c, m, L);
    if (r.k == L_FIN) {
        int s = lm_sgn(c, canon(c, add2(c, small(c, C_1), r.v)));
        if (s == 1) return lm_walk(c, p, L);
        if (s == -1) return lm_walk(c, m, L);
    }
    return L->a ? lm_combine(c, p, m, fin, L, mark, steps) : lm(L_UND, 0);
}

/* the ∞-∞ of a sum: the leading terms at ∞, then a conjugate (a square root in a two-term sum), then which term
   dominates, then (at a finite point) one denominator */
static __attribute__((noinline)) lv lm_inf_minus_inf(ctx *c, ref f, ref p, ref m, ref fin, lwalk *L)
{
    ref k;
    rat deg;
    if (!L->a && lm_lead(c, f, L->var, &k, &deg)) {
        int s = lm_sgn(c, k);
        if (s == 2) return lm(L_UND, 0);
        if (rat_cmp(&deg, &(rat){0, 1}) > 0) return lm(s > 0 ? L_PINF : L_NINF, 0);
        if (deg.p == 0) return lm(L_FIN, k);
        return lm(L_FIN, small(c, C_0));
    }
    {
        /* p + m with a root: (p^2 - m^2) ÷ (p - m) */
        if (lm_root(c, p) || lm_root(c, m)) {
            ref num_ = canon(c, add2(c, power(c, p, small(c, C_2)), neg(c, power(c, m, small(c, C_2)))));
            ref den = canon(c, add2(c, p, neg(c, m)));
            lv r = lm_quot(c, num_, den, L);
            if (r.k == L_FIN) r.v = canon(c, add2(c, r.v, fin));      /* the sum's finite terms beside p + m */
            return r;
        }
    }
    return lm_dominant(c, p, m, fin, L);
}

static __attribute__((noinline)) lv lm_sum(ctx *c, ref f, lwalk *L)
{
    ref s = small(c, C_0), pos = small(c, C_0), negs = small(c, C_0);
    int pinf = 0, ninf = 0, osc = 0, none = 0;
    for (int i = 0; i < nd(c, f)->n; i++) {
        ref g = list(c, nd(c, f)->a)[i];
        lv e = lm_walk(c, g, L);
        if (e.k == L_UND) return e;
        if (e.k == L_FIN) s = add2(c, s, e.v);
        else if (e.k == L_PINF) pinf++, pos = add2(c, pos, g);
        else if (e.k == L_NINF) ninf++, negs = add2(c, negs, g);
        else if (e.k == L_OSC) osc++;
        else none++;
    }
    /* rule 5: one oscillating term beside terms with finite limits has none (SIN(X)+1); two may cancel
       (SIN(X)+SIN(X+π) is 0), and one beside an ∞ is undecided (SIN(X)+X) */
    if (none || osc) return osc + none == 1 && !pinf && !ninf ? lm(L_NONE, 0) : lm(L_UND, 0);
    if (pinf && ninf) return lm_inf_minus_inf(c, f, canon(c, pos), canon(c, negs), canon(c, s), L);
    if (pinf) return lm(L_PINF, 0);
    if (ninf) return lm(L_NINF, 0);
    return lm(L_FIN, canon(c, s));
}

static __attribute__((noinline)) lv lm_prod(ctx *c, ref f, lwalk *L)
{
    ref fin = small(c, C_1), inf = small(c, C_1), zero = small(c, C_1), v = small(c, C_1);
    int sgn = 1, infs = 0, zeros = 0, osc = 0;
    for (int i = 0; i < nd(c, f)->n; i++) {
        ref g = list(c, nd(c, f)->a)[i];
        lv e = lm_walk(c, g, L);
        if (e.k == L_UND || e.k == L_NONE) return lm(L_UND, 0);
        if (e.k == L_OSC) { osc++; fin = mul2(c, fin, g); continue; }
        if (e.k == L_FIN) {
            int s = lm_sgn(c, e.v);
            if (s == 0) zeros++, zero = mul2(c, zero, g);
            else v = mul2(c, v, e.v), fin = mul2(c, fin, g), sgn *= s == 2 ? 1 : s;
        } else infs++, inf = mul2(c, inf, g), sgn *= e.k == L_PINF ? 1 : -1;
    }
    if (osc) return osc == 1 && !infs && !zeros && lm_sgn(c, canon(c, v)) != 2 ? lm(L_NONE, 0) : lm(L_UND, 0);
    if (zeros && infs) {
        /* 0 × ∞ as a quotient, inverting the factor whose reciprocal stays simple under L'Hôpital: an EXP going to 0
           (X×EXP(-X) is X÷EXP(X)), else an algebraic ∞ (X×LN(1+1÷X) is LN(1+1÷X)÷X^-1), else the zeros (X×LN(X) is
           LN(X)÷X^-1, (X-a)×TAN(X) is TAN(X)÷(X-a)^-1) */
        ref z = canon(c, zero), i = canon(c, inf), k;
        rat d;
        if (c->st != CASIM_OK) return lm(L_UND, 0);
        if (!(nd(c, z)->kind == K_FN && nd(c, z)->id == F_EXP) && lm_lead(c, i, L->var, &k, &d))
            return lm_quot(c, canon(c, mul2(c, z, fin)), canon(c, power(c, i, small(c, C_M1))), L);
        if (c->st != CASIM_OK) return lm(L_UND, 0);
        k = nd(c, z)->kind == K_FN && nd(c, z)->id == F_EXP ? fn(c, F_EXP, neg(c, nd(c, z)->a))   /* EXP(-u) */
                                                            : power(c, z, small(c, C_M1));
        return lm_quot(c, canon(c, mul2(c, i, fin)), canon(c, k), L);
    }
    if (zeros) return lm(L_FIN, small(c, C_0));
    if (infs) return lm_sgn(c, canon(c, v)) == 2 ? lm(L_UND, 0) : lm(sgn > 0 ? L_PINF : L_NINF, 0);
    return lm(L_FIN, canon(c, v));
}

static __attribute__((noinline)) lv lm_func(ctx *c, ref f, lwalk *L)
{
    int id = nd(c, f)->id;
    ref u = nd(c, f)->a;
    lv lu;
    if (id == F_INV) return lm_quot(c, small(c, C_1), u, L);
    lu = lm_walk(c, u, L);
    if (id == F_TAN) {                                  /* rule 5: TAN of an argument going to ±∞ has no limit */
        if (lu.k == L_PINF || lu.k == L_NINF) return lm(L_OSC, 0);
        if (lu.k != L_FIN) return lm(L_UND, 0);
        return lm_quot(c, canon(c, fn(c, F_SIN, u)), canon(c, fn(c, F_COS, u)), L);   /* its poles are a quotient's */
    }
    if (lu.k == L_UND || lu.k == L_NONE || lu.k == L_OSC) return lm(L_UND, 0);
    if (id == F_SGN) {                                  /* ABS's derivative: u's sign near the point, also where u -> 0 */
        int sg = lu.k == L_PINF ? 1 : lu.k == L_NINF ? -1 : lm_sgn(c, lu.v);
        if (sg == 0) sg = lm_sign_near(c, u, L);
        return sg == 1 || sg == -1 ? lm(L_FIN, small(c, sg == 1 ? C_1 : C_M1)) : lm(L_UND, 0);
    }
    if (lu.k == L_FIN) {
        ref w = canon(c, fn(c, id, lu.v));
        if (c->st == CASIM_UNDEFINED) c->st = CASIM_OK;
        else if (c->st == CASIM_OK) {
            abn x;
            int cv = const_val(c, w, &x);
            if (cv == CV_OK) return lm(L_FIN, w);
            for (int l = 0; l < 26; l++)
                if (contains(c, w, l)) return lm(L_FIN, w);
        }
        if ((id == F_LN || id == F_LOG) && lm_sgn(c, lu.v) == 0)     /* LN(u), u -> 0 from above */
            return lm_sign_near(c, u, L) == 1 ? lm(L_NINF, 0) : lm(L_UND, 0);
        return lm(L_UND, 0);
    }
    switch (id) {
    case F_EXP: case F_ALOG: return lu.k == L_PINF ? lm(L_PINF, 0) : lm(L_FIN, small(c, C_0));
    case F_LN: case F_LOG: case F_SQRT: return lu.k == L_PINF ? lm(L_PINF, 0) : lm(L_UND, 0);
    case F_ATAN:
        return lm(L_FIN, canon(c, mul2(c, mul2(c, small(c, lu.k == L_PINF ? C_HALF : C_MHALF), leaf(c, K_PI, 0)),
                                         angle_factor(c, true))));
    case F_SINH: return lu;
    case F_COSH: return lm(L_PINF, 0);
    case F_TANH: return lm(L_FIN, small(c, lu.k == L_PINF ? C_1 : C_M1));
    case F_SIN: case F_COS: return lm(L_OSC, 0);
    default: return lm(L_UND, 0);
    }
}

static lv lm_walk(ctx *c, ref f, lwalk *L)
{
    const node *x;
    ref v, n, d;
    lv r;
    if (c->st != CASIM_OK) return lm(L_UND, 0);
    if (!contains(c, f, L->var)) return lm(L_FIN, f);
    if (L->a && lm_at(c, f, L, &v)) return lm(L_FIN, v);          /* continuity */
    if (c->st != CASIM_OK || !enter(c)) return lm(L_UND, 0);
    x = nd(c, f);
    if (x->kind == K_VAR) r = L->a ? lm(L_FIN, L->a) : lm(L_PINF, 0);
    else if ((x->kind == K_MUL || x->kind == K_POW) && lm_split(c, f, &n, &d)) r = lm_quot(c, n, d, L);
    else if (x->kind == K_ADD) r = lm_sum(c, f, L);
    else if (x->kind == K_MUL) r = lm_prod(c, f, L);
    else if (x->kind == K_POW && contains(c, x->b, L->var))       /* b^e = EXP(e LN(b)) */
        r = lm_walk(c, canon(c, fn(c, F_EXP, mul2(c, nd(c, f)->b, fn(c, F_LN, nd(c, f)->a)))), L);
    else if (x->kind == K_POW) {
        ref e = x->b;
        lv lb = lm_walk(c, x->a, L);
        int se = lm_sgn(c, e);
        if (lb.k == L_FIN && lm_sgn(c, lb.v) == 0 && se != 1) r = lm(L_UND, 0);   /* 0^K: K's sign decides */
        else if (lb.k == L_FIN) {
            ref w = canon(c, power(c, lb.v, e));
            r = c->st == CASIM_OK ? lm(L_FIN, w) : lm(L_UND, 0);
            if (c->st == CASIM_UNDEFINED) c->st = CASIM_OK;
        } else if (lb.k == L_PINF) r = se == 1 ? lm(L_PINF, 0) : se == -1 ? lm(L_FIN, small(c, C_0)) : lm(L_UND, 0);
        else if (lb.k == L_NINF && is_num(c, e) && val(c, e)->q == 1 && se > 0)
            r = lm(val(c, e)->p % 2 ? L_NINF : L_PINF, 0);
        else r = lm(L_UND, 0);
    } else if (x->kind == K_FN) r = lm_func(c, f, L);
    else r = lm(L_UND, 0);
    c->depth--;
    return r;
}

static __attribute__((noinline)) casim_status do_limit(const char *eqn, int var, const char *point, int side, int angle,
                                                       char *out, size_t cap, void *arena, size_t arena_cap, bool rtl)
{
    ctx c;
    ref f = SPARE;
    abn av;
    lv r;
    lwalk L = {var, side, 0, 0};
    size_t ocap = cap < SLOT ? cap : SLOT;
    int chars = 0, inf = 0;
    out_t w = {out, ocap, 0, false};
    if (cap) out[0] = '\0';
    if (!eqn || !point || side < -1 || side > 1) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    if (!strcmp(point, "\xe2\x88\x9e")) inf = 1;
    if (!strcmp(point, "-\xe2\x88\x9e")) inf = -1;
    if (inf && side) return CASIM_BAD_ARG;              /* rule 2: the side of ∞ is implied */
    start(&c, eqn, angle, arena, arena_cap);
    c.rtl = rtl;
    c.var = var;                        /* the names derive refuses are refused (not opaque): L'Hôpital derives */
    c.sub_var = -1;
    if (c.st == CASIM_OK) f = parse(&c);
    if (c.st == CASIM_OK && nd(&c, f)->kind == K_REQ) return CASIM_BAD_ARG;      /* an equation */
    if (c.st == CASIM_OK) f = canon(&c, f);
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    if (inf < 0) {                                      /* -∞: +∞ of f(-X) */
        c.sub_var = var, c.sub_val = neg(&c, leaf(&c, K_VAR, var));
        f = canon(&c, check_rewrite(&c, f));
        c.sub_val = 0, c.sub_var = -1;
    }
    if (!inf) {
        for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
        L.a = read_bound(&c, point, chars + 1, &av);
        c.s = eqn;
        if (c.st == CASIM_UNDEFINED) c.st = CASIM_BAD_ARG;     /* a point with no value (1÷0): UNDEFINED is "no limit" */
    }
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    if (L.a && !side) {
        /* both sides: f's value where it has one, else each one-sided limit, which must agree */
        ref v;
        if (lm_at(&c, f, &L, &v)) r = lm(L_FIN, v);
        else {
            lv up, dn;
            size_t mark = c.used;
            L.side = 1, up = lm_walk(&c, f, &L);
            if (up.k == L_FIN) up.v = compact(&c, mark, up.v);     /* CAS 016: the - side gets the arena back */
            else if (c.st == CASIM_OK) taylor_drop(&c, mark);
            L.side = -1, L.steps = 0, dn = lm_walk(&c, f, &L);
            if (up.k == L_UND || dn.k == L_UND) r = lm(L_UND, 0);
            else if (up.k == L_NONE || dn.k == L_NONE || up.k == L_OSC || dn.k == L_OSC) r = lm(L_NONE, 0);
            else if (up.k != dn.k || (up.k == L_FIN && cmp(&c, canon(&c, up.v), canon(&c, dn.v)) != 0))
                r = lm(L_NONE, 0);
            else r = up;
        }
    } else r = lm_walk(&c, f, &L);
    if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
    if (r.k == L_FIN) return finish(&c, canon(&c, r.v), out, ocap);
    if (r.k == L_PINF || r.k == L_NINF) {
        puts_(&w, r.k == L_PINF ? "\xe2\x88\x9e" : "-\xe2\x88\x9e");
        if (!w.full) return CASIM_OK;
        if (ocap) out[0] = '\0';
        return CASIM_TOO_LONG;
    }
    return r.k == L_UND ? CASIM_UNDECIDED : CASIM_UNDEFINED;
}

casim_status casim_limit(const char *eqn, int var, const char *point, const char *side, int model, int angle, char *out,
                         size_t cap, void *arena, size_t arena_cap)
{
    int s;
    if (cap) out[0] = '\0';
    if (!model_ok(model) || !side) return CASIM_BAD_ARG;
    s = !strcmp(side, "+") ? 1 : !strcmp(side, "-") ? -1 : !*side ? 0 : 2;
    if (s == 2) return CASIM_BAD_ARG;
    return do_limit(eqn, var, point, s, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}

/* ---- sums (unit CAS 018, work/018-sums.md) ----
   Σ f for var = lo..hi. Integer bounds at most 64 terms apart are summed term by term (rule 7). Otherwise, terms
   coef × var^j × B^var by an antidifference, checked exactly (rule 4), and the rest through partfrac, as text, then
   telescoped (rule 5); to ∞, group by group (rule 8). */

#define SM_DEG 12                       /* var's highest power in a group */
#define SM_GROUPS 4                     /* ratios */
#define SM_TERMS 6                      /* rational terms, each through partfrac */
#define SM_SHIFT 12                     /* fractions in one telescoping class, and their widest shift */
#define SM_DIRECT 64                    /* rule 7's most terms */

/* tests/smcheck_check.c spoils F before its check (no input reaches the check as built: the antidifference is exact) */
#ifndef CASIM_SM_PLANT
#define CASIM_SM_PLANT(c, F) (F)
#endif

typedef struct {
    ref B;                              /* the ratio; 0 for a polynomial */
    ref q[SM_DEG + 1];                  /* q(var)'s coefficients, free of var */
    int d;
} smgroup;

typedef struct {
    const char *eqn;
    int var;
    bool inf, ilo, ihi;                 /* hi is ∞; a bound is an integer */
    int64_t a, b;                       /* ... its value */
    ref raw, f, e, lo, hi;              /* the typed summand, its canonical form, expanded (q(k)×B^k: the terms
                                           split), the bounds (hi 0 at ∞) */
} smsum;

static ref sm_int(ctx *c, int64_t k)
{
    rat r = {k, 1};
    return num(c, &r);
}

/* whether r holds a literal (a number past 2^62, kept as its text: an offset into the text it was read from) */
static bool sm_lit(ctx *c, ref r)
{
    const node *x = nd(c, r);
    bool h = x->kind == K_LIT;
    if (h || !enter(c)) return true;
    if (x->kind == K_FN || x->kind == K_POW || x->kind == K_OFN)
        h = sm_lit(c, x->a) || ((x->kind == K_POW || (x->kind == K_OFN && nd(c, r)->b)) && sm_lit(c, nd(c, r)->b));
    else if (x->kind == K_ADD || x->kind == K_MUL)
        for (int i = 0; i < x->n && !h; i++) h = sm_lit(c, list(c, nd(c, r)->a)[i]);
    c->depth--;
    return h;
}

/* rule 2: a bound, read as defint reads its bounds but free only of var; a constant one a whole number */
static ref sm_bound(ctx *c, const char *t, int at, int var, bool *whole, int64_t *v)
{
    ref r;
    bool letter = false;
    c->s2 = t, c->s = t, c->no_eq = true;
    r = parse(c);
    if (c->st == CASIM_SYNTAX) c->errpos += at;
    if (c->st == CASIM_OK) r = ib_canon(c, r);
    if (c->st == CASIM_UNDEFINED) c->st = CASIM_BAD_ARG;               /* a bound with no value (1÷0) */
    if (c->st != CASIM_OK) return SPARE;
    for (int l = 0; l < 26; l++) letter = letter || contains(c, r, l);
    *whole = is_num(c, r) && val(c, r)->q == 1;
    if (*whole) *v = val(c, r)->p;
    /* var, 1.5, π; a number past 2^62, whose text the next bound's would replace */
    if (contains(c, r, var) || (!letter && !*whole) || sm_lit(c, r)) c->st = CASIM_BAD_ARG;
    return c->st == CASIM_OK ? r : SPARE;
}

/* the request read: the summand typed and canonical, then the bounds (a SYNTAX offset as if the call were eqn|lo|hi) */
static void sm_read(ctx *c, smsum *S, const char *eqn, const char *lo, const char *hi)
{
    int chars = 0;
    c->opaque = true, c->var = S->var, c->sub_var = -1;
    if (c->st == CASIM_OK) S->raw = parse(c);
    if (c->st == CASIM_OK && nd(c, S->raw)->kind == K_REQ) c->st = CASIM_BAD_ARG;   /* an equation */
    if (c->st == CASIM_OK) S->f = ib_canon(c, S->raw);
    if (c->st != CASIM_OK) return;
    for (const char *p = eqn; *p; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    S->lo = sm_bound(c, lo, chars + 1, S->var, &S->ilo, &S->a);
    for (const char *p = lo; *p && c->st == CASIM_OK; p++) chars += ((unsigned char)*p & 0xC0) != 0x80;
    if (!S->inf && c->st == CASIM_OK) S->hi = sm_bound(c, hi, chars + 2, S->var, &S->ihi, &S->b);
    c->s = eqn;
    if (c->st == CASIM_OK) S->e = expand_fully(c, S->f);
}

/* rule 3: ib_keeps, but a positive constant to a power is never 0 either (2^K, as EXP(K)) */
static bool sm_keeps(ctx *c, ref d, ref t, int var)
{
    ref dc = ib_inv(c, ib_canon(c, d));
    int n = nd(c, dc)->kind == K_MUL ? nd(c, dc)->n : 1;
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref f = nd(c, dc)->kind == K_MUL ? list(c, nd(c, dc)->a)[i] : dc, b = f;
        if (!contains(c, f, var)) continue;
        if (nd(c, f)->kind == K_POW) b = nd(c, f)->a;
        if (nd(c, b)->kind == K_FN && (nd(c, b)->id == F_EXP || nd(c, b)->id == F_ALOG)) continue;
        if (!contains(c, b, var) && const_sign(c, b) > 0) continue;
        if (!ib_den(c, t, b)) return false;
    }
    return c->st == CASIM_OK;
}

/* rule 3: ib_domain_kept without its constant-base rule ((-1)^K has a value at every whole K); its own copy, since a
   second call made gcc stop inlining ib_domain_kept (ib_typed_ok's frame moved 80 -> 64) */
static bool sm_domain(ctx *c, ref r, ref t, int var)
{
    const node *x = nd(c, r);
    if (x->kind == K_RFN && raw_has_var(c, x->a, var) && contains(c, ib_canon(c, x->a), var)) {
        x = nd(c, r);
        switch (x->id) {
        case F_LN: case F_LOG: case F_ASIN: case F_ACOS: case F_ATANH: case F_ACOSH: case F_TAN:
            return ib_holds(c, t, ib_canon(c, r), false);
        case F_SQRT: return ib_holds(c, t, ib_canon(c, x->a), true) || ib_holds(c, t, ib_canon(c, r), false);
        default: return true;
        }
    }
    x = nd(c, r);
    if (x->kind == K_BPOW && raw_has_var(c, x->a, var) && !raw_has_var(c, x->b, var) &&
        contains(c, ib_canon(c, nd(c, r)->a), var)) {
        ref e = ib_canon(c, nd(c, r)->b);
        if (nd(c, e)->kind != K_NUM || val(c, e)->q != 1) return ib_holds(c, t, ib_canon(c, nd(c, r)->a), true);
    }
    return c->st == CASIM_OK;
}

/* rule 3: CAS 017 rule 2 on the typed text (a hole canon lost), except that a constant base of 0 or less to a power of
   var keeps its value at a whole var: (-1)^K */
static bool sm_typed_ok(ctx *c, ref r, ref t, int var)
{
    const node *x = nd(c, r);
    bool ok = true;
    if (r == 0 || r == SPARE || !raw_has_var(c, r, var) || !enter(c)) return true;
    if (x->kind == K_BDIV) ok = sm_keeps(c, x->b, t, var);
    else if (x->kind == K_RFN && x->id == F_INV) ok = sm_keeps(c, x->a, t, var);
    else if (x->kind == K_BPOW && !raw_has_var(c, x->b, var)) {
        ref e = ib_canon(c, nd(c, r)->b);
        if (nd(c, e)->kind == K_NUM && val(c, e)->p < 0) ok = sm_keeps(c, nd(c, r)->a, t, var);
    }
    x = nd(c, r);
    if (ok) ok = sm_domain(c, r, t, var);
    x = nd(c, r);
    if (ok && (x->kind == K_RFN || x->kind == K_NEG || x->kind == K_GROUP)) ok = sm_typed_ok(c, x->a, t, var);
    else if (ok && x->kind >= K_BADD && x->kind <= K_BPOW)
        ok = sm_typed_ok(c, x->a, t, var) && sm_typed_ok(c, nd(c, r)->b, t, var);
    c->depth--;
    return ok && c->st == CASIM_OK;
}

/* t's polynomial in var (pw reads raw trees: printed and read back, as ib_whole does) */
static bool sm_poly(ctx *c, ref t, int var, rat **lin)
{
    const char *s0 = c->s;
    size_t top = c->top;
    char *buf = ib_print(c, t, IB_TEXT);
    bool ok;
    ref r;
    if (!buf && c->st == CASIM_NOT_INTEGRABLE) c->st = CASIM_NO_ROOM;     /* past IB_TEXT: integrate's word */
    if (!buf) return false;
    c->s = buf;
    r = parse(c);
    ok = c->st == CASIM_OK && poly_of(c, r, var, lin);
    c->s = s0, c->top = top;
    return ok;
}

/* whether B holds a letter A-Z (rule 4b: answered with its condition); i is not one, and is refused with it */
static bool sm_letter(ctx *c, ref B)
{
    bool h = false;
    for (int l = 0; l < 26; l++) h = h || contains(c, B, l);
    return h && !contains(c, B, 26);
}

/* rule 3: t as coef × var^j × B^var (*B 0 for none); false when it isn't that shape */
static bool sm_term(ctx *c, ref t, int var, ref *coef, int *j, ref *B)
{
    int n = nd(c, t)->kind == K_MUL ? nd(c, t)->n : 1;
    *coef = small(c, C_1), *j = 0, *B = 0;
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref f = nd(c, t)->kind == K_MUL ? list(c, nd(c, t)->a)[i] : t, base = 0, e, a, b, Bn;
        const node *y = nd(c, f);
        rat *lin;
        if (!contains(c, f, var)) {
            *coef = mul2(c, *coef, f);
            continue;
        }
        if (y->kind == K_VAR) {
            (*j)++;
            continue;
        }
        if (y->kind == K_POW && nd(c, y->a)->kind == K_VAR && is_num(c, y->b) && val(c, y->b)->q == 1 &&
            val(c, y->b)->p > 0 && val(c, y->b)->p <= SM_DEG) {
            *j += (int)val(c, y->b)->p;
            continue;
        }
        if (y->kind == K_POW && !contains(c, y->a, var)) base = y->a, e = y->b;
        else if (y->kind == K_FN && y->id == F_EXP) e = y->a;
        else if (y->kind == K_FN && y->id == F_ALOG) base = small(c, C_10), e = y->a;
        else if (y->kind == K_POW && is_num(c, y->b) && nd(c, y->a)->kind == K_FN &&
                 (nd(c, y->a)->id == F_EXP || nd(c, y->a)->id == F_ALOG)) {
            /* EXP(u)^n, ALOG(u)^n (1÷EXP(K)): EXP(n×u), 10^(n×u) */
            if (nd(c, y->a)->id == F_ALOG) base = small(c, C_10);
            e = mul2(c, nd(c, f)->b, nd(c, nd(c, f)->a)->a);
        } else return false;
        if (!sm_poly(c, e, var, &lin) || pdeg(lin) != 1) return false;
        /* a base that isn't a positive constant: a and b whole ((-2)^(K÷2) has no value at an odd K) */
        if (base && const_sign(c, base) <= 0 && (lin[1].q != 1 || lin[0].q != 1)) return false;
        /* 0^(K+1) isn't 0^1 × (0^1)^K at K = -1; a letter's rows (rule 4b) say where its formula holds */
        if (base && const_sign(c, base) == 0 && !sm_letter(c, base)) return false;
        a = num(c, &lin[1]), b = num(c, &lin[0]);
        Bn = ib_canon(c, base ? power(c, base, a) : fn(c, F_EXP, a));
        *coef = mul2(c, *coef, base ? power(c, base, b) : fn(c, F_EXP, b));
        *B = *B ? ib_canon(c, mul2(c, *B, Bn)) : Bn;
    }
    *coef = ib_canon(c, *coef);
    if (*B && is_k(c, *B, 1, 1)) *B = 0;
    return *j <= SM_DEG && c->st == CASIM_OK;
}

static void sm_add(ctx *c, smgroup *G, int *ng, ref coef, int j, ref B)
{
    int gi;
    for (gi = 0; gi < *ng; gi++)
        if ((!G[gi].B && !B) || (G[gi].B && B && cmp(c, G[gi].B, B) == 0)) break;
    if (gi == *ng) {
        if (*ng == SM_GROUPS) {
            c->st = CASIM_NO_CLOSED_FORM;
            return;
        }
        G[gi].B = B, G[gi].d = 0;
        for (int q = 0; q <= SM_DEG; q++) G[gi].q[q] = small(c, C_0);
        (*ng)++;
    }
    G[gi].q[j] = add2(c, G[gi].q[j], coef);
    if (j > G[gi].d) G[gi].d = j;
}

/* the canonical summand's terms: polynomial-times-geometric ones into G, the rest into R (by tree) */
static void sm_split(ctx *c, ref f, int var, smgroup *G, int *ng, ref *R, int *nr)
{
    int n = nd(c, f)->kind == K_ADD ? nd(c, f)->n : 1;
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref t = nd(c, f)->kind == K_ADD ? list(c, nd(c, f)->a)[i] : f, coef, B;
        int j;
        if (sm_term(c, t, var, &coef, &j, &B)) sm_add(c, G, ng, coef, j, B);
        else if (c->st == CASIM_OK) {
            if (*nr == SM_TERMS) c->st = CASIM_NO_CLOSED_FORM;
            else R[(*nr)++] = t;
        }
    }
}

/* rule 4: F with B×F(k+1) - F(k) = q(k) (B 1 when none), top down; SPARE when B - 1 is 0 */
static ref sm_anti(ctx *c, const smgroup *g, ref B, int var)
{
    ref F[SM_DEG + 2], x = leaf(c, K_VAR, var), acc = small(c, C_0);
    for (int i = 0; i <= SM_DEG + 1; i++) F[i] = small(c, C_0);
    if (!B) {
        for (int i = g->d; i >= 0; i--) {
            ref s = g->q[i];
            int64_t bin = i + 1;                                        /* C(i+1, i) */
            for (int j = i + 2; j <= g->d + 1; j++) {
                bin = bin * j / (j - i);                                /* C(j, i), each step whole */
                s = add2(c, s, ib_neg(c, mul2(c, sm_int(c, bin), F[j])));
            }
            F[i + 1] = ib_canon(c, mul2(c, s, power(c, sm_int(c, i + 1), small(c, C_M1))));
        }
    } else {
        ref den = ib_canon(c, add2(c, B, small(c, C_M1)));
        if (is_k(c, den, 0, 1)) return SPARE;
        for (int i = g->d; i >= 0; i--) {
            ref s = small(c, C_0);
            int64_t bin = 1;
            for (int j = i + 1; j <= g->d; j++) {
                bin = bin * j / (j - i);
                s = add2(c, s, mul2(c, sm_int(c, bin), F[j]));
            }
            s = add2(c, g->q[i], ib_neg(c, mul2(c, B, s)));
            F[i] = ib_canon(c, mul2(c, s, power(c, den, small(c, C_M1))));
        }
    }
    for (int i = 0; i <= SM_DEG + 1; i++)
        if (!is_k(c, F[i], 0, 1)) acc = add2(c, acc, mul2(c, F[i], power(c, x, sm_int(c, i))));
    return ib_canon(c, acc);
}

/* rule 5: a rational term as one n÷d text for partfrac, its factors free of var left out (they multiply its answer:
   CAS 017 rule 1's constant factor); false when it doesn't fit */
static bool sm_frac(ctx *c, ref t, int var, char *buf, size_t cap)
{
    int n = nd(c, t)->kind == K_MUL ? nd(c, t)->n : 1;
    ref nu = small(c, C_1), de = small(c, C_1);
    out_t w = {buf, cap, 0, false};
    for (int i = 0; i < n; i++) {
        ref f = nd(c, t)->kind == K_MUL ? list(c, nd(c, t)->a)[i] : t;
        const node *y = nd(c, f);
        if (!contains(c, f, var)) continue;
        if (y->kind == K_POW && is_num(c, y->b) && val(c, y->b)->p < 0 && val(c, y->b)->q == 1) {
            rat e = *val(c, y->b);
            e.p = -e.p;
            de = mul2(c, de, power(c, y->a, num(c, &e)));
        } else nu = mul2(c, nu, f);
    }
    puts_(&w, "(");
    print(c, &w, ib_canon(c, nu));
    puts_(&w, ")" DIVIDE "(");
    print(c, &w, ib_canon(c, de));
    puts_(&w, ")");
    if (!w.full) buf[w.n] = '\0';
    return c->st == CASIM_OK && !w.full;
}

/* the product of t's factors free of var */
static ref sm_const(ctx *c, ref t, int var)
{
    int n = nd(c, t)->kind == K_MUL ? nd(c, t)->n : 1;
    ref k = small(c, C_1);
    for (int i = 0; i < n; i++) {
        ref f = nd(c, t)->kind == K_MUL ? list(c, nd(c, t)->a)[i] : t;
        if (!contains(c, f, var)) k = mul2(c, k, f);
    }
    return ib_canon(c, k);
}

/* rule 5: whether a pole at var = k0 is in the sum's range (lo is an integer here) */
static bool sm_in(const smsum *S, int64_t k0)
{
    return k0 >= S->a && (S->inf || !S->ihi || k0 <= S->b);
}

/* rule 5: partfrac's answers pf[0..nr) telescoped: the sum of the fractions (polynomial parts join G), or SPARE */
static __attribute__((noinline)) ref sm_rational(ctx *c, const smsum *S, ref *R, int nr, char **pf, smgroup *G, int *ng)
{
    struct {
        rat s;
        ref cf;
        int m;
    } tm[SM_SHIFT];
    int nt = 0, var = S->var;
    ref T = small(c, C_0);
    for (int r = 0; r < nr && c->st == CASIM_OK; r++) {
        ref P, k = sm_const(c, R[r], var);
        char *rows = strchr(pf[r], ';');
        bool rtl = c->rtl;
        if (rows) *rows = '\0';
        c->s = pf[r], c->rtl = false;
        P = parse(c);
        c->rtl = rtl;
        if (rows) *rows = ';';
        P = ib_canon(c, P);
        if (c->st == CASIM_OK && sm_lit(c, P)) c->st = CASIM_NO_ROOM;     /* its text is partfrac's, not eqn */
        c->s = S->eqn;
        /* the rows: a value the cancelling removed ("K=a") is a hole of the summand */
        for (char *p = rows; p && c->st == CASIM_OK; p = strchr(p + 1, ';')) {
            ref v;
            const char *q = p + 1;
            while (*q == ' ') q++;
            if (q[0] != 'A' + var || q[1] != '=') continue;           /* "p=0": no rational root */
            {
                char *e = strchr(q, ';');
                if (e) *e = '\0';
                c->s = q + 2;
                v = parse(c);
                v = ib_canon(c, v);                             /* a raw number reads its digits from c->s */
                c->s = S->eqn;
                if (e) *e = ';';
            }
            if (c->st == CASIM_OK && is_num(c, v) && val(c, v)->q == 1 && sm_in(S, val(c, v)->p))
                c->st = CASIM_UNDEFINED;
        }
        for (int i = 0, n = nd(c, P)->kind == K_ADD ? nd(c, P)->n : 1; i < n && c->st == CASIM_OK; i++) {
            ref t = nd(c, P)->kind == K_ADD ? list(c, nd(c, P)->a)[i] : P, coef, B, f = 0, cf = k;
            int j, m = nd(c, t)->kind == K_MUL ? nd(c, t)->n : 1;
            rat *lin, ia, s, iam;
            if (sm_term(c, t, var, &coef, &j, &B)) {
                sm_add(c, G, ng, ib_canon(c, mul2(c, k, coef)), j, B);  /* the polynomial part */
                continue;
            }
            for (int q = 0; q < m; q++) {
                ref h = nd(c, t)->kind == K_MUL ? list(c, nd(c, t)->a)[q] : t;
                if (!contains(c, h, var)) cf = mul2(c, cf, h);
                else if (!f) f = h;
                else return c->st = CASIM_NO_CLOSED_FORM, SPARE;
            }
            /* c÷(a×var+b)^m, as (c÷a^m)÷(var+s)^m */
            if (c->st != CASIM_OK || nd(c, f)->kind != K_POW || !is_num(c, nd(c, f)->b) || val(c, nd(c, f)->b)->q != 1 ||
                val(c, nd(c, f)->b)->p >= 0 || !sm_poly(c, nd(c, f)->a, var, &lin) || pdeg(lin) != 1 || nt == SM_SHIFT)
                return c->st = c->st == CASIM_OK ? CASIM_NO_CLOSED_FORM : c->st, SPARE;
            m = (int)-val(c, nd(c, f)->b)->p;
            if (!rat_make(&ia, lin[1].q, lin[1].p) || !rat_mul(&s, &lin[0], &ia) || !rat_pow(&iam, &ia, m))
                return c->st = CASIM_NO_ROOM, SPARE;
            if (s.q == 1 && sm_in(S, -s.p)) return c->st = CASIM_UNDEFINED, SPARE;     /* a pole in the range */
            tm[nt].s = s, tm[nt].m = m, tm[nt].cf = ib_canon(c, mul2(c, cf, num(c, &iam)));
            nt++;
        }
    }
    /* classes by (s mod 1, m), each summing to 0: Σ_{k=lo}^{hi} = Σ_i c_i Σ_{j<d_i} [1÷(hi+1+s0+j)^m - 1÷(lo+s0+j)^m] */
    for (int i = 0; i < nt && c->st == CASIM_OK; i++) {
        rat s0 = tm[i].s;
        int m = tm[i].m;
        ref tot = small(c, C_0), me = sm_int(c, -m);
        if (!tm[i].cf) continue;
        for (int j = 0; j < nt; j++) {
            rat d, ns = {-tm[j].s.p, tm[j].s.q};
            if (tm[j].cf && tm[j].m == m && rat_add(&d, &s0, &ns) && d.q == 1 && d.p > 0) s0 = tm[j].s;
        }
        for (int j = 0; j < nt && c->st == CASIM_OK; j++) {
            rat d, ns = {-s0.p, s0.q};
            if (!tm[j].cf || tm[j].m != m || !rat_add(&d, &tm[j].s, &ns) || d.q != 1) continue;
            if (d.p > SM_SHIFT) return c->st = CASIM_NO_CLOSED_FORM, SPARE;
            tot = add2(c, tot, tm[j].cf);
            for (int q = 0; q < d.p; q++) {
                rat sq = {q, 1}, w;
                ref at;
                if (!rat_add(&w, &s0, &sq)) return c->st = CASIM_NO_ROOM, SPARE;
                if (!S->inf) {
                    at = add2(c, add2(c, S->hi, small(c, C_1)), num(c, &w));
                    T = add2(c, T, mul2(c, tm[j].cf, power(c, at, me)));
                }
                at = add2(c, S->lo, num(c, &w));
                T = add2(c, T, ib_neg(c, mul2(c, tm[j].cf, power(c, at, me))));
            }
            tm[j].cf = 0;
        }
        if (c->st == CASIM_OK && !is_k(c, ib_canon(c, tot), 0, 1)) return c->st = CASIM_NO_CLOSED_FORM, SPARE;
    }
    return ib_canon(c, T);
}

/* q(var) of a group */
static ref sm_q(ctx *c, const smgroup *g, ref x)
{
    ref q = small(c, C_0);
    for (int j = 0; j <= g->d; j++) q = add2(c, q, mul2(c, g->q[j], power(c, x, sm_int(c, j))));
    return q;
}

static bool sm_mono(ctx *c, ref B, rat *k, int *L, int *m);

/* the most negative whole power of the letter L among t's factors, 0 for none */
static int sm_negpow(ctx *c, ref t, int L)
{
    const node *x = nd(c, t);
    const ref *f = x->kind == K_MUL ? list(c, x->a) : &t;
    int n = x->kind == K_MUL ? x->n : 1, e = 0;
    for (int i = 0; i < n; i++) {
        const node *y = nd(c, f[i]);
        if (y->kind == K_POW && nd(c, y->a)->kind == K_VAR && nd(c, y->a)->id == L && is_num(c, y->b) &&
            val(c, y->b)->q == 1 && val(c, y->b)->p < e)
            e = (int)val(c, y->b)->p;
    }
    return e;
}

/* rule 4b, abacus #8349: a letter group q(var)×B^var whose q holds a negative power of B's letter L (K×X^(K-1): q is
   K÷X) is summed shifted, as q(var+s)×B^s×B^var over lo-s .. hi-s, with s the least that clears them; the answer then
   divides by no power of L, and needs no row for L = 0 that the sum doesn't. g->q becomes q(var+s)×B^s; returns s, 0
   when there is none to clear (or past SM_DEG, or a binomial that overflows) */
static __attribute__((noinline)) int sm_shift(ctx *c, smgroup *g)
{
    rat k;
    int L, m, s = 0;
    ref Bs;
    if (!g->B || !sm_mono(c, g->B, &k, &L, &m) || m <= 0) return 0;
    for (int j = 0; j <= g->d; j++) {
        int e = sm_negpow(c, g->q[j], L);
        if (e < 0 && (-e + m - 1) / m > s) s = (-e + m - 1) / m;
    }
    if (s == 0 || s > SM_DEG) return 0;
    Bs = power(c, g->B, sm_int(c, s));
    for (int i = 0; i <= g->d; i++) {
        /* q'(i) = B^s × Σ_{j >= i} q(j) × C(j, i) × s^(j-i) */
        ref t = small(c, C_0);
        for (int j = i; j <= g->d; j++) {
            int64_t w = 1;
            for (int r = 0; r < j - i; r++)
                if (!mul64(&w, w, (int64_t)(j - r)) || !mul64(&w, w, s)) return 0;
            for (int r = 1; r <= j - i; r++) w /= r;                   /* C(j, i) s^(j-i): j!/(i!) / (j-i)! s^(j-i) */
            t = add2(c, t, mul2(c, sm_int(c, w), g->q[j]));
        }
        g->q[i] = ib_canon(c, expand_fully(c, ib_canon(c, mul2(c, Bs, t))));
    }
    return c->st == CASIM_OK ? s : 0;
}

/* rule 8: |B| (1 for a polynomial) against |B2|: the sign of B^2 - B2^2; 2 when exactly equal, 0 when undecided */
static int sm_cmpabs(ctx *c, ref B, ref B2)
{
    ref one = small(c, C_1), two = small(c, C_2);
    ref d = ib_canon(c, add2(c, power(c, B ? B : one, two), ib_neg(c, power(c, B2 ? B2 : one, two))));
    if (c->st != CASIM_OK) return 0;
    return is_k(c, d, 0, 1) ? 2 : const_sign(c, d);
}

/* rules 4 and 8: each group's part of the sum into *S (lo's part alone at ∞); at ∞ a divergent sum's status, or ∞
   (*dir 1 or -1) */
static __attribute__((noinline)) ref sm_groups(ctx *c, const smsum *S, smgroup *G, int ng, int *dir)
{
    int var = S->var, best = -1, W = -1;
    bool tie = false;
    ref sum = small(c, C_0), x = leaf(c, K_VAR, var);
    for (int l = 22; l >= 0 && W < 0; l--)                          /* W, V, U, ... : a letter in nothing here */
        if (l != var && !contains(c, S->f, l) && !contains(c, S->lo, l) && !(S->hi && contains(c, S->hi, l))) W = l;
    for (int gi = 0; gi < ng; gi++)
        for (int j = 0; j <= G[gi].d; j++) G[gi].q[j] = ib_canon(c, G[gi].q[j]);
    for (int gi = 0; gi < ng && c->st == CASIM_OK; gi++) {
        smgroup *g = &G[gi];
        ref F, q, z, B = g->B, top, lo;
        size_t mark;
        bool zero = true;
        int sh;
        for (int j = 0; j <= g->d; j++) zero = zero && is_k(c, g->q[j], 0, 1);
        if (zero) continue;
        sh = sm_shift(c, g);                                        /* rule 4b: lo-sh .. hi-sh, q shifted */
        mark = c->used;                                             /* each group's work is given back (not q's) */
        if (B) {
            /* rule 4: a rational B is solved as it is; another with B = W+1, so the check's
               divisions are powers of W, which expand merges (by B-1 they don't: EXP(1)-1), and W is B-1 after */
            ref Bw = B;
            bool w = !is_num(c, B);
            if (w && W < 0) return c->st = CASIM_NO_CLOSED_FORM, SPARE;
            /* B - 1 must be decidably nonzero, or B hold a letter (rule 4b: its rows say where it is 1): i^K, or a B
               that is 1 but not written so (SQRT(2)^K×SQRT(0.5)^K), would divide by 0 */
            if (w && !sm_letter(c, B) && const_sign(c, ib_canon(c, add2(c, B, small(c, C_M1)))) == 0)
                return c->st = CASIM_NO_CLOSED_FORM, SPARE;
            if (w) Bw = ib_canon(c, add2(c, leaf(c, K_VAR, W), small(c, C_1)));
            F = compact(c, mark, CASIM_SM_PLANT(c, sm_anti(c, g, Bw, var)));
            if (w) Bw = ib_canon(c, add2(c, leaf(c, K_VAR, W), small(c, C_1)));      /* the compact took it */
            q = sm_q(c, g, x);
            z = add2(c, add2(c, mul2(c, Bw, ib_at(c, F, var, add2(c, x, small(c, C_1)))), ib_neg(c, F)), ib_neg(c, q));
            if (c->st == CASIM_OK && (F == SPARE || !ib_same(c, z, small(c, C_0))))
                return c->st = c->st == CASIM_OK ? CASIM_NO_CLOSED_FORM : c->st, SPARE;
            if (w) F = ib_at(c, F, W, add2(c, B, small(c, C_M1)));
        } else {
            F = compact(c, mark, CASIM_SM_PLANT(c, sm_anti(c, g, 0, var)));
            q = sm_q(c, g, x);
            z = add2(c, add2(c, ib_at(c, F, var, add2(c, x, small(c, C_1))), ib_neg(c, F)), ib_neg(c, q));
            if (c->st == CASIM_OK && !ib_same(c, z, small(c, C_0)))
                return c->st = c->st == CASIM_OK ? CASIM_NO_CLOSED_FORM : c->st, SPARE;
        }
        if (c->st != CASIM_OK || F == SPARE) return c->st = c->st == CASIM_OK ? CASIM_NO_CLOSED_FORM : c->st, SPARE;
        F = compact(c, mark, F);
        lo = sh ? add2(c, S->lo, sm_int(c, -sh)) : S->lo;
        sum = add2(c, sum, ib_neg(c, mul2(c, ib_at(c, F, var, lo), B ? power(c, B, lo) : small(c, C_1))));
        if (!S->inf) {
            top = add2(c, S->hi, sm_int(c, 1 - sh));
            sum = add2(c, sum, mul2(c, ib_at(c, F, var, top), B ? power(c, B, top) : small(c, C_1)));
        }
        sum = compact(c, mark, ib_canon(c, sum));
        if (!S->inf || (B && sm_letter(c, B))) continue;           /* rule 4b: a letter group's own row at ∞ */
        /* rule 8: converges when |B| < 1; the dominant divergent group by |B|, then degree */
        {
            int s = sm_cmpabs(c, B, 0);
            if (s == 0) return c->st = c->st == CASIM_OK ? CASIM_UNDECIDED : c->st, SPARE;
            if (s < 0) continue;
            if (best < 0) {
                best = gi;
                continue;
            }
            s = sm_cmpabs(c, B, G[best].B);
            if (s == 0) return c->st = c->st == CASIM_OK ? CASIM_UNDECIDED : c->st, SPARE;
            if (s == 2) {
                /* |B| = 1 both: a polynomial's partial sums grow a degree faster than its terms, a swing's don't */
                int e = g->d + !B, eb = G[best].d + !G[best].B;
                if (e == eb) tie = true;
                else if (e > eb) best = gi, tie = false;
            } else if (s > 0) best = gi, tie = false;
        }
    }
    if (c->st != CASIM_OK || best < 0) return c->st == CASIM_OK ? sum : SPARE;
    {
        smgroup *g = &G[best];
        int sg;
        if (tie) return c->st = CASIM_UNDECIDED, SPARE;
        sg = const_sign(c, g->q[g->d]);                        /* a leading coefficient that may be 0: undecided */
        if (g->B && const_sign(c, g->B) == 0) sg = 0;
        if (sg == 0) return c->st = c->st == CASIM_OK ? CASIM_UNDECIDED : c->st, SPARE;
        if (g->B && const_sign(c, g->B) < 0) return c->st = CASIM_UNDEFINED, SPARE;      /* it swings: no limit */
        *dir = sg;
    }
    return sum;
}

/* rule 7: term by term, each from the typed text with var replaced by its value */
static ref sm_direct(ctx *c, const smsum *S)
{
    ref acc = small(c, C_0);
    size_t mark = c->used;
    for (int64_t k = S->a; k <= S->b && c->st == CASIM_OK; k++) {
        ref v;
        c->sub_var = S->var, c->sub_val = sm_int(c, k);
        v = ib_canon(c, S->raw);
        c->sub_val = 0, c->sub_var = -1;
        acc = compact(c, mark, ib_canon(c, add2(c, acc, v)));
    }
    return acc;
}

/* rule 4b: a condition row, l ≠ r ('!'), l < r or l > r */
typedef struct {
    ref l, r;
    char op;
    int letter;                         /* its order: the letter solved for, or 26 for B itself */
} smrow;

#define SM_ROWS (2 * SM_GROUPS)

/* B as k × L^m: a rational k, one letter L, a whole m != 0 (|m| <= 12) */
static bool sm_mono(ctx *c, ref B, rat *k, int *L, int *m)
{
    int n = nd(c, B)->kind == K_MUL ? nd(c, B)->n : 1;
    *k = (rat){1, 1}, *L = -1, *m = 0;
    for (int i = 0; i < n; i++) {
        ref f = nd(c, B)->kind == K_MUL ? list(c, nd(c, B)->a)[i] : B;
        const node *x = nd(c, f);
        if (x->kind == K_NUM) *k = *val(c, f);
        else if (*L >= 0) return false;
        else if (x->kind == K_VAR && x->id < 26) *L = x->id, *m = 1;
        else if (x->kind == K_POW && nd(c, x->a)->kind == K_VAR && nd(c, x->a)->id < 26 && is_num(c, x->b) &&
                 val(c, x->b)->q == 1 && val(c, x->b)->p != 0 && val(c, x->b)->p >= -SM_DEG && val(c, x->b)->p <= SM_DEG)
            *L = nd(c, x->a)->id, *m = (int)val(c, x->b)->p;
        else return false;
    }
    return *L >= 0 && k->p != 0;
}

static void sm_row(ctx *c, smrow *rows, int *n, ref l, char op, ref r, int letter)
{
    int at;
    if (c->st != CASIM_OK) return;
    for (int i = 0; i < *n; i++)
        if (rows[i].op == op && cmp(c, rows[i].l, l) == 0 && cmp(c, rows[i].r, r) == 0) return;      /* once */
    if (*n == SM_ROWS) {
        c->st = CASIM_NO_ROOM;
        return;
    }
    /* by letter, then by value ascending (X≠-1 before X≠1) */
    for (at = *n; at > 0; at--) {
        const smrow *p = &rows[at - 1];
        if (p->letter < letter) break;
        if (p->letter == letter && const_sign(c, ib_canon(c, add2(c, p->r, ib_neg(c, r)))) <= 0) break;
    }
    memmove(&rows[at + 1], &rows[at], (size_t)(*n - at) * sizeof rows[0]);
    rows[at] = (smrow){l, r, op, letter};
    (*n)++;
}

/* rule 4b: each letter group's rows. Finite: where B is 1 (k×L^m = 1 solved for L, else B ≠ 1). At ∞: where it
   converges, |B| < 1 (|L| < or > |k|^(-1/m), else ABS(B) < 1) */
static void sm_rows(ctx *c, const smsum *S, const smgroup *G, int ng, ref sum, smrow *rows, int *n)
{
    for (int gi = 0; gi < ng && c->st == CASIM_OK; gi++) {
        ref B = G[gi].B, one = small(c, C_1);
        rat k, t;
        int L, m;
        bool zero = true;
        for (int j = 0; j <= G[gi].d; j++) zero = zero && is_k(c, G[gi].q[j], 0, 1);
        if (zero || !B || !sm_letter(c, B)) continue;
        /* L ≠ 0, for each letter L of B, where the answer itself has no value at 0, though the sum may (the cold
           review: K×X^(K-1)'s X÷(X-X^2), X^(-K) from 0). Asked of the answer: X^(K-1)'s merges to X^N and needs none.
           X^(-K) from a lo of 1 or more has no value at 0 anyway (abacus #8320 (b)). The probe's nodes are given back
           before the row is made */
        for (int l = 0; l < 26 && c->st == CASIM_OK; l++) {
            size_t mark = c->used;
            bool hole;
            if (!contains(c, B, l) || (sm_mono(c, B, &k, &L, &m) && m < 0 && S->ilo && S->a >= 1)) continue;
            ib_at(c, sum, l, small(c, C_0));
            hole = c->st == CASIM_UNDEFINED;
            if (hole) c->st = CASIM_OK;
            if (c->st == CASIM_OK) compact(c, mark, sum);
            if (hole) sm_row(c, rows, n, leaf(c, K_VAR, l), '!', small(c, C_0), l);
        }
        if (!sm_mono(c, B, &k, &L, &m)) {
            if (S->inf) sm_row(c, rows, n, fn(c, F_ABS, B), '<', one, 26);
            else sm_row(c, rows, n, B, '!', one, 26);
            continue;
        }
        if (S->inf) {
            /* |k| |L|^m < 1: |L| < |k|^(-1/m) for m > 0, > for m < 0 */
            rat ak = k, e = {-1, m};
            if (ak.p < 0) ak.p = -ak.p;
            if (e.q < 0) e.p = -e.p, e.q = -e.q;
            sm_row(c, rows, n, fn(c, F_ABS, leaf(c, K_VAR, L)), m > 0 ? '<' : '>',
                   ib_canon(c, power(c, num(c, &ak), num(c, &e))), L);
            continue;
        }
        /* L^m = 1/k: m odd, one root; m even, two when 1/k > 0, none when < 0. k is in lowest terms with q > 0, so 1/k
           is too (no rat_make: a new call to it moves lm_sum's frame, CAS 017's lesson) */
        t = k.p < 0 ? (rat){-k.q, -k.p} : (rat){k.q, k.p};
        {
            bool neg = t.p < 0;
            rat at = t, e = {1, m};
            ref u;
            if (neg) at.p = -at.p;
            if (e.q < 0) e.p = -e.p, e.q = -e.q;
            u = ib_canon(c, power(c, num(c, &at), num(c, &e)));
            if (m % 2) sm_row(c, rows, n, leaf(c, K_VAR, L), '!', neg ? ib_canon(c, ib_neg(c, u)) : u, L);
            else if (!neg) {
                sm_row(c, rows, n, leaf(c, K_VAR, L), '!', ib_canon(c, ib_neg(c, u)), L);
                sm_row(c, rows, n, leaf(c, K_VAR, L), '!', u, L);
            }
        }
    }
}

/* the answer (∞ or -∞ when dir), then its rows after "; " (CANCEL's) */
static casim_status sm_print(ctx *c, ref sum, int dir, const smrow *rows, int n, char *out, size_t ocap)
{
    out_t w = {out, ocap, 0, false};
    if (c->st != CASIM_OK || (!dir && !n)) return finish(c, dir ? SPARE : sum, out, ocap);
    if (ocap == 0) return CASIM_TOO_LONG;
    if (dir) puts_(&w, dir > 0 ? "\xe2\x88\x9e" : "-\xe2\x88\x9e");
    else print(c, &w, sum);
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        puts_(&w, "; ");
        print(c, &w, rows[i].l);
        puts_(&w, rows[i].op == '!' ? "\xe2\x89\xa0" : rows[i].op == '<' ? "<" : ">");
        print(c, &w, rows[i].r);
    }
    if (c->st == CASIM_OK && w.full) c->st = CASIM_TOO_LONG;
    if (c->st != CASIM_OK) {
        out[0] = '\0';
        return c->st;
    }
    out[w.n] = '\0';
    return CASIM_OK;
}

/* closed: rule 7's sum was past out, so a closed form instead (rule 4b), its rows saying where it holds; *direct when
   rule 7's sum was the answer */
static __attribute__((noinline)) casim_status do_sum_(const char *eqn, int var, const char *lo, const char *hi, int angle,
                                                      char *out, size_t cap, void *arena, size_t arena_cap, bool rtl,
                                                      bool closed, bool *direct)
{
    ctx c;
    smsum S;
    smgroup G[SM_GROUPS];
    ref R[SM_TERMS], sum, T = 0;
    char *pf[SM_TERMS];
    size_t ocap = cap < SLOT ? cap : SLOT, top = arena_cap;
    int ng = 0, nr = 0, dir = 0, nrows = 0;
    smrow rows[SM_ROWS];
    bool swung;
    if (cap) out[0] = '\0';
    if (!eqn || !lo || !hi) return CASIM_BAD_ARG;
    if (var < 0 || var > 25 || angle < CASIM_DEG || angle > CASIM_GRAD) return CASIM_BAD_ARG;
    memset(&S, 0, sizeof S);
    S.var = var, S.eqn = eqn;
    S.inf = !strcmp(hi, "\xe2\x88\x9e");
    if (!strcmp(lo, "\xe2\x88\x9e") || !strcmp(lo, "-\xe2\x88\x9e") || !strcmp(hi, "-\xe2\x88\x9e")) return CASIM_BAD_ARG;
    /* rule 5 first: each rational term's n÷d through partfrac, on the whole arena but the texts kept at its top */
    for (int r = 0;; r++) {
        size_t len, at;
        char *buf;
        casim_status st;
        start(&c, eqn, angle, arena, top > IB_TEXT ? top - IB_TEXT : 0);
        c.rtl = rtl;
        sm_read(&c, &S, eqn, lo, hi);
        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
        if (S.ilo && S.ihi && S.b < S.a) return finish(&c, small(&c, C_0), out, ocap);   /* rule 2: none */
        if (!S.inf && !(S.ilo && S.ihi)) {
            /* symbolic bounds a known whole number apart: hi < lo is the empty sum too (N+2 to N) */
            ref span = ib_canon(&c, add2(&c, S.hi, ib_neg(&c, S.lo)));
            if (c.st == CASIM_OK && is_num(&c, span) && val(&c, span)->q == 1 && val(&c, span)->p < 0)
                return finish(&c, small(&c, C_0), out, ocap);
        }
        if (S.ilo && S.ihi && (uint64_t)S.b - (uint64_t)S.a < SM_DIRECT && !closed)
            return *direct = true, finish(&c, sm_direct(&c, &S), out, ocap);
        if (r == 0 && !sm_typed_ok(&c, S.raw, S.f, var) && c.st == CASIM_OK) c.st = CASIM_NO_CLOSED_FORM;
        ng = nr = 0;
        if (c.st == CASIM_OK) sm_split(&c, S.e, var, G, &ng, R, &nr);
        if (c.st == CASIM_OK && nr && !S.ilo) c.st = CASIM_NO_CLOSED_FORM;        /* rule 2: it would divide by lo */
        /* a telescoped form over fewer terms than its shifts can name a pole outside the range (rule 7) */
        if (c.st == CASIM_OK && nr && closed && (uint64_t)S.b - (uint64_t)S.a < SM_SHIFT) c.st = CASIM_TOO_LONG;
        if (c.st != CASIM_OK) return finish(&c, SPARE, out, ocap);
        if (r == nr) break;
        buf = (char *)arena + top - IB_TEXT;
        if (top < 2 * IB_TEXT + SLOT || !sm_frac(&c, R[r], var, buf, IB_TEXT))
            return c.st = c.st == CASIM_OK ? CASIM_NO_ROOM : c.st, finish(&c, SPARE, out, ocap);
        len = strlen(buf) + 1;
        memmove((char *)arena + top - len, buf, len);
        at = top - len - SLOT;
        st = do_partfrac((char *)arena + top - len, var, angle, (char *)arena + at, SLOT, arena, at, false);
        if (st == CASIM_TOO_LONG) st = CASIM_NO_ROOM;
        else if (st == CASIM_PARTIAL) st = CASIM_OK;
        else if (st != CASIM_OK && st != CASIM_NO_ROOM && st != CASIM_UNDEFINED) st = CASIM_NO_CLOSED_FORM;
        if (st != CASIM_OK) return st;
        len = strlen((char *)arena + at) + 1;
        memmove((char *)arena + top - len, (char *)arena + at, len);
        top -= len;
        pf[r] = (char *)arena + top;
    }
    /* the sum, in what's left */
    start(&c, eqn, angle, arena, top);
    c.rtl = rtl;
    sm_read(&c, &S, eqn, lo, hi);
    ng = nr = 0;
    if (c.st == CASIM_OK) sm_split(&c, S.e, var, G, &ng, R, &nr);
    if (c.st == CASIM_OK && nr) T = sm_rational(&c, &S, R, nr, pf, G, &ng);
    swung = c.st == CASIM_OK;                   /* an UNDEFINED from here on is rule 8's swing, not a pole */
    sum = c.st == CASIM_OK ? sm_groups(&c, &S, G, ng, &dir) : SPARE;
    if (c.st == CASIM_UNDEFINED && S.inf && swung) {
        /* a swing beside a letter group: the letter can outgrow it (X^K+(-2)^K at X = 2), so not proven */
        bool letter = false;
        for (int gi = 0; gi < ng; gi++) letter = letter || (G[gi].B && sm_letter(&c, G[gi].B));
        if (letter) c.st = CASIM_UNDECIDED;
    }
    if (c.st == CASIM_OK && !dir) sum = expand_fully(&c, ib_canon(&c, sum));
    if (c.st == CASIM_OK && !dir && T) sum = ib_canon(&c, add2(&c, sum, T));
    if (c.st == CASIM_OK) sm_rows(&c, &S, G, ng, dir ? small(&c, C_0) : sum, rows, &nrows);
    return sm_print(&c, sum, dir, rows, nrows, out, ocap);
}

/* rule 7 first: whole bounds at most 64 apart are summed term by term; when that sum doesn't fit out, the closed
   form and its rows (rule 4b: X+X^2+X^3 needs no row), and TOO_LONG when there's none */
static __attribute__((noinline)) casim_status do_sum(const char *eqn, int var, const char *lo, const char *hi, int angle,
                                                     char *out, size_t cap, void *arena, size_t arena_cap, bool rtl)
{
    bool direct = false;
    casim_status st = do_sum_(eqn, var, lo, hi, angle, out, cap, arena, arena_cap, rtl, false, &direct);
    if (st != CASIM_TOO_LONG || !direct) return st;
    st = do_sum_(eqn, var, lo, hi, angle, out, cap, arena, arena_cap, rtl, true, &direct);
    if (st != CASIM_OK && cap) out[0] = '\0';
    return st == CASIM_OK ? st : CASIM_TOO_LONG;
}

casim_status casim_sum(const char *eqn, int var, const char *lo, const char *hi, int model, int angle, char *out,
                       size_t cap, void *arena, size_t arena_cap)
{
    if (cap) out[0] = '\0';
    if (!model_ok(model)) return CASIM_BAD_ARG;
    return do_sum(eqn, var, lo, hi, angle, out, cap, arena, arena_cap, model == CASIM_MODEL_STU);
}
