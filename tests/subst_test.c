/* Unit CAS 003, subst: every occurrence replaced by the replacement as a whole (as if in parentheses),
 * then simplified; a replacement that holds var; L=R; a replacement that would copy a draw (RAND) refused
 * (#4264 point 2); SYNTAX offsets as if the call were eqn + "|" + repl (point 6); the visible S cases. */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

static casim_status sub(const char *e, const char *r, char *out)
{
    return casim_subst(e, 'X' - 'A', r, CASIM_RAD, out, OUT, arena, sizeof arena);
}

static void is(const char *e, const char *r, const char *want)
{
    char out[OUT];
    casim_status st = sub(e, r, out);
    CHECK(st == CASIM_OK && strcmp(out, want) == 0, "subst %s, X = %s: %d \"%s\", want \"%s\"", e, r, (int)st, out, want);
}

static void status(const char *e, const char *r, casim_status want, const char *detail)
{
    char out[OUT];
    casim_status st = sub(e, r, out);
    CHECK(st == want && strcmp(out, detail) == 0, "subst %s, X = %s: %d \"%s\", want %d \"%s\"", e, r, (int)st, out,
          (int)want, detail);
}

static void rules(void)
{
    is("X^2", "X+1", "(X+1)^2");                /* the classic: the replacement holds var, as a whole */
    is("X+X^2", "Y", "Y+Y^2");                  /* every occurrence */
    is("SIN(X)+COS(X)", "2×Y", "SIN(2×Y)+COS(2×Y)");
    is("X×Y", "Y", "Y^2");                      /* then simplified */
    is("X-1", "X+1", "X");
    is("2×X", "-Y", "-2×Y");                    /* as if in parentheses: 2×(-Y) */
    is("X^2", "-Y", "Y^2");                     /* (-Y)^2, not -Y^2 */
    is("Y", "X+1", "Y");                        /* no X: simplified alone */
    is("X=X^2", "Y+1", "Y+1=(Y+1)^2");          /* each side */
    /* a draw copied: refused; once is fine */
    status("X-X", "RAND", CASIM_NOT_DIFFERENTIABLE, "RAND");
    is("X+1", "RAND", "RAND+1");
    /* SYNTAX: in eqn as usual; in repl at len(eqn) + 1 + its offset ("X+1|2Y": 2Y's error at 1) */
    status("2X", "Y", CASIM_SYNTAX, "1");
    status("X+1", "2Y", CASIM_SYNTAX, "5");
    status("X×π", "(Y", CASIM_SYNTAX, "6");     /* 3 characters, then "|", then (Y's end at 2 */
}

static void visible(void)
{
    FILE *fp = fopen("work/003-examples.txt", "r");
    char line[4096];
    int n = 0;
    CHECK(fp != NULL, "work/003-examples.txt not found");
    if (!fp) return;
    while (fgets(line, sizeof line, fp)) {
        char *f[9], *s = line, out[OUT];
        int k = 0;
        abn got, want;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 9) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 8 || strcmp(f[1], "subst") != 0) continue;
        n++;
        ce_angle(strcmp(f[5], "DEG") == 0 ? CASIM_DEG : CASIM_RAD);
        for (char *as = strtok(f[6], ";"); as; as = strtok(NULL, ";"))
            if (as[0] >= 'A' && as[0] <= 'Z' && as[1] == '=') ce_set(as[0], as + 2);
        want = ce_num(f[7]);
        CHECK(casim_subst(f[2], f[3][0] - 'A', f[4], CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
                  ce_eval(out, &got) == ABN_OK && ce_close(&got, &want, -24),
              "%s: subst %s, %s = %s: %s is %s, want %s", f[0], f[2], f[3], f[4], out, ce_text(&got), f[7]);
    }
    fclose(fp);
    CHECK(n == 12, "%d visible S cases, want 12", n);
}

int main(void)
{
    ce_init();
    rules();
    visible();
    return check_done();
}
