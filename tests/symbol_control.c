/* The symbol check's planted control (scripts/symbol-check.sh): it calls libm's pow and the core's abn_pow, both of
 * which the check must report. Not a test binary and never linked. */
#include <math.h>

#include "abn.h"

double symbol_control(double x, const abn *a, const abn *b);

double symbol_control(double x, const abn *a, const abn *b)
{
    abn r;
    abn_pow(&r, a, b);
    return pow(x, 1.0 / (x + 1));
}
