/* The pinned core is reachable from casim, and the facts about its equation engine that casim
 * builds on still hold at the pin (core/eqn.h; read 2026-10-06 at 5464c0b). */
#include <stdlib.h>
#include <string.h>

#include "calc.h"
#include "eqn.h"

#include "check.h"

static abn_status eval(ab_calc *c, const char *text, ab_val *v, int *errpos)
{
    return eqn_eval(c, text, false, false, v, errpos);
}

int main(void)
{
    ab_calc *c = calloc(1, sizeof *c);
    ab_val v;
    int errpos = -1;
    abn want;
    if (!c) return 2;
    ab_init(c);
    c->var['X' - 'A'].kind = AB_REAL;
    c->var['X' - 'A'].re = abn_from_int(3);

    /* a value: 2×X^2+1 at X = 3 */
    CHECK(eval(c, "2\xc3\x97X^2+1", &v, &errpos) == ABN_OK, "2×X^2+1 did not evaluate");
    want = abn_from_int(19);
    CHECK(v.kind == AB_REAL && abn_cmp(&v.re, &want) == 0, "2×X^2+1 at X=3 is not 19");

    /* an = gives left minus right (p.6-10) */
    CHECK(eval(c, "X^2=4", &v, &errpos) == ABN_OK, "X^2=4 did not evaluate");
    want = abn_from_int(5);
    CHECK(abn_cmp(&v.re, &want) == 0, "X^2=4 at X=3 is not 5");

    /* no implied multiplication: casim must print every × */
    CHECK(eval(c, "2X", &v, &errpos) == ABN_SYNTAX && errpos == 1, "2X parsed (errpos %d)", errpos);

    /* -A^2 is -(A^2): power before unary minus */
    CHECK(eval(c, "-X^2", &v, &errpos) == ABN_OK, "-X^2 did not evaluate");
    want = abn_from_int(-9);
    CHECK(abn_cmp(&v.re, &want) == 0, "-X^2 at X=3 is not -9");

    free(c);
    return check_done();
}
