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
    CASIM_TOO_LONG,             /* out or the arena too small for this equation; out is "" */
    CASIM_BAD_ARG               /* var outside 0-25 or angle outside 0-2 (ours); out is "" */
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

#endif
