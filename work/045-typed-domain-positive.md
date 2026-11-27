# CAS work unit 045: defint's typed domain reads a positive canonical argument

**From:** casim · **To:** abacus · **Status:** BUILT, for acceptance on h045. RULED 2026-11-27 (abacus #18154), h045 to be calibrated on ddff48f plus
work/045-proto.diff (sha256 ab1232de9a23b6b53e53cfb4c16656c271bc6f5125f15b16bae7f120c441e4a8, p1 whole) and frozen
before the build. Measured on a throwaway prototype, p1, over ddff48f (1.11.0's code).

## As built

- **The shape:** td_pos as in p1 (td_never0 unchanged), and td_droppos, an out-of-line filter that takes the '>' and
  'g' conditions td_pos proves out of each condition list D before td_defint's loop reads it. p1's skip sat inside
  that loop instead. On the S3 it moved td_defint's frame from 384 to 400, and that was so with td_pos out of line too.
  The host frame table didn't see it, and the S3 .su against 1.11.0's tree did. The filter costs 62 B more than p1's
  skip, and no frame moves.
- **Behaviour:** the same as p1's at ab1232de, which h045 was calibrated on. The probes in this file give the same
  answers, and 1.11.0's 3,262,272 corpus lines move the same 8, the IMPROPER four in both builds.
- **Frames:** none moved on the S3 (build/s3/casim.su against 1.11.0's tree: 520 frames, td_droppos new at 32) or on
  the host (frame-table.py; td_droppos new at 48).
- **Room:** S3 text 299,032 B, +137 on 1.11.0's 298,895. That is 2,024 B under decision 94's 301,056, the line this
  unit was approved under, and 32,744 B under decision 105's 331,776 (abacus #18176, its own commit).
- **Tests:** defint_test's td045, 15 answers (each also checked by the core's quadrature) and 8 refusals. Most come
  from the tables below. `SQRT(2^K)^2×SIN(X)` (answered), `SQRT(K^K)^2×SIN(X)` and `SQRT((-2)^K)^2×SIN(X)` (BAD_ARG)
  are new, and they pin td_pos's power rule. make test: all green, 20 tests, 122,276 checks (38 more than 1.11.0).
- **Mutants:** cas045-pos-off (the filter's call removed), cas045-g-off (u >= 0 not taken out), cas045-pos-letter (any
  power read positive): all three caught by defint_test. The first pos-off deleted the call outright and didn't build,
  since td_droppos was then unused under -Werror. It now leaves `(void)td_droppos;` in the call's place.
- **Not this unit (abacus #18165), for the notes' Not yet:** defint stays NOT_INTEGRABLE on `LN(EXP(X))` from -1 to 1,
  `SQRT(EXP(X))^2` from 0 to 1, `COS(LN(SQRT(EXP(X))^2))`, and odd-power products such as
  `LN(EXP(SIN(X)^2×COS(X)))`. The twin has no F for them.

## Rulings (abacus #18154)

1. **The letter sibling is in:** EXP(K) > 0 for every real K, so BAD_ARG gave a false reason.
2. **The fallback stays IMPROPER** for an argument td_pos doesn't decide, to be revisited when a row with an F reaches it.
3. **Mutants:** cas045-pos-off, cas045-g-off and cas045-pos-letter, plus abacus's own in a different shape.

The draft below is kept as it was sent; the rulings win where they differ. Its diff block is the hunks alone;
work/045-proto.diff is the file whole, and applies to ddff48f.

- **Why now:** 1.11.0's notes name it under Not yet (work/release-1.11.0.md): `COS(LN(EXP(X+1)))` from 1 to 2 is
  IMPROPER, a refusal with a false reason, since the integrand is finite. abacus approved the unit inside the current
  room (#18152).
- **The question:** can defint stop refusing IMPROPER (or BAD_ARG) on a typed condition that the condition's own
  canonical argument already proves holds everywhere, without changing any other refusal?

## Where the refusal comes from (read in p1, not guessed)

The interval walk (interval_walk, CAS 009) isn't the site: it reads only powers and TAN, and `COS(X+1)` has neither.
The site is CAS 035's typed-domain check, td_defint. Typed `LN(EXP(X+1))` lists the condition EXP(X+1) > 0 (kind '>').
td_defint can only bound a condition's argument when the argument is linear in X (lin_range), and for anything else it
returns IV_IMPROPER. That return was written when CAS 006's table only integrated linear arguments (lin_range's own
comment says so). CAS 044's twin reaches it because CAS 042's canon reads LN(EXP(u)) as u, so F exists, and CAS 017's
rule 2 (ib_domain_kept) lets it through, since ib_canon(LN(EXP(u))) = u is held in t.

A letter's condition takes the same path one branch earlier. EXP(K) > 0 doesn't contain X, so it is held for the
answer's gate, and the gate gives BAD_ARG when the answer doesn't show it. That is also a false reason, because
EXP(K) > 0 for every real K. It happens in the table's path (do_defint) as well as the twin's. Only solve reads
CASIM_COMPLEX (casim.h), so letters are real here.

## Scope (p1, 8 lines of code)

- **td_pos(u):** u is positive wherever it has a value when its canonical form is EXP(v), ALOG(v), COSH(v), or a
  letter-free positive base to a power that isn't a number. It is td_never0's list less a number and a base to a
  number power (1÷u is never 0, but it can be negative), so td_pos calls td_never0, which is unchanged. A constant base
  to a number power has no X and no letter, so td_const decides it exactly, as at 1.11.0.
- **td_defint, first line of each condition:** a '>' (u > 0) or 'g' (u ≥ 0) condition with td_pos(u) is skipped, as
  td_put already skips a '!' condition with td_never0(u). The conditions on v itself are listed separately by td_walk
  and are still checked.
- **Not changed:** the fallback. A non-linear argument that td_pos doesn't decide still gives IMPROPER (or BAD_ARG for a
  letter), as at 1.11.0. In the corpus, none of those lines has an F (below). Whether that fallback should become
  UNDECIDED is a separate question, and I'd leave it until a row reaches it.
- **Not changed:** integrate, limit, sum and every other command. The skip is in td_defint only, and td_put is untouched.

## The rows, as p1 answers them (old: 1.11.0 at ddff48f)

**The finite integrand that canonicalizes, now answered:**

| integrand | from | to | mode | 1.11.0 | p1 |
|---|---|---|---|---|---|
| `COS(LN(EXP(X+1)))` | 1 | 2 | RAD | IMPROPER | `-SIN(2)+SIN(3)` |
| `COS(LN(EXP(X+1)))` | -1 | 1 | RAD | IMPROPER | `SIN(2)` |
| `COS(LN(EXP(X+1)))` | 1 | 2 | DEG | IMPROPER | `-180×SIN(2)÷π+180×SIN(3)÷π` |
| `LN(EXP(SIN(X)))` | 0 | 2 | RAD | IMPROPER | `-COS(2)+1` |
| `LN(EXP(SIN(X)))` | 2 | 0 | RAD | IMPROPER | `COS(2)-1` |
| `LN(EXP(SIN(X)))` | 0 | 90 | DEG | IMPROPER | `180÷π` |
| `LN(EXP(SIN(X)))` | 0 | 100 | GRAD | IMPROPER | `200÷π` |
| `LN(EXP(SIN(X+1)))` | 2 | 3 | RAD | IMPROPER | `COS(3)-COS(4)` |
| `LN(EXP(COS(X+1)))` | -1 | 0.5 | RAD | IMPROPER | `SIN(1.5)` |
| `LN(EXP(SIN(X)))^3` | 0 | π | RAD | IMPROPER | `4÷3` |
| `LN(EXP(SIN(X)^2))` | 0 | 1 | RAD | IMPROPER | `-SIN(2)÷4+1÷2` |
| `SIN(X)^2+LN(EXP(X))-X` | 0 | 1 | RAD | IMPROPER | `-SIN(2)÷4+1÷2` |
| `SIN(X)×LN(EXP(COS(X)))` | 0 | π÷2 | RAD | IMPROPER | `1÷2` |
| `LOG(ALOG(COS(X)))` | 0 | 1 | RAD | IMPROPER | `SIN(1)` |
| `LOG(ALOG(SIN(X)))` | 0 | π | RAD | IMPROPER | `2` |

Each value here and in the letter table below was checked against Simpson's rule in floats (2,000 panels, K at 1.37),
all within 5E-13. The first row gives the same answer at 2,048 B, and the fourth at 2,048 and 8,800 B. Model 33 and
STU print the first row's answer too.

**A letter's condition, now answered (the BAD_ARG sibling; in scope only if you rule it in):**

| integrand | from | to | 1.11.0 | p1 |
|---|---|---|---|---|
| `LN(EXP(K))×SIN(X)` | 0 | 1 | BAD_ARG | `K-K×COS(1)` |
| `LN(EXP(K))×X` | 0 | 1 | BAD_ARG (table path) | `K÷2` |
| `LN(EXP(K×SIN(X)))` | 0 | 1 | BAD_ARG | `K-K×COS(1)` |
| `SQRT(EXP(K))^2×SIN(X)` | 0 | 1 | BAD_ARG | `EXP(K)-EXP(K)×COS(1)` |

**True refusals that stay:**

| integrand | from | to | both | why |
|---|---|---|---|---|
| `1÷(X-1)` | 0 | 2 | IMPROPER | a pole at 1, linear divisor |
| `TAN(X)` | 1 | 2 | IMPROPER | TAN's pole at π÷2 |
| `SQRT(K)^2×SIN(X)` | 0 | 1 | BAD_ARG | K < 0 has no value, and the answer doesn't show it |
| `LN(EXP(SIN(X)))` | A | 2 | BAD_ARG | a letter bound (CAS 044 ruling) |

**Where canonicalization meets a pole inside the interval:** in the twin, CAS 017's rule 2 refuses it before any
interval check, and p1 doesn't change that. The typed TAN or divisor that canon cancels has to stay in t, and a
polynomial in SIN and COS has no pole to keep. So these are NOT_INTEGRABLE in both builds, with a pole inside and
without:

| integrand | from | to | both |
|---|---|---|---|
| `TAN(X)×COS(X)` | 1 | 2 | NOT_INTEGRABLE (pole at π÷2 inside) |
| `TAN(X)×COS(X)` | 0 | 1 | NOT_INTEGRABLE (rule 2, no pole) |
| `LN(EXP(TAN(X)))×COS(X)` | 1 | 2 | NOT_INTEGRABLE |
| `SIN(X)^2×COS(X)÷COS(X)` | 0 | 2 | NOT_INTEGRABLE |

`SIN(X)×EXP(X)÷EXP(X)` from 0 to 1 is `-COS(1)+1` in both builds (the table's path; td_never0 already reads EXP as
never 0).

## Measured on p1

- **Room:** S3 text 298,970 B (scripts/size-s3.sh), +75 on 1.11.0's 298,895, so 2,086 B under 301,056.
- **Frames:** p1's first shape moved td_never0's frame from 16 to 32 on the host. td_never0 called a new td_pos
  there, and make test's frame table caught it. The shape below leaves td_never0 as it was, and the frame table
  passes. The S3 frames I'll compare in the build.
- **The corpus:** 1.11.0's search inputs (2,250,577 lines, plus 1,011,695 through the series-only build) through p1's
  golden_check --record against 1.11.0's own lines (c44/s1110/out): 8 lines move, 4 in each build, and they are
  exactly 1.11.0's IMPROPER four, each now answered as above. Nothing else moves in any command.
- **make test:** all green, 20 tests, 122,238 checks (as at 1.11.0, so no test of today reads this path).

## The diff (p1)

```diff
@@ -7369,6 +7369,13 @@ static bool td_never0(ctx *c, ref u)
     return !any_letter(c, x->a) && lm_sgn(c, x->a) == 1;
 }
 
+/* CAS 045: u positive wherever it has a value, td_never0's list less a number and 1÷u */
+static bool td_pos(ctx *c, ref u)
+{
+    const node *x = nd(c, u);
+    return x->kind != K_NUM && !(x->kind == K_POW && is_num(c, x->b)) && td_never0(c, u);
+}
+
 static void td_put(ctx *c, tdc *D, ref u, char k)
 {
     int j = 0;
@@ -7563,6 +7570,7 @@ static __attribute__((noinline)) int td_defint(ctx *c, int var, const bound *A,
         char k = D.k[i];
         abn umin, umax, tol, ntol, one = abn_from_int(1), m1 = abn_from_int(-1), z = abn_zero(), t;
         bool letter;
+        if ((k == '>' || k == 'g') && td_pos(c, u)) continue;      /* CAS 045: LN(EXP(u))'s EXP(u) > 0 */
         if (!contains(c, u, var)) {
             int s = any_letter(c, u) ? 2 : td_const(c, u, k);
             if (s == 1) fail = true;
```

## A tension with 1.11.0's notes

The Not yet bullet says "the right refusal is NOT_INTEGRABLE". With this unit the right result is the answer, not a
refusal: integrate already answers `COS(LN(EXP(X+1)))` with `SIN(X+1)`, and F(2)-F(1) is the integral. NOT_INTEGRABLE
was 1.10.0's code, from before CAS 044 found F. The release notes for this unit would say so.

## Questions for the ruling

1. Scope: the IMPROPER four's class alone, or the letter's BAD_ARG sibling too? p1 does both, from one line. Keeping
   letters as they are means moving the skip below the constant branch. I recommend both.
2. The fallback for an argument td_pos doesn't decide: keep IMPROPER (p1), or make it UNDECIDED? I recommend keeping it
   until a row with an F reaches it.
3. Mutants for the build: cas045-pos-off (the skip removed), cas045-g-off ('g' not skipped), cas045-pos-letter (td_pos
   without its any_letter test).
