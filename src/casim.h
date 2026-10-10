/* Casimir (casim): computer algebra in portable C11 for a calculator, and for anything else that can call C.
 *
 * Each operation reads an expression as text and writes its answer as text (docs/language.md: the input language,
 * the models, the angle modes and each operation's statuses, with examples). The answers are exact. The operations
 * that search for an answer check it before writing it, and refuse when the check cannot close: integrate
 * differentiates its answer back, solve divides out each root exactly, factor reads its product back as the input,
 * pdiv checks f = q g + r, pgcd and cancel accept a gcd only when it divides both exactly. The rewriting operations
 * (derive, simplify, expand, collect, subst) apply only rules that keep the value; the tests check them.
 *
 * The contract, for every call here:
 *  - The caller gives all the memory: out (cap bytes, its NUL included) and an arena (arena_cap bytes, any
 *    alignment; 8 KiB is the calculator's budget, and at most 524,280 bytes of it are used). Nothing is allocated.
 *  - out always holds a NUL-terminated text when cap >= 1. A status other than CASIM_OK says what out holds.
 *  - Text is UTF-8 with the calculator's symbols (× for times, ÷ for divide, π); var is 0-25 for A-Z.
 *  - No state outlives a call: casim keeps no globals that a call writes, so calls on separate arenas and outputs
 *    do not touch each other.
 *  - The numbers are IEEE 754 decimal128 (34 digits) through abn.h; no binary floating point reaches an answer.
 *
 * The work units (work/CAS NNN) say why each rule is what it is; docs/canonical-form.md defines the output form.
 */
#ifndef CASIM_H
#define CASIM_H

#include <stddef.h>

/* The release, as semver; casim_version() returns the same text from the compiled library. */
#define CASIM_VERSION "1.0.0-rc5"
#define CASIM_VERSION_MAJOR 1
#define CASIM_VERSION_MINOR 0
#define CASIM_VERSION_PATCH 0

const char *casim_version(void);

typedef enum {
    CASIM_OK = 0,
    CASIM_SYNTAX,               /* out: the character offset of the first error, in decimal */
    CASIM_NOT_DIFFERENTIABLE,   /* out: what was refused: a function's name, "!", "[", or the literal */
    CASIM_TOO_LONG,             /* the result does not fit: past 191 bytes (expand, collect, subst), past cap,
                                   or more terms than 191 bytes can hold; out is "" */
    CASIM_BAD_ARG,              /* var outside 0-25 or angle outside 0-2 (ours); out is "" */
    CASIM_NO_ROOM,              /* CAS 004: the arena ran out, or the depth guard stopped a walk (the stack's
                                   bound); out is "". TOO_LONG before CAS 004. CAS 005: past solve's bounds */
    CASIM_NOT_POLYNOMIAL,       /* CAS 005, solve: var inside a function, under a root, in a denominator, or to a
                                   power not a whole number >= 0; out is "" */
    CASIM_NOT_NUMERIC,          /* CAS 005, solve: a coefficient not an exact rational number (another letter,
                                   π, SQRT(2), a draw); out is "" */
    CASIM_NOT_INTEGRABLE,       /* CAS 006, integrate: not one of the table's forms as typed, or an answer the
                                   derive-back check could not close; out is "" */
    CASIM_PARTIAL,              /* CAS 007, factor: out holds a correct factorization (it reads back as the
                                   input exactly), but a factor of degree >= 4 is not proven irreducible (rule 5a);
                                   the firmware shows MAY FACTOR FURTHER beside it */
    CASIM_UNDEFINED,            /* CAS 008: an exact 0 under a negative power (1÷COS(π÷2), 1÷0): the expression
                                   has no value; out is "" */
    CASIM_IMPROPER              /* CAS 009, defint: a pole or domain edge of the integrand on the closed interval
                                   (or within 1E-20 of it), so the integral is improper there; out is "" */
} casim_status;

/* The angle modes, numbered as the core's ab_angle (AB_ANGLE_DEG, _RAD, _GRAD). */
#define CASIM_DEG 0
#define CASIM_RAD 1
#define CASIM_GRAD 2

