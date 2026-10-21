/* Casimir's host command line (units CAS 001 and 002, rule 1):
 *
 *   casim derive "<text>" X RAD      the derivative's text, simplified, or the status
 *   casim simplify "<text>" RAD      the text in its canonical form, or the status
 *   casim expand "<text>" RAD        expanded (CAS 003)
 *   casim collect "<text>" X RAD     collected in X
 *   casim subst "<text>" X "<u>" RAD every X replaced by u, simplified
 *   casim solve "<text>" X RAD       the real roots in X, exactly, its status first (CAS 005)
 *   casim integrate "<text>" X RAD   an antiderivative in X, without +C (CAS 006)
 *   casim factor "<text>" X RAD      the factors over Q; PARTIAL <text> when one is unproven (CAS 007)
 *   casim defint "<text>" X LO HI RAD  the exact definite integral from LO to HI; IMPROPER on a pole (CAS 009)
 *   casim pdiv "<f>" X "<g>" RAD      the quotient and remainder of f by g over Q, "q; r" (CAS 010)
 *   casim pgcd "<f>" X "<g>" RAD      the gcd of f and g, primitive with a positive leading coefficient (CAS 010)
 *   casim cancel "<n÷d>" X RAD        n÷d reduced by their gcd, then the removed values (CAS 010)
 *   casim partfrac "<n÷d>" X RAD      n÷d in partial fractions over Q, then the removed values (CAS 012)
 *   casim taylor "<text>" X A N RAD   the Taylor polynomial about A to order N, ascending powers of (X-A) (CAS 014)
 *   casim limit "<text>" X A S RAD    the limit as X goes to A (or ∞, -∞) from side S: +, -, or '' for both (CAS 015)
 *   casim sum "<text>" K LO HI RAD    the sum for K = LO..HI (HI may be ∞), a closed form or exact (CAS 018)
 *   casim steps [integrate|solve|partfrac] "<text>" X RAD  derive's (or integrate's, solve's, partfrac's) steps, one
 *                                    a line, each
 *                                    indented by its depth: RULE: d/dX(u) = ... (work/steps-derive.md,
 *                                    steps-integrate.md, steps-solve.md);
 *                                    --one-line: the results only (the answer still last); --tsv: k, depth, T or R, the
 *                                    rule and the text, tab-separated
 *   --arena N                        the arena in bytes (default 8800, the calculator's: Michael, cas-arena-8800)
 *   --min-arena                      also print the smallest arena that gives the same result
 *   --decimal                        every number by CAS 002's rule, the calculator's →DEC (CAS 019 rule 6); an
 *                                    answer printed so because its fractions didn't fit, holding a decimal, is
 *                                    "DECIMAL_FORM" on a line of its own, then the answer
 *   --complex                        solve's complex roots too, a conjugate pair for each quadratic factor with no
 *                                    real root, (A±B×SQRT(r)×0i1)÷D (CAS 020); not in model 33
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
       OP_SUBST = CASIM_SUBST, OP_SOLVE = CASIM_SOLVE, OP_INTEGRATE = CASIM_INTEGRATE, OP_FACTOR = CASIM_FACTOR,
       OP_DEFINT = CASIM_DEFINT, OP_PDIV = CASIM_PDIV, OP_PGCD = CASIM_PGCD, OP_CANCEL = CASIM_CANCEL,
       OP_PARTFRAC = CASIM_PARTFRAC, OP_TAYLOR = CASIM_TAYLOR, OP_LIMIT = CASIM_LIMIT,
       OP_SUM = CASIM_SUM };

static int model = CASIM_MODEL_35, repl_model = CASIM_MODEL_35, print_dec = 0, complex_roots = 0;
static size_t out_cap = OUT;

/* The angle with the options' mode bits (→DEC, CAS 020's complex roots) */
static int mode_bits(int angle)
{
    return angle < 0 ? angle : angle | (print_dec ? CASIM_PRINT_DECIMAL : 0) | (complex_roots ? CASIM_COMPLEX : 0);
}

/* The calculator's own call (casim_run), with the texts' models. */
static const char *repl2;

static casim_status run(int op, const char *text, int var, const char *repl, int angle, char *out, size_t arena_cap)
{
    casim_req rq = {(casim_op)op, text, model, var, repl, repl_model, mode_bits(angle), repl2};
    return casim_run(&rq, out, out_cap, arena, arena_cap);
}

