/* The golden test (CAS 011, D1): regression, not verification. Each line of the corpus is one call to casim_run and
 * the status and text it gave when the corpus was recorded; the library must give the same, byte for byte. It needs
 * only the library (no firmware core), so it runs wherever the library builds: natively, under ctest, under node.
 * What shows an answer right is the library's own checks and the developer tests (README); this shows that nothing
 * moved since the recording.
 *
 *   golden_check CORPUS            every line must match; prints "checks: N failed: F" (scripts/run-tests.sh)
 *   golden_check --record INPUTS   reads lines of the first ten fields and writes each with its status and text
 *
 * A line is tab-separated: op model var angle repl_model arena cap eqn repl repl2 [status out]. op and status are
 * names (derive, OK); in the three texts and out, \\ \t and \n are escapes, and a lone \N is NULL. taylor's repl2 is
 * its order n, as text, as casim_req carries it; limit's is its side ("+", "-", or empty for both); sum's repl and
 * repl2 are its bounds (CAS 018). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "casim.h"

#define LINE 8192
#define OUT 65536
#define ARENA (1 << 19)

static const char *const OPS[] = {"derive", "simplify", "expand", "collect", "subst", "solve", "integrate", "factor",
                                  "defint", "pdiv", "pgcd", "cancel", "partfrac", "taylor", "limit", "sum"};
static const char *const STATUSES[] = {"OK", "SYNTAX", "NOT_DIFFERENTIABLE", "TOO_LONG", "BAD_ARG", "NO_ROOM",
                                       "NOT_POLYNOMIAL", "NOT_NUMERIC", "NOT_INTEGRABLE", "PARTIAL", "UNDEFINED",
                                       "IMPROPER", "UNDECIDED", "NO_CLOSED_FORM"};
#define COUNT(a) ((int)(sizeof a / sizeof a[0]))

static _Alignas(8) unsigned char arena[ARENA];     /* aligned, as tests/golden_log.c logs the arena */
static char out[OUT];

static int find(const char *const *names, int n, const char *s)
{
    for (int i = 0; i < n; i++)
        if (strcmp(names[i], s) == 0) return i;
    return -1;
}

/* s unescaped in place; NULL for a lone \N */
static char *unescape(char *s)
{
    char *w = s;
    if (strcmp(s, "\\N") == 0) return NULL;
    for (char *r = s; *r; r++) {
        if (*r == '\\' && r[1]) {
            r++;
            *w++ = *r == 't' ? '\t' : *r == 'n' ? '\n' : *r;
        } else *w++ = *r;
    }
    *w = '\0';
    return s;
}

static void put_escaped(FILE *f, const char *s)
{
    if (!s) {
        fputs("\\N", f);
        return;
    }
    for (; *s; s++) {
        if (*s == '\\') fputs("\\\\", f);
        else if (*s == '\t') fputs("\\t", f);
        else if (*s == '\n') fputs("\\n", f);
        else fputc(*s, f);
    }
}

int main(int argc, char **argv)
{
    int record = argc == 3 && strcmp(argv[1], "--record") == 0;
    const char *path = argv[argc - 1];
    static char line[LINE], raw[LINE];
    long n = 0, failed = 0, lineno = 0;
    FILE *f;
    if (argc != 2 && !record) {
        fprintf(stderr, "usage: golden_check [--record] FILE\n");
        return 2;
    }
    if (!(f = fopen(path, "r"))) {
        fprintf(stderr, "golden_check: cannot open %s\n", path);
        return 2;
    }
    while (fgets(line, sizeof line, f)) {
        char *field[12] = {0};
        int k = 0, op, st;
        size_t len = strlen(line), arena_cap, cap;
        casim_req rq;
        lineno++;
        if (len && line[len - 1] == '\n') line[--len] = '\0';
        if (!len || line[0] == '#') continue;
        memcpy(raw, line, len + 1);
        for (char *p = line; k < 12; k++) {
            field[k] = p;
            p = strchr(p, '\t');
            if (!p) {
                k++;
                break;
            }
            *p++ = '\0';
        }
        if (k != (record ? 10 : 12) || (op = find(OPS, COUNT(OPS), field[0])) < 0) {
            fprintf(stderr, "golden_check: %s:%ld: malformed line\n", path, lineno);
            return 2;
        }
        arena_cap = (size_t)strtoul(field[5], NULL, 10);
        cap = (size_t)strtoul(field[6], NULL, 10);
        if (arena_cap > ARENA || cap > OUT) {
            fprintf(stderr, "golden_check: %s:%ld: arena or cap too large\n", path, lineno);
            return 2;
        }
        rq = (casim_req){(casim_op)op, unescape(field[7]), atoi(field[1]), atoi(field[2]), unescape(field[8]),
                         atoi(field[4]), atoi(field[3]), unescape(field[9])};
        memset(out, 0x5a, cap ? cap : 1);       /* stale bytes would show if a status left out unwritten */
        st = (int)casim_run(&rq, out, cap, arena, arena_cap);
        if (cap == 0) out[0] = '\0';
        if (record) {
            fputs(raw, stdout);
            printf("\t%s\t", st >= 0 && st < COUNT(STATUSES) ? STATUSES[st] : "?");
            put_escaped(stdout, out);
            putchar('\n');
            continue;
        }
        n++;
        {
            char *want = unescape(field[11]);
            int want_st = find(STATUSES, COUNT(STATUSES), field[10]);
            if (st != want_st || !want || strcmp(out, want) != 0) {
                failed++;
                if (failed <= 10) {
                    fprintf(stderr, "FAIL %s:%ld: %s ", path, lineno, field[0]);
                    put_escaped(stderr, rq.eqn);
                    fprintf(stderr, "\n  want %s ", field[10]);
                    put_escaped(stderr, want);
                    fprintf(stderr, "\n  got  %s ", st >= 0 && st < COUNT(STATUSES) ? STATUSES[st] : "?");
                    put_escaped(stderr, out);
                    fputc('\n', stderr);
                }
            }
        }
    }
    fclose(f);
    if (record) return 0;
#ifdef CASIM_VERSION                    /* --record runs on the source the corpus is recorded from, older than the call */
    printf("golden: %s, Casimir %s\n", path, casim_version());
#endif
    printf("checks: %ld failed: %ld\n", n, failed);
    return failed ? 1 : 0;
}