/* d/d(var) of eqn: an expression gives its derivative, L=R gives L'=R' (unit CAS 001). var is 0-25
   for A-Z. The text written to out (cap bytes, NUL included) is valid in the core's grammar. */
casim_status casim_derive(const char *eqn, int var, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap);

/* eqn in its canonical form (unit CAS 002): one text for each expression, the same value wherever eqn
   is defined; L=R simplifies each side. angle matters only to the derivative, which casim_derive also
   returns simplified. A construct casim_derive refuses is refused here too, by name. */
casim_status casim_simplify(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap);

/* Unit CAS 003. Each returns TOO_LONG for a result past 191 bytes, whatever cap is (an equation slot
   holds 192 with its NUL); L=R treats each side. */

/* eqn expanded: × distributed over +, sums raised to whole powers multiplied out, at every depth (function
   arguments too), in the canonical form. A sum under a ÷ or to a power not whole stays, expanded inside. */
casim_status casim_expand(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap);

/* eqn collected in var (0-25): Σ c_k × var^k, each k once, in descending k, each c_k simplified and free of
   var; terms where var is not a whole power follow, simplified (docs/canonical-form.md, "Collect"). */
casim_status casim_collect(const char *eqn, int var, int angle, char *out, size_t cap, void *arena,
                           size_t arena_cap);

/* eqn with every var (0-25) replaced by repl, as if in parentheses, then simplified. */
casim_status casim_subst(const char *eqn, int var, const char *repl, int angle, char *out, size_t cap, void *arena,
                         size_t arena_cap);

/* Unit CAS 004: the five operations through one call, each text read as the model it was typed in, as the
   core stamps an equation (unit 031): CASIM_MODEL_STU groups ^ right to left (2^3^2 is 2^(3^2)), and a
   minus that starts an exponent covers the chain after it (2^-3^2 is 2^-(3^2)); 33 and 35 group left to
   right; 0 reads as 35, as the core's eqn_eval. Implied products are not read: the core writes their ×
   into a stored STU equation (unit 031 rule 7). The output reads the same under every model: each chain
   of powers has its parentheses, each product its ×. The five calls above read 35. */
#define CASIM_MODEL_STU 1           /* = the core's AB_MODEL_STU */
#define CASIM_MODEL_33 33
#define CASIM_MODEL_35 35

typedef enum { CASIM_DERIVE, CASIM_SIMPLIFY, CASIM_EXPAND, CASIM_COLLECT, CASIM_SUBST, CASIM_SOLVE, CASIM_INTEGRATE,
               CASIM_FACTOR, CASIM_DEFINT, CASIM_PDIV, CASIM_PGCD, CASIM_CANCEL } casim_op;

typedef struct {
    casim_op op;
    const char *eqn;
    int model;                  /* eqn's: 33, 35, CASIM_MODEL_STU, or 0 (35) */
    int var;                    /* derive, collect, subst: 0-25 for A-Z */
    const char *repl;           /* subst: the replacement; defint: the lower bound */
    int repl_model;             /* subst: repl's model, as model (defint reads both bounds as model) */
    int angle;
    const char *repl2;          /* CAS 009, defint: the upper bound (last, so older initializers leave it NULL) */
} casim_req;

/* rq's operation, as the calls above; BAD_ARG for an op, model, var or angle outside its range. */
casim_status casim_run(const casim_req *rq, char *out, size_t cap, void *arena, size_t arena_cap);

