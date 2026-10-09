/* Casimir from C: a derivative, a factorization and a definite integral, each with its status.
 *
 *   cc -std=c11 -Isrc examples/hello.c build/libcasim.a -lm && ./a.out
 */
#include <stdio.h>

#include "casim.h"

static unsigned char arena[8192];       /* all of Casimir's working memory: the caller's, here 8 KiB */

static const char *const STATUS[] = {"OK", "SYNTAX", "NOT_DIFFERENTIABLE", "TOO_LONG", "BAD_ARG", "NO_ROOM",
                                     "NOT_POLYNOMIAL", "NOT_NUMERIC", "NOT_INTEGRABLE", "PARTIAL", "UNDEFINED",
                                     "IMPROPER"};

static void show(const char *what, casim_req rq)
{
    char out[192];                      /* the calculator's equation slot: 191 bytes and the NUL */
    casim_status st = casim_run(&rq, out, sizeof out, arena, sizeof arena);
    printf("%s\n    %s%s%s\n", what, st == CASIM_OK ? "" : STATUS[st], st == CASIM_PARTIAL ? " " : "",
           st == CASIM_OK || st == CASIM_PARTIAL ? out : "");
}

int main(void)
{
    printf("Casimir %s\n", casim_version());
    /* {op, text, model, var (0-25 for A-Z), repl, repl_model, angle, repl2} */
    show("d/dX X^2×SIN(X)", (casim_req){CASIM_DERIVE, "X^2×SIN(X)", CASIM_MODEL_35, 'X' - 'A', NULL, 0, CASIM_RAD, NULL});
    show("factor X^3-6×X^2+11×X-6", (casim_req){CASIM_FACTOR, "X^3-6×X^2+11×X-6", CASIM_MODEL_35, 'X' - 'A', NULL, 0,
                                               CASIM_RAD, NULL});
    show("solve X^2-2=0", (casim_req){CASIM_SOLVE, "X^2-2=0", CASIM_MODEL_35, 'X' - 'A', NULL, 0, CASIM_RAD, NULL});
    show("integral of SIN(X), 0 to π", (casim_req){CASIM_DEFINT, "SIN(X)", CASIM_MODEL_35, 'X' - 'A', "0", 0, CASIM_RAD,
                                                   "π"});
    show("integral of 1÷X, -1 to 1", (casim_req){CASIM_DEFINT, "1÷X", CASIM_MODEL_35, 'X' - 'A', "-1", 0, CASIM_RAD, "1"});
    return 0;
}
