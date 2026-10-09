/* The golden corpus's harvester (scripts/golden-harvest.py): linked into a developer test with -Wl,--wrap for each of
 * casim's calls, it appends every call the test makes to $GOLDEN_LOG as a line of golden_check's input (op, model,
 * var, angle, repl_model, arena, cap, eqn, repl, repl2), then makes the call. The arena is logged as the bytes casim
 * can use once it aligns the pointer to 8, so the line replays exactly on golden_check's aligned arena. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "casim.h"

static const char *const OPS[] = {"derive", "simplify", "expand", "collect", "subst", "solve", "integrate", "factor",
                                  "defint", "pdiv", "pgcd", "cancel"};

static void esc(FILE *f, const char *s)
{
    if (!s) {
        fputs("\\N", f);
        return;
    }
    for (; *s; s++) {
        if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\t') fputs("\\t", f);
        else if (*s == '\n') fputs("\\n", f);
        else fputc(*s, f);
    }
}

static void log_call(int op, int model, int var, int angle, int repl_model, const void *arena, size_t arena_cap,
                     size_t cap, const char *eqn, const char *repl, const char *repl2)
{
    static FILE *f;
    uintptr_t p = (uintptr_t)arena, q = (p + 7u) & ~(uintptr_t)7u;
    if (!eqn || op < 0 || op >= (int)(sizeof OPS / sizeof OPS[0])) return;
    if (!f) {
        const char *path = getenv("GOLDEN_LOG");
        if (!path || !(f = fopen(path, "a"))) return;
    }
    arena_cap = arena_cap > q - p ? arena_cap - (q - p) : 0;
    fprintf(f, "%s\t%d\t%d\t%d\t%d\t%zu\t%zu\t", OPS[op], model, var, angle, repl_model, arena_cap, cap);
    esc(f, eqn);
    fputc('\t', f);
    esc(f, repl);
    fputc('\t', f);
    esc(f, repl2);
    fputc('\n', f);
    fflush(f);
}

#define A void *arena, size_t arena_cap
casim_status __real_casim_derive(const char *, int, int, char *, size_t, A);
casim_status __real_casim_simplify(const char *, int, char *, size_t, A);
casim_status __real_casim_expand(const char *, int, char *, size_t, A);
casim_status __real_casim_collect(const char *, int, int, char *, size_t, A);
casim_status __real_casim_subst(const char *, int, const char *, int, char *, size_t, A);
casim_status __real_casim_solve(const char *, int, int, int, char *, size_t, A);
casim_status __real_casim_integrate(const char *, int, int, int, char *, size_t, A);
casim_status __real_casim_factor(const char *, int, int, int, char *, size_t, A);
casim_status __real_casim_run(const casim_req *, char *, size_t, A);
casim_status __wrap_casim_derive(const char *e, int var, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_simplify(const char *e, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_expand(const char *e, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_collect(const char *e, int var, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_subst(const char *e, int var, const char *r, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_solve(const char *e, int var, int model, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_integrate(const char *e, int var, int model, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_factor(const char *e, int var, int model, int angle, char *out, size_t cap, A);
casim_status __wrap_casim_run(const casim_req *rq, char *out, size_t cap, A);

casim_status __wrap_casim_derive(const char *e, int var, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_DERIVE, 35, var, angle, 35, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_derive(e, var, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_simplify(const char *e, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_SIMPLIFY, 35, 0, angle, 35, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_simplify(e, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_expand(const char *e, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_EXPAND, 35, 0, angle, 35, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_expand(e, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_collect(const char *e, int var, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_COLLECT, 35, var, angle, 35, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_collect(e, var, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_subst(const char *e, int var, const char *r, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_SUBST, 35, var, angle, 35, arena, arena_cap, cap, e, r, NULL);
    return __real_casim_subst(e, var, r, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_solve(const char *e, int var, int model, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_SOLVE, model, var, angle, model, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_solve(e, var, model, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_integrate(const char *e, int var, int model, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_INTEGRATE, model, var, angle, model, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_integrate(e, var, model, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_factor(const char *e, int var, int model, int angle, char *out, size_t cap, A)
{
    log_call(CASIM_FACTOR, model, var, angle, model, arena, arena_cap, cap, e, NULL, NULL);
    return __real_casim_factor(e, var, model, angle, out, cap, arena, arena_cap);
}

casim_status __wrap_casim_run(const casim_req *rq, char *out, size_t cap, A)
{
    if (rq) log_call((int)rq->op, rq->model, rq->var, rq->angle, rq->repl_model, arena, arena_cap, cap, rq->eqn, rq->repl,
                     rq->repl2);
    return __real_casim_run(rq, out, cap, arena, arena_cap);
}