/* Show steps (work/steps-derive.md, steps-integrate.md, steps-solve.md): the working of rq, one step a page, for
   derive, integrate and solve (CASIM_DERIVE, CASIM_INTEGRATE, CASIM_SOLVE; another op is BAD_ARG). casim_step runs rq
   with steps on and prints step k into out: "d/dX(before) = after" for derive, "∫(before)dX = after" for integrate,
   where after is the rule applied with its parts still to do (a template line) or the step's result; integrate's
   CHECK lines are "d/dX(F) = f". The walk is top-down: a template line, its parts' steps, then its result; the
   root's result is the last step and is the op's own answer (never a "+C"), with or without CASIM_STEPS_ONE_LINE,
   which leaves out the template lines (abacus #6252: one rule for the firmware). Solve's steps are the equation as
   it stands, "...=0", at depth 0 (moved, cleared, each factor divided out) and each factor set to 0, "F=0: X=...",
   at depth 1; its last step is solve's answer (ANSWER). k < 0 only counts. Each call is complete in itself: page k
   again or k ± 1 with the same rq. info gets the count, and for step k its depth (0 for the root), its line and its
   rule. NO_ROOM when the steps don't fit the arena (the op may still); BAD_ARG for k >= count; TOO_LONG when the
   step's text passes cap (the count is the same whatever cap is); the op's own refusal otherwise. An equation's
   derive lines are d/dX(a)=d/dX(b) and its derived equation. */
#define CASIM_STEPS_ONE_LINE 1
#define CASIM_STEP_TEMPLATE 0
#define CASIM_STEP_RESULT 1
enum {
    CASIM_RULE_SUM, CASIM_RULE_CONSTANT, CASIM_RULE_VARIABLE, CASIM_RULE_CONSTANT_MULTIPLE, CASIM_RULE_PRODUCT,
    CASIM_RULE_POWER, CASIM_RULE_EXPONENTIAL, CASIM_RULE_GENERAL_POWER, CASIM_RULE_CHAIN, CASIM_RULE_BOTH_SIDES,
    CASIM_RULE_RECIPROCAL,      /* integrate: 1÷(aX+b), to LN(ABS(aX+b))÷a */
    CASIM_RULE_POLYNOMIAL,      /* integrate: a product multiplied out, then term by term */
    CASIM_RULE_CHECK,           /* integrate: the term's answer differentiated back to the term */
    CASIM_RULE_INTEGER_COEFFICIENTS,    /* solve: whole coefficients with no common factor, the leading one positive */
    CASIM_RULE_COMMON_FACTOR,   /* solve: the root 0's factor X^k taken out */
    CASIM_RULE_LINEAR_FACTOR,   /* solve: a linear factor divided out (template: the division) */
    CASIM_RULE_QUADRATIC_FACTOR,        /* solve: a quadratic factor divided out (template: the division) */
    CASIM_RULE_LINEAR,          /* solve: a linear factor = 0 */
    CASIM_RULE_QUADRATIC_FORMULA,       /* solve: a quadratic factor = 0 (template: the formula, or b^2-4ac < 0) */
    CASIM_RULE_LEFT,            /* solve: a factor of degree 3 or more left */
    CASIM_RULE_ANSWER,          /* solve: its answer, the last step */
    CASIM_RULE_FUNCTION         /* + k: a function of var itself (derive), or of aX+b (integrate's table), its name
                                   from casim_rule_name */
};
typedef struct {
    int count;                  /* the steps */
    int depth;                  /* step k's: 0 for the root, 1 for its parts, ... */
    int line;                   /* CASIM_STEP_TEMPLATE or CASIM_STEP_RESULT */
    int rule;                   /* CASIM_RULE_... */
} casim_step_info;
casim_status casim_step(const casim_req *rq, int k, int flags, casim_step_info *info, char *out, size_t cap,
                        void *arena, size_t arena_cap);
/* A rule's name, uppercase and short (SUM, PRODUCT, CHAIN, SIN, ...); "" for an id that is none. */
const char *casim_rule_name(int rule);

/* Unit CAS 005: the real roots of eqn (L=R as L-R=0, an expression as expr=0), a polynomial in var (0-25)
   with exact rational coefficients, read as model. out is one line, its status first (abacus #4841):
     SOLVED: X=2; X=3            every irreducible factor over Q has degree <= 2 (a root's multiplicity
                                  above 1 follows it as " (MULT k)"; ascending; a÷b or (a±b×SQRT(r))÷d)
     PARTIAL, DEGREE n LEFT: ... a factor of degree >= 3 is left (n: the degree left in all)
     NO REAL ROOTS, NO SOLUTION (a nonzero constant), EVERY X (0)
   Degree <= 12 and, cleared to a primitive integer polynomial, coefficients <= 10^7, else NO_ROOM. Every
   root is verified by exact division. TOO_LONG past 191 bytes, whatever cap is. */
