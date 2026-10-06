/* Unit CAS 001's visible examples (work/001-examples.txt): ID | expression | variable | angle |
 * assignments | f | f'. The harness is proved first: the core's value of each expression must be the
 * line's f, so the file is read as abacus meant it. Then casim's derivative, evaluated by the core at
 * the same point, must be f' within a relative 1E-24 (the unit's tolerance). */
#include <stdio.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

static unsigned char arena[16384];

/* The next " | "-separated field of *s, NUL-terminated in place. */
static char *field(char **s)
{
    char *f = *s, *bar = strstr(f, " | ");
    if (bar) {
        *bar = '\0';
        *s = bar + 3;
    } else {
        f[strcspn(f, "\r\n")] = '\0';
        *s = f + strlen(f);
    }
    return f;
}

int main(void)
{
    FILE *fp = fopen("work/001-examples.txt", "r");
    char line[2048];
    int cases = 0;
    ce_init();
    CHECK(fp != NULL, "work/001-examples.txt not found (run from the repo root)");
    if (!fp) return check_done();
    while (fgets(line, sizeof line, fp)) {
        char *s = line, *id, *expr, *var, *ang, *asg, *f, *fp1, out[1024];
        abn got, want;
        int angle;
        casim_status st;
        if (line[0] == '#' || line[0] == '\n') continue;
        id = field(&s), expr = field(&s), var = field(&s), ang = field(&s), asg = field(&s);
        f = field(&s), fp1 = field(&s);
        angle = strcmp(ang, "DEG") == 0 ? CASIM_DEG : strcmp(ang, "GRAD") == 0 ? CASIM_GRAD : CASIM_RAD;
        ce_angle(angle);
        for (char *a = strtok(asg, ";"); a; a = strtok(NULL, ";"))
            if (a[0] >= 'A' && a[0] <= 'Z' && a[1] == '=') ce_set(a[0], a + 2);
        cases++;
        /* the harness: the expression's own value */
        want = ce_num(f);
        CHECK(ce_eval(expr, &got) == ABN_OK && ce_close(&got, &want, -24), "%s harness: %s is %s, the file says %s",
              id, expr, ce_text(&got), f);
        /* the derivative */
        st = casim_derive(expr, var[0] - 'A', angle, out, sizeof out, arena, sizeof arena);
        CHECK(st == CASIM_OK, "%s: d/d%s %s: status %d (%s)", id, var, expr, (int)st, out);
        if (st != CASIM_OK) continue;
        want = ce_num(fp1);
        CHECK(ce_eval(out, &got) == ABN_OK && ce_close(&got, &want, -24), "%s: d/d%s %s = %s is %s, want %s", id, var,
              expr, out, ce_text(&got), fp1);
    }
    fclose(fp);
    CHECK(cases == 12, "%d examples read, the file has 12", cases);
    return check_done();
}
