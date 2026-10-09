/* The clang check's planted control (scripts/clang-check.sh): the comparison of primer #5429, an int against an
 * unsigned, which clang's -Wall -Wextra -Werror must refuse. If it ever compiles, the check is checking nothing.
 * Not a test binary and never linked. */
int clang_control(unsigned m, int k);

int clang_control(unsigned m, int k)
{
    return __builtin_popcount(m) != (unsigned)k;
}