casim_status casim_solve(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                         size_t arena_cap);

/* Unit CAS 006: an antiderivative of the expression eqn in var (0-25), read as model, in the angle mode, in 002's
   canonical form and without "+C" (C is a register: the firmware shows "+C" beside the answer). The table's
   forms only, as typed: linearity (constants may hold other letters); x^n (n rational, not -1); 1÷X and
   1÷(aX+b) as LN(ABS(...)); EXP, SIN, COS, TAN, ALOG, SINH, COSH and (aX+b)^n of a linear argument with
   rational a and b; a polynomial times a polynomial, expanded. SIN, COS and TAN carry the angle's factor
   (180÷π, 200÷π). Every answer is differentiated back and must equal the integrand exactly (002, expanded)
   before it prints; anything else is NOT_INTEGRABLE. An equation is BAD_ARG. TOO_LONG past 191 bytes. */
casim_status casim_integrate(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                             size_t arena_cap);

/* Unit CAS 007: the polynomial eqn (in var, as typed, CAS 005's rule 2) factored over Q: the rational content first
   (when not 1), then each irreducible factor once, primitive with integer coefficients and a positive leading
   coefficient, in 002's canonical form, ^k for a multiplicity, in ascending degree (ties in 002's order). Every
   factor of degree >= 2 is proven irreducible (degree 2: its discriminant is not a square; degree >= 3: a complete
   search, docs and rule 5a); the text is read back and must be the input's polynomial exactly. PARTIAL when a
   factor could not be proven irreducible (out is still a correct factorization). An equation is BAD_ARG; TOO_LONG
   past 191 bytes. */
casim_status casim_factor(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap);

/* Unit CAS 009: the definite integral of eqn in var from lo to hi, exact: F(hi) - F(lo) with CAS 006's verified F and
   CAS 008's exact values. The integrand is CAS 006's, as typed (its refusals pass through). The bounds are exact
   constants read as model (a letter is BAD_ARG; no value is UNDEFINED; not real is BAD_ARG); lo > hi gives the
   sign reversed, lo = hi gives 0. IMPROPER when a pole or domain edge of the integrand is on the closed interval
   [min, max] or within 1E-20 of it: u = 0 under a negative power, u < 0 under a non-integer power, TAN's poles in
   the angle mode, for u linear in var; a pole placed by another letter is BAD_ARG. TOO_LONG past 191 bytes. */
casim_status casim_defint(const char *eqn, int var, const char *lo, const char *hi, int model, int angle, char *out,
                          size_t cap, void *arena, size_t arena_cap);

/* Unit CAS 010: polynomials in var with rational coefficients, read as typed by CAS 005's walk (degree <= 12,
   coefficients <= 10^7 once cleared); the second one in repl, read as repl_model.
   PDIV: the quotient and remainder over Q, out "q; r" (f = q g + r, deg r < deg g, checked exactly); g = 0 is
   UNDEFINED; an int64 overflow is NO_ROOM.
   PGCD: the greatest common divisor, the primitive integer polynomial with a positive leading coefficient (a constant
   gcd is 1; gcd(f, 0) is f's primitive part; gcd(0, 0) is 0), found modulo 31-bit primes and accepted only when it
   divides both exactly; NO_ROOM if no candidate passes within the primes tried. */
casim_status casim_pdiv(const char *eqn, int var, const char *repl, int model, int repl_model, int angle, char *out,
                        size_t cap, void *arena, size_t arena_cap);
casim_status casim_pgcd(const char *eqn, int var, const char *repl, int model, int repl_model, int angle, char *out,
                        size_t cap, void *arena, size_t arena_cap);
/* CANCEL: eqn typed as num÷den, reduced by gcd(num, den): out is the reduced quotient, then a row for each value the
   cancelling removed: "X=a" for each rational root of the gcd (ascending, once), then the rest of the gcd as "p=0"
   when it has no rational root. A gcd of 1 prints the quotient in canonical form with no rows. */
casim_status casim_cancel(const char *eqn, int var, int model, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap);

#endif
