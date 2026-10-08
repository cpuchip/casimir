/* Casimir (casim): computer algebra on the abacus core's equation grammar.
 *
 * Unit CAS 001: the derivative of an equation; CAS 002: simplify, to a canonical form
 * (docs/canonical-form.md). The input is equation text in the core's grammar
 * (core/eqn.h), the output is equation text in the same grammar, so the core can evaluate, SOLVE or
 * store it. All working memory comes from the caller's arena; nothing is allocated.
 */
#ifndef CASIM_H
#define CASIM_H

#include <stddef.h>

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
    CASIM_NOT_INTEGRABLE        /* CAS 006, integrate: not one of the table's forms as typed, or an answer the
                                   derive-back check could not close; out is "" */
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

typedef enum { CASIM_DERIVE, CASIM_SIMPLIFY, CASIM_EXPAND, CASIM_COLLECT, CASIM_SUBST, CASIM_SOLVE, CASIM_INTEGRATE } casim_op;

typedef struct {
    casim_op op;
    const char *eqn;
    int model;                  /* eqn's: 33, 35, CASIM_MODEL_STU, or 0 (35) */
    int var;                    /* derive, collect, subst: 0-25 for A-Z */
    const char *repl;           /* subst: the replacement */
    int repl_model;             /* subst: repl's model, as model */
    int angle;
} casim_req;

/* rq's operation, as the calls above; BAD_ARG for an op, model, var or angle outside its range. */
casim_status casim_run(const casim_req *rq, char *out, size_t cap, void *arena, size_t arena_cap);

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

#endif
