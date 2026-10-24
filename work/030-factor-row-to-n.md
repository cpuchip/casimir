# CAS work unit 030: a function factor's row to N, as a letter's

**From:** casim · **To:** abacus · **Status:** draft, for a ruling on its rule. Nothing is built. The numbers come from a
throwaway prototype (p1, over 9a390b0, whose src is 33b265d's, CAS 029 as accepted) and a probe built for this unit.
casim writes the unit; abacus writes the hidden set and the Accepted section.
- **From:** the 24, named in 1.0.1's, 1.1.0's and 1.1.1's Not yet, first in abacus's order after 029 (#10275). The
  framing went to abacus at #10358, with three options: (a) keep them named, (b′) the factor's row to N, giving way at
  the cap, and (c) a form that ties the factor's power to N. **Michael ruled (b′)** (2026-10-24, in casim's session:
  "Let us do b′"). It changes CAS 026's rule (decision 90 (3)), which skips the factor's row where the answer has no
  value at the factor's zero.
- **The question:** to N, where a sum's function factor is 0 away from its letter's 0 and the answer has no value
  there, does the answer exclude the point by the factor's row, as a letter's answer does?
- **The oracle:** the sum by its terms at the factor's exact zeros, on a grid of the other letter and of N from lo−1
  (probe-nv's judgment, CAS 027). No point may be left where the sum has a value, no row excludes it, and the answer
  has none. probe-f's two judgments must stay at 0: no answer that claims a value where the sum has none, and no
  factor row for nothing (the sum has a value at every zero, every grid point and every N).

## Where it is today (33b265d, 1.1.1's src)

K from 1 to N, at 65,536 B and 16,384 B alike:

| summand | 33b265d |
|---|---|
| `(Y÷SIN(Z))^K` | `-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; Y÷SIN(Z)≠1` |
| `(Y÷X)^K` (its twin) | `Y÷(X-Y)+(Y÷X)^(N+1)÷(Y÷X-1); X≠0; Y÷X≠1` |

At Z = π and N = 0 the sum is empty, 0, while the answer has no value: 1÷SIN(π) is in it. No row excludes the point.
The twin's `X≠0` excludes X = 0 at N = 0 too, a row for nothing there (the sum is 0) and the row the sum needs for
every N ≥ 1. The difference is in two rules that are each other's reverse:
- **sm_rows, a letter of the ratio:** `L≠0` where the answer has no value at 0, "though the sum may" (CAS 018's cold
  review). The answer then never claims a value it doesn't have, and the empty sum is excluded, not missing.
- **sm_rowf, a factor (CAS 026, decision 90):** the factor's row where a term from lo divides by it, "unless the answer
  itself has no value there". Where it has none, the row is skipped, and the empty sum is missing.

CAS 027's probe-nv finds 24 of the 640 function-factor sums (476 judged) with a missing value at 33b265d, the same 24
as at 1.0.1. All are to N, at SIN's and TAN's π, COS's π÷2 or LN's 1, at the empty sum or at an N before the first term
that divides.

## The change (p1)

**To N (hi neither ∞ nor whole), where the answer has no value at a factor's zero, the factor's row is kept when the
factor is SIN, COS, TAN, LN, LOG, ACOS or ACOSH of a letter. The row gives way at the cap:** where the line with it
would pass 191 B, the line is printed without it, as CAS 028's note gives way (#10148). Every other case is as at
33b265d.

```c
        if (c->st == CASIM_UNDEFINED) {
            c->st = CASIM_OK;
            /* the answer has no value there already, but to N the sum has one at an N before the first term that
               divides (the empty sum; a whole hi has none). Where the factor is 0 away from its letter's 0, the row is
               kept, as the letters' (CAS 030), one that gives way at the cap ('?', sm_rput): SIN, COS and TAN of a
               letter (π, π÷2), LN, LOG, ACOS and ACOSH (1). Of anything else the real zeros aren't known here (Z^2+1
               has none) */
            if (S->inf || S->ihi || x->kind != K_FN || nd(c, x->a)->kind != K_VAR ||
                !((1u << x->id) & (1u << F_SIN | 1u << F_COS | 1u << F_TAN | 1u << F_LN | 1u << F_LOG |
                                   1u << F_ACOS | 1u << F_ACOSH))) {
                compact(c, mark, sum);
                continue;
            }
            op = '?';
        }
        if (c->st == CASIM_OK) compact(c, mark, sum);
        sm_row(c, rows, n, h, op, small(c, C_0), x->kind == K_VAR ? x->id : 26);
```

The giving way is a row op, `'?'`, which a new function reads when sm_print puts the rows. It replaces sm_print's call
to rows_put:

```c
/* rows_put for a sum's rows, each '?' row a ≠ row that gives way (CAS 030): where the line with them passes the cap,
   the rows are put again without them, as CAS 028's note gives way. Out of line, so do_sum_'s frame doesn't move */
static __attribute__((noinline)) void sm_rput(ctx *c, out_t *w, smrow *rows, int n)
{
    size_t at = w->n;
    bool full = w->full;
    unsigned opt = 0;
    int k = 0;
    for (int i = 0; i < n; i++)
        if (rows[i].op == '?') rows[i].op = '!', opt |= 1u << i;
    rows_put(c, w, rows, n, true);
    if (full || !w->full || !opt || c->st != CASIM_OK) return;
    for (int i = 0; i < n; i++)
        if (!(opt >> i & 1u)) rows[k++] = rows[i];
    w->n = at, w->full = false, w->o[at] = '\0';
    rows_put(c, w, rows, k, true);
}
```

sm_print's `rows` loses its const. The '?' can't meet a '!' row with the same text: one sum's probe gives every group
the same answer at a factor's zero. Patch: ~/scratch/casim/c31/p030.patch on lepton (sha256 218de0a6…ce8a). Its
work/027-examples.txt and scripts/mutants.py hunks are below.

| summand, K from lo to hi | 33b265d | p1 |
|---|---|---|
| `(Y÷SIN(Z))^K`, 1 to N | `…; Z≠0; Y÷SIN(Z)≠1` | `…; Z≠0; SIN(Z)≠0; Y÷SIN(Z)≠1` |
| `K×COS(Z)^(K-1)`, 0 to N | `…; Z≠0; COS(Z)≠1` | `…; Z≠0; COS(Z)≠0; COS(Z)≠1` |
| `TAN(Z)^(2×K-1)×Y^K`, 0 to N | `…; Z≠0; Y×TAN(Z)^2≠1` | `…; Z≠0; TAN(Z)≠0; Y×TAN(Z)^2≠1` |
| `LN(Z)^(3×K-1)×Y^K`, 0 to N | `…; Z≠0; Y×LN(Z)^3≠1` | `…; Z≠0; LN(Z)≠0; Y×LN(Z)^3≠1` |
| `(Y÷ACOS(Z))^K`, 0 to N | `…; Y÷ACOS(Z)≠1` | `…; ACOS(Z)≠0; Y÷ACOS(Z)≠1` |
| `K×SIN(Z)^(K-1)`, -1 to N | 184 B, `…; Z≠0; SIN(Z)≠1` | the same: with the row it is 196 B, so the row gives way |

Each new row is the row the sum needs from the first N whose terms divide (the sum has no value at the zero there),
and a row for nothing at a smaller N, where it excludes the sum's value. That is what a letter's row does (`X≠0`
above): the point is excluded, not missing.

### What it leaves, and why

- **A factor whose only zero is its letter's 0:** ASIN, ATAN, SINH, TANH, ASINH and ATANH of a letter, as roots and ABS
  already are (CAS 026's probe at the letter's 0). Their answers carry the letter's `Z≠0` where the answer has no
  value at 0, so a factor row there is redundant. The first p1, with only roots and ABS left out, gave such a row to
  116 sums of the first family (15 factors; 29 each for ASIN, ATAN, SINH and TANH, at both arenas). None of them
  fixed a missing value.
- **Any other factor:** a polynomial (`Y-1`, `Z^2-1`) or a function of something other than a letter (`SIN(2×Z)`).
  casim doesn't know its real zeros here, and the rule taken wider prints a row for nothing: `(Y÷(Z^2+1))^K` from 1
  gained `Z^2+1≠0`, and Z^2+1 is never 0. Their missing values stay. `X^K÷(Y-1)^K` from 1 to N has no value at Y = 1 and
  N = 0, where the sum is 0. It is CAS 029's ÷ spelling's inherited row, and work/029-examples.txt:38 pins its line
  (the first p1 moved it, with `Y-1≠0`).
- **To ∞:** the 12 (Not yet's "to ∞ from a negative lo, where the term at lo is 0") are their own class: no N is
  before the first term. The raw rule at #10358 gave 24 SIN, COS, TAN and LN rows that weren't needed, most of them to
  ∞.
- **A whole hi:** no empty sum and no smaller N, so the row would exclude nothing. The first p1 gave
  `(Y÷SIN(Z))^K` from 1 to 100 a `SIN(Z)≠0`, which the `S->ihi` test now refuses.
- **At the cap:** a line within 12 B of 191 (`; SIN(Z)≠0` is 12 B) keeps its missing value: 4 of the 24, 13 in the
  family.

## The probe

c31/probe030.py on lepton judges two CLIs, 33b265d's and p1's, at 65,536 B and 16,384 B. It uses probe-f's and
probe-nv's three judgments, at each zero of the factor, with the factor put to 0 exactly in the text, the letter Z to
that zero elsewhere, and Y at 1÷3 and −1÷4:
- **missing:** the sum has no value, and the answer claims one;
- **novalue:** the sum has a value, no row with a value excludes the point, and the answer has none;
- **nothing:** a factor row where the sum has a value at every zero, every grid point and every N.

The zeros: SIN and TAN at 0 and π, COS at π÷2, LN, LOG, ACOS and ACOSH at 1, the roots, ABS, ASIN, ATAN, SINH, TANH,
ASINH and ATANH at 0, SIN(2×Z) at 0 and π÷2, Z-1 at 1 and Z^2-1 at ±1. EXP, COSH and Z^2+1 have none. A polynomial
factor is judged with Z put to its zero exactly, and its text is left as printed.

**The family** (work/030-family.txt, 2,640 sums): 22 factors (SIN, COS, TAN, LN, LOG, ASIN, ACOS, ATAN, SINH, TANH,
ACOSH, ASINH, ATANH, SQRT and Z^(1÷3) of Z, ABS(Z), EXP(Z), COSH(Z), SIN(2×Z), Z-1, Z^2+1 and Z^2-1). Each takes 12
shapes: the 24's six (`K×F^(K-1)`, `(F×Y)^K`, `(Y÷F)^K`, `(K-1)×(F×Y)^K÷F`, `F^(2×K-1)×Y^K`, `F^(3×K-1)×Y^K`), the
640's `K×(F×Y)^K÷F`, the 12's `(K+1)×F^(K+1)×Y^K`, the quotients `Y^K÷F^K`, `1÷F^K` and `(2×Y÷F)^K`, and `K×Y^K÷F`.
Each is taken at lo −2 to 2, to N and to ∞.

**The control:** on the 640, probe030 finds 33b265d's 24 at both arenas (48 calls), the count probe-nv gives, and p1's
4. The two judgments that must stay at 0 are 0 on both CLIs, on the 640 and on the family.

**Counts:** the same at both arenas.

| | the 640 | the family |
|---|---|---|
| sums with a missing value, 33b265d | 24 | 353 |
| sums with a missing value, p1 | 4 | 148 |
| sums moved | 20 | 205 |
| moves that are one row, the factor's own, added | 20 | 205 |
| moves that fix a missing value | 20 | 205 |
| row missing; factor row for nothing (either CLI) | 0; 0 | 0; 0 |

The 205 are SIN (29), COS (29), TAN (29), LN (32), LOG (29), ACOS (29) and ACOSH (28), all to N. Nothing moves for the
other 15 factors, or to ∞. The family's summands at a whole hi, 5 and 70, are a third check: 2,640 sums at both
arenas, 0 calls moved.

**The 148 left in the family:**
- 59 are `K×Y^K÷F` (below);
- 40 are polynomial factors;
- 28 are SIN(2×Z);
- 13 are at the cap;
- 8 are to ∞, all `(K+1)×F^(K+1)×Y^K` from −1, the 12's shape.

## Found beside it: a factor outside the ratio, and the empty sum

`K×Y^K÷SIN(Z)` from 1 to N prints `…; Y≠1`. SIN(Z) isn't in the ratio, so neither sm_rowf nor sm_rows sees it. At Z = π
every term has no value, and so has the answer, but at N = 0 the sum is empty and 0. The same holds with a letter:
- `Y^K÷X` from 1 to N is `Y÷(X-X×Y)-Y^(N+1)÷(X-X×Y); Y≠1`;
- `1÷X` from 1 to N is `N÷X`, 0÷0 at X = 0 and N = 0.

All are as at 1.1.0. It is the empty-sum item b7156e4 put in Not yet (ruling 3, #10313), at its widest: any factor
free of K with no value at a point. Not yet names it only for a letter at a negative power in the ratio. In the family
it is 59 sums, at every lo and for every factor with a zero, the letter-0 ones included. (b′) doesn't reach it, since
the factor isn't one of the ratio's.

## Moves

- **The golden corpus:** 10,822 calls, 0 moved; →DEC's 10,788, 0; the complex golden, 0.
- **Doc examples:** work/027-examples.txt:52, `(Y÷SIN(Z))^K` from 1 to N, gains `SIN(Z)≠0`. It is the 24's own row,
  which 027 pinned as named. Nothing else of the 881 moves. docs/language.md's sum section, whose factor sentence says
  "unless the answer itself has no value there", gets the (b′) sentence in the build.
- **make check** on p1: green, with two of 026's mutants re-anchored (below). It took 195.95 s and 239,036 KiB beside
  the 1.1.1 mutant run. sum-values's 643 checks pass on both CLIs: it passed at 33b265d with the 24 in place, so it
  can't see them fixed either.

## Cost

- **Host frames:** 425 as recorded (gcc 15.2.0), plus sm_rput at 80. The first form put the giving way in a new
  7-argument function in place of sm_print's call, and do_sum_ moved 816 → 832: the call's seventh argument goes on
  the stack, and sm_print had been inlined. In sm_print's rows call, the 4-argument sm_rput leaves every frame as it
  was.
- **S3 (size-s3, claim #10389):** 422 of 33b265d's 422 frames unchanged, plus sm_rput at 48. Text 243,222 B, +257 on
  33b265d's 242,965.

## Mutants (planned for the build)

- **Re-anchored:** 026's factor026-row-off (sm_row's op is now a variable) and factor026-hole-kept (the skip's
  `compact; continue` moved into the new test). They are the same faults: the row off, and the hole kept for every
  factor (the raw (b) of #10358).
- **New, each with its catching row:**
  - row030-inf-kept: the ∞ test dropped. The 12's shape gains a row, which is for nothing at every point.
  - row030-whole-kept: the whole-hi test dropped. 1 to 100 gains its row.
  - row030-any-fn: any function of a letter. ASIN's redundant row.
  - row030-any-arg: the bare-letter test dropped. SIN(2×Z) gains a row; its zero at π÷2 makes it a needed row, so
    the catch is a pinned line, not a value.
  - row030-giveway-off: '?' as '!'. The cap's 4 go TOO_LONG.
  - row030-giveway-always: the '?' rows always left out. The 20 lose their row.

  The oracle in make check is a factor-zero family in scripts/sum-values.py with exact zeros, as probe030 judges. It
  has seeded and held rows, and its control is 33b265d's CLI, which must fail it.

## Questions for abacus

1. **The list:** SIN, COS, TAN, LN, LOG, ACOS and ACOSH of a letter. ATAN, ASIN, SINH, TANH, ASINH and ATANH are left
   to the letter's row. Lean: as drafted.
2. **The rest stays named:** polynomial factors, a function of anything but a letter, and the 13 at the cap. Lean:
   Not yet, one item each, with `X^K÷(Y-1)^K` and `(Y÷SIN(2×Z))^K` as their examples.
3. **The factor outside the ratio** (`1÷X` → `N÷X`, `K×Y^K÷F`): Lean: Not yet's empty-sum item widened to name it, not
   030. If an answer to N isn't meant to claim the empty sum at all, the 24's empty-sum points and this item go
   together. That is a domain ruling (Michael's?).
4. **h030:** yours. The family file is here as a floor only.
5. **The oracle and the mutants** as above.

## Not in 030

The 12 (to ∞), the constant beside the factor, and COS's redundant row stay on the follow-up list, in that order.
