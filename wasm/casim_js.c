/* The WebAssembly module's C side (wasm/casim.mjs is the JavaScript one): casim_run with the module's own arena and
 * output buffer, so JavaScript passes only texts and numbers. One call at a time, as JavaScript makes them. */
#include <emscripten/emscripten.h>

#include "casim.h"

#define ARENA_MAX 524288        /* casim uses at most 524,280 bytes of an arena */
#define OUT_MAX 65536

static unsigned char arena[ARENA_MAX] __attribute__((aligned(8)));
static char out[OUT_MAX];

EMSCRIPTEN_KEEPALIVE int casimjs_run(int op, const char *eqn, int model, int var, const char *repl, int repl_model,
                                     int angle, const char *repl2, int arena_cap, int cap)
{
    casim_req rq = {(casim_op)op, eqn, model, var, repl, repl_model, angle, repl2};
    if (arena_cap < 0 || arena_cap > ARENA_MAX || cap < 1 || cap > OUT_MAX) {
        out[0] = '\0';
        return CASIM_BAD_ARG;
    }
    return (int)casim_run(&rq, out, (size_t)cap, arena, (size_t)arena_cap);
}

EMSCRIPTEN_KEEPALIVE const char *casimjs_out(void)
{
    return out;
}

EMSCRIPTEN_KEEPALIVE const char *casimjs_version(void)
{
    return casim_version();
}