/* every step of derive's (integrate's, solve's, partfrac's) working, through casim_step page by page, as a student would
   walk it; partfrac's PARTIAL on every page is printed once, after them */
static casim_status steps(int op, const char *text, int var, int angle, size_t arena_cap, int flags, int tsv)
{
    casim_req rq = {(casim_op)op, text, model, var, NULL, 0, mode_bits(angle), NULL};
    casim_step_info info;
    char out[OUT];
    casim_status st = casim_step(&rq, -1, flags, &info, out, out_cap, arena, arena_cap);
    for (int k = 0; (st == CASIM_OK || st == CASIM_PARTIAL) && k < info.count; k++) {
        st = casim_step(&rq, k, flags, &info, out, out_cap, arena, arena_cap);
        if (st != CASIM_OK && st != CASIM_PARTIAL) break;
        if (tsv) printf("%d\t%d\t%c\t%s\t%s\n", k, info.depth, info.line == CASIM_STEP_TEMPLATE ? 'T' : 'R',
                        casim_rule_name(info.rule), out);
        else printf("%*s%s: %s\n", 2 * info.depth, "", casim_rule_name(info.rule), out);
    }
    if (st != CASIM_OK) printf("%s\n", st == CASIM_NO_ROOM ? "NO_ROOM" : st == CASIM_TOO_LONG ? "TOO_LONG"
                              : st == CASIM_SYNTAX ? "SYNTAX" : st == CASIM_NOT_DIFFERENTIABLE ? "NOT_DIFFERENTIABLE"
                              : st == CASIM_NOT_INTEGRABLE ? "NOT_INTEGRABLE" : st == CASIM_NOT_POLYNOMIAL ? "NOT_POLYNOMIAL"
                              : st == CASIM_NOT_NUMERIC ? "NOT_NUMERIC" : st == CASIM_PARTIAL ? "PARTIAL"
                              : st == CASIM_UNDEFINED ? "UNDEFINED" : st == CASIM_UNDECIDED ? "UNDECIDED" : "BAD_ARG");
    return st;
}

static int model_arg(const char *a)
{
    return strcmp(a, "STU") == 0 ? CASIM_MODEL_STU : strcmp(a, "33") == 0 ? CASIM_MODEL_33
         : strcmp(a, "35") == 0 ? CASIM_MODEL_35 : -1;
}

