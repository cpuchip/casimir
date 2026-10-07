/* Unit CAS 003, collect: the shape abacus's checker parses (#4264 point 5, accepted): terms c×X^k in
 * descending k, each k once, c×X for k = 1, c alone for k = 0, X^k alone for c = 1, -X^k for c = -1, a non-sum
 * c's sign folded into the joining -, a sum c in parentheses, negative k as c×X^(-k), then the remainder
 * (terms where X is not a whole power) after the lowest power, joined like a sum. This test parses that
 * shape itself, checks every c is free of X by its value at two X, each visible case's c_k at 1E-24, the
 * whole value, and idempotence. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"
#include "check.h"
#include "coreeval.h"

#define OUT 4096
static unsigned char arena[65536];

typedef struct {
    int n, remainder_at;        /* terms; the first remainder term's index, or n */
    long k[64];
    char c[64][OUT];            /* each power term's coefficient, its sign included */
    bool power[64];
} shape;

/* The core's value of text at X = x (Y and Z as set); false on an error. */
static bool at(const char *text, const char *x, abn *v)
{
    ce_set('X', x);
    return ce_eval(text, v) == ABN_OK;
}

/* A term's power of X: X, ...×X, X^n, ...×X^n, X^(-n), ...×X^(-n). Returns false when it is none of these. */
static bool power_of_x(const char *t, long *k, char *c)
{
    size_t n = strlen(t);
    const char *x = NULL;
    for (const char *p = t; *p; p++)
        if (*p == 'X' && (p == t || strncmp(p - 2, "×", 2) == 0)) x = p;
    if (!x) return false;
    if (strcmp(x, "X") == 0) *k = 1;
    else if (x[1] == '^' && x[2] == '(' && x[3] == '-') {
        char *e;
        *k = -strtol(x + 4, &e, 10);
        if (strcmp(e, ")") != 0) return false;
    } else if (x[1] == '^') {
        char *e;
        *k = strtol(x + 2, &e, 10);
        if (*e) return false;
    } else return false;
    if (x == t) strcpy(c, "1");
    else {
        memcpy(c, t, (size_t)(x - 2 - t));
        c[x - 2 - t] = '\0';
    }
    (void)n;
    return true;
}

/* The output's top-level terms, each with its sign, sorted into power terms and the remainder. */
static void parse(const char *s, shape *sh)
{
    int depth = 0, start = 0, sign = 1;
    size_t n = strlen(s);
    memset(sh, 0, sizeof *sh);
    sh->remainder_at = -1;
    if (s[0] == '-') sign = -1, start = 1;
    for (size_t i = (size_t)start; i <= n; i++) {
        char ch = s[i];
        bool cut = false;
        if (ch == '(') depth++;
        else if (ch == ')') depth--;
        else if (ch == '\0') cut = true;
        else if (depth == 0 && (ch == '+' || ch == '-') && i > (size_t)start &&
                 !(s[i - 1] == 'E' && i >= 2 && (s[i - 2] >= '0' && s[i - 2] <= '9')))
            cut = true;
        if (cut && sh->n < 64) {
            char t[OUT], c[OUT];
            long k;
            int m = sh->n;
            memcpy(t, s + start, i - (size_t)start);
            t[i - (size_t)start] = '\0';
            if (power_of_x(t, &k, c)) {
                sh->power[m] = true;
                sh->k[m] = k;
                if (sign < 0) {
                    if (snprintf(sh->c[m], OUT, "-(%s)", c) >= OUT) sh->c[m][0] = '\0';
                } else strcpy(sh->c[m], c);
            } else {
                /* free of X: the constant term (k = 0); else the remainder */
                abn a, b;
                bool free_ = at(t, "0.3", &a) && at(t, "1.7", &b) && abn_cmp(&a, &b) == 0;
                sh->power[m] = free_;
                sh->k[m] = 0;
                if (sign < 0) {
                    if (snprintf(sh->c[m], OUT, "-(%s)", t) >= OUT) sh->c[m][0] = '\0';
                } else strcpy(sh->c[m], t);
            }
            if (!sh->power[m] && sh->remainder_at < 0) sh->remainder_at = m;
            sh->n++;
            sign = ch == '-' ? -1 : 1;
            start = (int)i + 1;
        }
    }
    if (sh->remainder_at < 0) sh->remainder_at = sh->n;
}

