/* Casimir's host command line (units CAS 001 and 002, rule 1):
 *
 *   casim derive "<text>" X RAD      the derivative's text, simplified, or the status
 *   casim simplify "<text>" RAD      the text in its canonical form, or the status
 *   casim expand "<text>" RAD        expanded (CAS 003)
 *   casim collect "<text>" X RAD     collected in X
 *   casim subst "<text>" X "<u>" RAD every X replaced by u, simplified
 *   casim solve "<text>" X RAD       the real roots in X, exactly, its status first (CAS 005)
 *   --arena N                        the arena in bytes (default 8192, the unit's S3 budget)
 *   --min-arena                      also print the smallest arena that gives the same result
 *   --cap N                          out's size in bytes, its NUL included (default 65536): 192 is
 *                                    the calculator's equation slot, which derive and simplify keep as
 *                                    the caller's cap (CAS 004, soroban #4831)
 *   --model STU|33|35                the model the text was typed in, as the core stamps it (CAS 004;
 *                                    default 35); --repl-model the same for subst's replacement
 *
 * Exit 0 for OK, 1 for a status (printed as SYNTAX <offset>, NOT_DIFFERENTIABLE <name>,
 * TOO_LONG, BAD_ARG or NO_ROOM), 2 for a usage error. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"

#define OUT 65536
static unsigned char arena[1 << 20];

enum { OP_DERIVE = CASIM_DERIVE, OP_SIMPLIFY = CASIM_SIMPLIFY, OP_EXPAND = CASIM_EXPAND, OP_COLLECT = CASIM_COLLECT,
       OP_SUBST = CASIM_SUBST, OP_SOLVE = CASIM_SOLVE };

static int model = CASIM_MODEL_35, repl_model = CASIM_MODEL_35;
static size_t out_cap = OUT;

/* The calculator's own call (casim_run), with the texts' models. */
static casim_status run(int op, const char *text, int var, const char *repl, int angle, char *out, size_t arena_cap)
{
    casim_req rq = {(casim_op)op, text, model, var, repl, repl_model, angle};
    return casim_run(&rq, out, out_cap, arena, arena_cap);
}

static int model_arg(const char *a)
{
    return strcmp(a, "STU") == 0 ? CASIM_MODEL_STU : strcmp(a, "33") == 0 ? CASIM_MODEL_33
         : strcmp(a, "35") == 0 ? CASIM_MODEL_35 : -1;
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
        else if (strcmp(argv[i], "--cap") == 0 && i + 1 < argc) {
            out_cap = strtoul(argv[++i], NULL, 10);
            if (out_cap == 0 || out_cap > OUT) goto usage;
        } else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            if ((model = model_arg(argv[++i])) < 0) goto usage;
        } else if (strcmp(argv[i], "--repl-model") == 0 && i + 1 < argc) {
            if ((repl_model = model_arg(argv[++i])) < 0) goto usage;
        }
        else goto usage;
    }
    if (cap > sizeof arena || i >= argc) goto usage;
    op = strcmp(argv[i], "derive") == 0 ? OP_DERIVE : strcmp(argv[i], "simplify") == 0 ? OP_SIMPLIFY
       : strcmp(argv[i], "expand") == 0 ? OP_EXPAND : strcmp(argv[i], "collect") == 0 ? OP_COLLECT
       : strcmp(argv[i], "subst") == 0 ? OP_SUBST : strcmp(argv[i], "solve") == 0 ? OP_SOLVE : -1;
    args = op == OP_DERIVE || op == OP_COLLECT || op == OP_SOLVE ? 4 : op == OP_SUBST ? 5 : 3;
    if (op < 0 || argc - i != args) goto usage;
    if (op == OP_DERIVE || op == OP_COLLECT || op == OP_SUBST || op == OP_SOLVE) {
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
    case CASIM_NO_ROOM: printf("NO_ROOM\n"); break;
    case CASIM_NOT_POLYNOMIAL: printf("NOT_POLYNOMIAL\n"); break;
    case CASIM_NOT_NUMERIC: printf("NOT_NUMERIC\n"); break;
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
                    "usage: casim [--arena N] [--min-arena] [--model STU|33|35] derive \"<text>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] simplify \"<text>\" RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] expand \"<text>\" RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] collect \"<text>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] subst \"<text>\" X \"<u>\" RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] solve \"<text>\" X RAD|DEG|GRAD\n"
                    "       (every op takes --model and --cap N; subst also --repl-model; defaults 35, 65536)\n");
    return 2;
}