int main(int argc, char **argv)
{
    size_t cap = 8800;
    int min_arena = 0, op, var = 0, angle = CASIM_RAD, i = 1, args, one_line = 0, tsv = 0;
    const char *repl = NULL;
    char out[OUT];
    casim_status st;
    for (; i < argc && strncmp(argv[i], "--", 2) == 0; i++) {
        if (strcmp(argv[i], "--min-arena") == 0) min_arena = 1;
        else if (strcmp(argv[i], "--one-line") == 0) one_line = 1;
        else if (strcmp(argv[i], "--decimal") == 0) print_dec = 1;
        else if (strcmp(argv[i], "--complex") == 0) complex_roots = 1;
        else if (strcmp(argv[i], "--tsv") == 0) tsv = 1;
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
    if (strcmp(argv[i], "steps") == 0) {
        const char *a = argv[argc - 1];
        int sop = CASIM_DERIVE;
        if (i + 1 < argc && (strcmp(argv[i + 1], "integrate") == 0 || strcmp(argv[i + 1], "derive") == 0 ||
                           strcmp(argv[i + 1], "solve") == 0 || strcmp(argv[i + 1], "partfrac") == 0))
            sop = argv[i + 1][0] == 'i' ? CASIM_INTEGRATE : argv[i + 1][0] == 's' ? CASIM_SOLVE
                : argv[i + 1][0] == 'p' ? CASIM_PARTFRAC : CASIM_DERIVE, i++;
        if (argc - i != 4 || strlen(argv[i + 2]) != 1) goto usage;
        angle = strcmp(a, "DEG") == 0 ? CASIM_DEG : strcmp(a, "RAD") == 0 ? CASIM_RAD
              : strcmp(a, "GRAD") == 0 ? CASIM_GRAD : -1;
        return steps(sop, argv[i + 1], argv[i + 2][0] - 'A', angle, cap, one_line ? CASIM_STEPS_ONE_LINE : 0, tsv) == CASIM_OK ? 0 : 1;
    }
    op = strcmp(argv[i], "derive") == 0 ? OP_DERIVE : strcmp(argv[i], "simplify") == 0 ? OP_SIMPLIFY
       : strcmp(argv[i], "expand") == 0 ? OP_EXPAND : strcmp(argv[i], "collect") == 0 ? OP_COLLECT
       : strcmp(argv[i], "subst") == 0 ? OP_SUBST : strcmp(argv[i], "solve") == 0 ? OP_SOLVE
       : strcmp(argv[i], "integrate") == 0 ? OP_INTEGRATE : strcmp(argv[i], "factor") == 0 ? OP_FACTOR
       : strcmp(argv[i], "defint") == 0 ? OP_DEFINT : strcmp(argv[i], "pdiv") == 0 ? OP_PDIV
       : strcmp(argv[i], "pgcd") == 0 ? OP_PGCD : strcmp(argv[i], "cancel") == 0 ? OP_CANCEL
       : strcmp(argv[i], "partfrac") == 0 ? OP_PARTFRAC : strcmp(argv[i], "taylor") == 0 ? OP_TAYLOR
       : strcmp(argv[i], "limit") == 0 ? OP_LIMIT : strcmp(argv[i], "sum") == 0 ? OP_SUM : -1;
    args = op == OP_DERIVE || op == OP_COLLECT || op == OP_SOLVE || op == OP_INTEGRATE || op == OP_FACTOR || op == OP_CANCEL ||
           op == OP_PARTFRAC ? 4
         : op == OP_SUBST || op == OP_PDIV || op == OP_PGCD ? 5 : op == OP_DEFINT || op == OP_TAYLOR || op == OP_LIMIT || op == OP_SUM ? 6
         : 3;
    if (op < 0 || argc - i != args) goto usage;
    if (op == OP_DERIVE || op == OP_COLLECT || op == OP_SUBST || op == OP_SOLVE || op == OP_INTEGRATE || op == OP_FACTOR ||
        op == OP_DEFINT || op == OP_PDIV || op == OP_PGCD || op == OP_CANCEL || op == OP_PARTFRAC || op == OP_TAYLOR ||
        op == OP_LIMIT || op == OP_SUM) {
        if (strlen(argv[i + 2]) != 1) goto usage;
        var = argv[i + 2][0] - 'A';
    }
    if (op == OP_SUBST || op == OP_PDIV || op == OP_PGCD) repl = argv[i + 3];
    if (op == OP_DEFINT || op == OP_SUM) repl = argv[i + 3], repl2 = argv[i + 4];
    if (op == OP_TAYLOR) repl = argv[i + 3], repl2 = argv[i + 4];      /* taylor "<text>" X <point> <n>: n as text */
    if (op == OP_LIMIT) repl = argv[i + 3], repl2 = argv[i + 4];       /* limit "<text>" X <point> <side>, verbatim */
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
    case CASIM_NOT_INTEGRABLE: printf("NOT_INTEGRABLE\n"); break;
    case CASIM_PARTIAL: printf("PARTIAL %s\n", out); break;
    case CASIM_UNDEFINED: printf("UNDEFINED\n"); break;
    case CASIM_IMPROPER: printf("IMPROPER\n"); break;
    case CASIM_UNDECIDED: printf("UNDECIDED\n"); break;
    case CASIM_NO_CLOSED_FORM: printf("NO_CLOSED_FORM\n"); break;
    case CASIM_DECIMAL_FORM: printf("DECIMAL_FORM\n%s\n", out); break;
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
                    "       casim [--arena N] [--min-arena] integrate \"<text>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] factor \"<text>\" X RAD|DEG|GRAD\n"

                    "       casim [--arena N] [--min-arena] defint \"<text>\" X LO HI RAD|DEG|GRAD\n"


                    "       casim [--arena N] [--min-arena] pdiv|pgcd \"<f>\" X \"<g>\" RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--one-line] [--tsv] steps [integrate|solve|partfrac] \"<text>\" X RAD|DEG|GRAD\n"


                    "       casim [--arena N] [--min-arena] cancel \"<n÷d>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] partfrac \"<n÷d>\" X RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] taylor \"<text>\" X POINT N RAD|DEG|GRAD\n"
                    "       casim [--arena N] [--min-arena] limit \"<text>\" X POINT +|-|'' RAD|DEG|GRAD (POINT may be ∞, -∞)\n"
                    "       casim [--arena N] [--min-arena] sum \"<text>\" K LO HI RAD|DEG|GRAD (HI may be ∞)\n"
                    "       (every op takes --model, --cap N, --decimal and --complex; subst also --repl-model; defaults 35, 65536)\n");
    return 2;
}