static bool collect_ok(const char *text, shape *sh, char *out);

/* text collects in X to the agreed shape, with text's value; c free of X; a fixed point. Returns the
   shape for the caller's own checks. */
static bool collect_ok(const char *text, shape *sh, char *out)
{
    char again[OUT];
    abn f, g;
    casim_status st = casim_collect(text, 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena);
    CHECK(st == CASIM_OK, "collect %s: status %d", text, (int)st);
    if (st != CASIM_OK) return false;
    if (strchr(out, '=')) {
        /* L=R: each side has the shape (checked by collecting each alone), and the whole is a fixed point */
        char l[OUT], r[OUT], lo[OUT], ro[OUT];
        shape ls, rs;
        size_t at = (size_t)(strchr(out, '=') - out);
        memcpy(l, out, at);
        l[at] = '\0';
        strcpy(r, out + at + 1);
        CHECK(collect_ok(l, &ls, lo) && strcmp(lo, l) == 0 && collect_ok(r, &rs, ro) && strcmp(ro, r) == 0,
              "collect %s = %s: a side is not in its collected form", text, out);
        *sh = ls;
        CHECK(casim_collect(out, 'X' - 'A', CASIM_RAD, again, OUT, arena, sizeof arena) == CASIM_OK &&
                  strcmp(again, out) == 0, "collect is not a fixed point: %s -> %s -> %s", text, out, again);
        return true;
    }
    parse(out, sh);
    for (int i = 0; i < sh->n; i++) {
        abn a, b;
        if (i >= sh->remainder_at) {
            CHECK(!sh->power[i], "collect %s = %s: a power term after the remainder", text, out);
            continue;
        }
        CHECK(i == 0 || sh->k[i] < sh->k[i - 1], "collect %s = %s: k not descending, each once", text, out);
        CHECK(at(sh->c[i], "0.3", &a) && at(sh->c[i], "1.7", &b) && abn_cmp(&a, &b) == 0,
              "collect %s = %s: c = %s depends on X", text, out, sh->c[i]);
    }
    CHECK(!at(text, "0.9", &f) || (at(out, "0.9", &g) && ce_close(&g, &f, -24)), "collect %s = %s: value", text, out);
    CHECK(casim_collect(out, 'X' - 'A', CASIM_RAD, again, OUT, arena, sizeof arena) == CASIM_OK && strcmp(again, out) == 0,
          "collect is not a fixed point: %s -> %s -> %s", text, out, again);
    return true;
}

static void is(const char *text, const char *want)
{
    shape sh;
    char out[OUT];
    if (collect_ok(text, &sh, out))
        CHECK(strcmp(out, want) == 0, "collect %s: \"%s\", want \"%s\"", text, out, want);
}

static void rules(void)
{
    ce_angle(CASIM_RAD);
    ce_set('Y', "1.3");
    ce_set('Z', "0.45");
    is("(X+1)^2", "X^2+2×X+1");
    is("X×Y+X+Y×X^2", "Y×X^2+(Y+1)×X");
    is("-X^2-3×X", "-X^2-3×X");
    is("(Y+1)×X^2-(Y+1)×X", "(Y+1)×X^2+(-Y-1)×X");
    is("X^(-2)+X+1", "X+1+X^(-2)");
    is("X^2+SIN(X)+X^0.5", "X^2+SIN(X)+SQRT(X)");          /* a remainder, after the powers */
    is("X×SIN(X)+X", "X+X×SIN(X)");
    is("1÷X+X+SIN(X)", "X+X^(-1)+SIN(X)");                 /* the remainder after the lowest power */
    is("Y", "Y");
    is("0", "0");
    is("(X+Y)×(X-Y)", "X^2-Y^2");
    is("X×(Y+1)^2", "(Y+1)^2×X");                          /* c_k simplified, not expanded: only X is */
    is("X=Y×X^2", "X=Y×X^2");                              /* each side */
    {
        /* the review: a merge in the expansion that makes another power of X is expanded again */
        shape sh;
        char out[OUT];
        collect_ok("(Y×SQRT(X+1)+1)^2", &sh, out);
    }
    {
        /* the delta review: a coefficient made primitive re-merges its run (547 in 20,000 were not fixed points) */
        shape sh;
        char out[OUT];
        collect_ok("0.5×X×Y+3E18×X×Y+(1÷3)×X×Y", &sh, out);
        collect_ok("-4E18×X×Y-3E18×X×Y+X", &sh, out);
    }
    {
        char out[OUT];
        CHECK(casim_collect("(X+SIN(Y)+COS(Y)+TAN(Y)+EXP(Y))^5", 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) ==
                  CASIM_TOO_LONG, "a collected form past 191 bytes: \"%.60s\"", out);
    }
}

