/* Casimir's host command line (units CAS 001 and 002, rule 1):
 *
 *   casim derive "<text>" X RAD      the derivative's text, simplified, or the status
 *   casim simplify "<text>" RAD      the text in its canonical form, or the status
 *   casim expand "<text>" RAD        expanded (CAS 003)
 *   casim collect "<text>" X RAD     collected in X
 *   casim subst "<text>" X "<u>" RAD every X replaced by u, simplified
 *   --arena N                        the arena in bytes (default 8192, the unit's S3 budget)
 *   --min-arena                      also print the smallest arena that gives the same result
 *
 * Exit 0 for OK, 1 for a status (printed as SYNTAX <offset>, NOT_DIFFERENTIABLE <name>,
 * TOO_LONG or BAD_ARG), 2 for a usage error. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"

#define OUT 65536
static unsigned char arena[1 << 20];

enum { OP_DERIVE, OP_SIMPLIFY, OP_EXPAND, OP_COLLECT, OP_SUBST };

static casim_status run(int op, const char *text, int var, const char *repl, int angle, char *out, size_t arena_cap)
{
    switch (op) {
    case OP_DERIVE: return casim_derive(text, var, angle, out, OUT, arena, arena_cap);
    case OP_EXPAND: return casim_expand(text, angle, out, OUT, arena, arena_cap);
    case OP_COLLECT: return casim_collect(text, var, angle, out, OUT, arena, arena_cap);
    case OP_SUBST: return casim_subst(text, var, repl, angle, out, OUT, arena, arena_cap);
    default: return casim_simplify(text, angle, out, OUT, arena, arena_cap);
    }
}

int main(int argc, char **argv)
{
    size_t cap = 8192;
    int min_arena = 0, op, var = 0, angle = CASIM_RAD, i = 1, args;
    const char *repl = NULL;
    char out[OUT];
    casim_status st;
    for (; i < argc && strncmp(argv[i], "--", 2) == 0; i++) {
        if (strcmp(argv[i], "--min-arena") == 0) min_arena = 1;
        else if (strcmp(argv[i], "--arena") == 0 && i + 1 < argc) cap = strtoul(argv[++i], NULL, 10);
        else goto usage;
    }
    if (cap > sizeof arena || i >= argc) goto usage;
    op = strcmp(argv[i], "derive") == 0 ? OP_DERIVE : strcmp(argv[i], "simplify") == 0 ? OP_SIMPLIFY
       : strcmp(argv[i], "expand") == 0 ? OP_EXPAND : strcmp(argv[i], "collect") == 0 ? OP_COLLECT
       : strcmp(argv[i], "subst") == 0 ? OP_SUBST : -1;
    args = op == OP_DERIVE || op == OP_COLLECT ? 4 : op == OP_SUBST ? 5 : 3;
    if (op < 0 || argc - i != args) goto usage;
    if (op == OP_DERIVE || op == OP_COLLECT || op == OP_SUBST) {
        if (strlen(argv[i + 2]) != 1) goto usage;
        var = argv[i + 2][0] - 'A';
    }
    if (op == OP_SUBST) repl = argv[i + 3];
    {
        const char *a = argv[argc - 1];
        angle = strcmp(a, "DEG") == 0 ? CASIM_DEG : strcmp(a, "RAD") == 0 ? CASIM_RAD
              : strcmp(a, "GRAD") == 0 ? CASIM_GRAD : -1;
    }
    st = run(op, argv[i + 1], var, repl, angle, out, cap);
    switch (st) {
    case CASIM_OK: printf("%s\n", out); break;
    case CASIM_SYNTAX: printf("SYNTAX %s\n", out); break;
    case CASIM_NOT_DIFFERENTIABLE: printf("NOT_DIFFERENTIABLE %s\n", out); break;
    case CASIM_TOO_LONG: printf("TOO_LONG\n"); break;
    case CASIM_BAD_ARG: printf("BAD_ARG\n"); break;
    }
    if (min_arena && st == CASIM_OK) {
        /* the smallest arena giving OK, by bisection over [0, cap] */
        size_t lo = 0, hi = cap;
        char tmp[OUT];
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (run(op, argv[i + 1], var, repl, angle, tmp, mid) == CASIM_OK) hi = mid;
            else lo = mid + 1;
        }
        printf("arena %zu\n", lo);
    }
    return st == CASIM_OK ? 0 : 1;
usage:
    fprintf(stderr, "Casimir, computer algebra for the STU-32\n"
                    "usage: casim [--arena N] [--min-arena] derive \"<text>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] simplify \"<text>\" RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] expand \"<text>\" RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] collect \"<text>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] subst \"<text>\" X \"<u>\" RAD|DEG|GRAD\n");
    return 2;
}
