/* casim: a stub until unit CAS 001 lands (its tests are written first). */
#include "casim.h"

casim_status casim_derive(const char *eqn, int var, int angle, char *out, size_t cap, void *arena,
                          size_t arena_cap)
{
    (void)eqn, (void)var, (void)angle, (void)arena, (void)arena_cap;
    if (cap) out[0] = '\0';
    return CASIM_TOO_LONG;
}

casim_status casim_reprint(const char *eqn, char *out, size_t cap, void *arena, size_t arena_cap)
{
    (void)eqn, (void)arena, (void)arena_cap;
    if (cap) out[0] = '\0';
    return CASIM_TOO_LONG;
}