/* Sums of c×X^k×Y^j with coefficients large, small and fractional: collect is a fixed point, and so is
   simplify (the delta review's fuzz shape, ours). */
static void big_coefficients(void)
{
    static const char *const CO[] = {"3E18", "-4E18", "2E18÷7", "0.5", "1÷3", "-1", "5÷6", "4E18", "-3E18", "7"};
    unsigned long long r = 12345;
    int fails = 0;
    for (int n = 0; n < 2000; n++) {
        char t[OUT], out[OUT], again[OUT];
        int len = 0, terms = 2 + (int)((r >> 33) % 4);
        for (int i = 0; i < terms; i++) {
            r = r * 6364136223846793005ULL + 1442695040888963407ULL;
            len += snprintf(t + len, sizeof t - (size_t)len, "%s(%s)×X^%d×Y^%d", i ? "+" : "",
                            CO[(r >> 33) % 10], (int)((r >> 40) % 3), (int)((r >> 45) % 2) + 1);
        }
        r = r * 6364136223846793005ULL + 1442695040888963407ULL;
        if (casim_collect(t, 'X' - 'A', CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
            (casim_collect(out, 'X' - 'A', CASIM_RAD, again, OUT, arena, sizeof arena) != CASIM_OK ||
             strcmp(again, out) != 0) && fails++ < 3)
            CHECK(false, "collect not a fixed point: %s -> %s -> %s", t, out, again);
        if (casim_simplify(t, CASIM_RAD, out, OUT, arena, sizeof arena) == CASIM_OK &&
            (casim_simplify(out, CASIM_RAD, again, OUT, arena, sizeof arena) != CASIM_OK || strcmp(again, out) != 0) &&
            fails++ < 3)
            CHECK(false, "simplify not a fixed point: %s -> %s -> %s", t, out, again);
    }
    CHECK(fails == 0, "%d of 2000 mixed-coefficient sums not fixed points", fails);
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
        shape sh;
        line[strcspn(line, "\r\n")] = '\0';
        while (k < 9) {
            char *bar = strstr(s, " | ");
            f[k++] = s;
            if (!bar) break;
            *bar = '\0';
            s = bar + 3;
        }
        if (k < 8 || strcmp(f[1], "collect") != 0) continue;
        n++;
        ce_angle(strcmp(f[4], "DEG") == 0 ? CASIM_DEG : CASIM_RAD);
        for (char *as = strtok(f[5], ";"); as; as = strtok(NULL, ";"))
            if (as[0] >= 'A' && as[0] <= 'Z' && as[1] == '=') ce_set(as[0], as + 2);
        {
            /* the case's X, kept: at() moves X for the c checks */
            char xs[64];
            abn want = ce_num(f[6]), got;
            snprintf(xs, sizeof xs, "%s", ce_text(&ce_calc->var['X' - 'A'].re));
            if (!collect_ok(f[2], &sh, out)) continue;
            CHECK(at(out, xs, &got) && ce_close(&got, &want, -24), "%s: %s at its point", f[0], out);
            /* each c_k of the case, and no other k */
            for (char *kc = strtok(f[7], ";"); kc; kc = strtok(NULL, ";")) {
                long kk = strtol(kc, NULL, 10);
                abn cw = ce_num(strchr(kc, '=') + 1), cg;
                bool found = false;
                for (int i = 0; i < sh.remainder_at; i++)
                    if (sh.k[i] == kk) {
                        found = true;
                        CHECK(at(sh.c[i], xs, &cg) && ce_close(&cg, &cw, -24), "%s: c_%ld = %s is %s, want %s", f[0], kk,
                              sh.c[i], ce_text(&cg), ce_text(&cw));
                    }
                CHECK(found, "%s: no term for k = %ld in %s", f[0], kk, out);
            }
        }
    }
    fclose(fp);
    CHECK(n == 12, "%d visible C cases, want 12", n);
}

int main(void)
{
    ce_init();
    rules();
    big_coefficients();
    visible();
    return check_done();
}
