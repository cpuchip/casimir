# CAS work unit 031: the closed form's common factor, cancelled

**From:** casim · **To:** abacus · **Status:** built (As built below), ruled at #10576; h031 frozen at #10576 (sha256
ac8964c5…8ff1, 397 sums at 16,384 B and 65,536 B). ~~Draft, for h031. Nothing is built.~~ Michael ruled the cancel (card
cas-031-cancel, abacus #10524 (3)). The draft's numbers come from a throwaway prototype, p031e, over f59feca. Its src
is 8462e77's, CAS 030 as accepted. The build is p031e's code. casim writes the unit; abacus writes the hidden set and
the Accepted section.
- **From:** the 12, in 1.1.2's Not yet ("to ∞ from a negative lo, where the term at lo is 0"), next in abacus's order
  (#10275). The framing went to abacus at #10515, with (b), a factor row where no term divides, measured. abacus asked
  for (c) (#10516), measured at #10521, and put it on Michael's board as card cas-031-cancel (#10522), with (b) as
  the fallback.
- **The question:** where a term of the closed form is a monomial over a sum that shares it, so the answer is 0÷0
  at a letter's 0 or a factor's zero, does the answer give the sum's value there?
- **The oracle:** the sum by its merged terms at the letters' 0 and the factor's exact zeros (the reading,
  simplify's convention, #9687 (3)). Wherever a moved line claims a value (its first part has one, and no row with a
  value excludes the point), the sum must have one and it must be the same. Every moved line must equal the line
  before it to 45 digits wherever both have values, at generic points of every letter and of N. probe030's three
  judgments (missing, novalue, nothing) must not get worse anywhere.

## Where it is today (8462e77, CAS 030)

| summand, K from lo to hi | 8462e77 |
|---|---|
| `(K+1)×SIN(Z)^(K+1)×Y^K`, -1 to ∞ | `Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1` |
| `(K+1)×X^(K+1)×Y^K` (its twin), -1 to ∞ | `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1` |

At Z = π every term is 0 (at -1 it is 0×SIN(Z)^0), so the sum is 0, while the answer is 0÷0 there. No term divides
by SIN(Z), so sm_rowf skips the factor before its probe, and no row excludes the point. The twin gets `X≠0` from
sm_rows' rule (a letter's row where the answer has no value at its 0), which has no division test. That is a row for
nothing: the sum is 0 at X = 0. Both come from one form. The closed form's term is a monomial n over a sum P that
holds n's letters (or factor) in every term, and nothing takes it out.

## The change (p031e)

**Each term of a sum's closed form that is n×P^(-1), P a sum, loses the monomial n and P share, before the rows.**
For each base b that n holds at a positive whole power, the cancel takes out b^k, k the least of n's power of b and
b's least whole power over P's terms (0 where a term lacks b). The term becomes (n÷b^k)×(P÷b^k)^(-1), with P÷b^k
expanded. Where the cancel runs out of room, the answer is the one before it: the cancel gives way. Every closed form
goes through it, to N and to ∞, from any lo. The rows are made after it, from the new answer, by CAS 026's and
CAS 030's rules unchanged.

```c
/* CAS 031: b's whole power in t (a factor b^k, k a whole number), or 0 */
static long sm_pw(ctx *c, ref t, ref b);

/* CAS 031: a term n×P^(-1), P a sum, loses the monomial n and P share: each base's least whole power over P's
   terms, as far as n holds it */
static ref sm_cancel1(ctx *c, ref t);

/* CAS 031: each term of the answer through sm_cancel1, the sum made once. The new answer is compacted, so the rows
   get the cancel's room back; where the cancel runs out of room, the answer as it was, and the arena as it was
   (taylor_drop's rewind: the constants cached since are dropped) */
static __attribute__((noinline)) ref sm_cancel(ctx *c, ref sum)
{
    size_t mark = c->used;
    int n = nd(c, sum)->kind == K_ADD ? nd(c, sum)->n : 1;
    ref room = grab(c, (size_t)n * sizeof(ref)), r;
    bool moved = false;
    for (int i = 0; i < n && c->st == CASIM_OK; i++) {
        ref u = nd(c, sum)->kind == K_ADD ? list(c, nd(c, sum)->a)[i] : sum, v = sm_cancel1(c, u);
        if (c->st == CASIM_OK) ((ref *)(void *)nd(c, room))[i] = v;
        moved = moved || v != u;
    }
    r = c->st == CASIM_OK && moved ? (n == 1 ? ((ref *)(void *)nd(c, room))[0] : add(c, (ref *)(void *)nd(c, room), n))
                                   : sum;
    if (c->st == CASIM_OK && r != sum) r = compact(c, mark, r);
    if (c->st == CASIM_NO_ROOM) {
        c->st = CASIM_OK;
        r = sum;
    }
    if (r == sum) taylor_drop(c, mark);
    return r;
}
```

It is called in do_sum_ once, after the tail is added and before sm_rows:

```c
    if (c.st == CASIM_OK && !dir && T) sum = ib_canon(&c, add2(&c, sum, T));
    if (c.st == CASIM_OK && !dir) sum = sm_cancel(&c, sum);
    if (c.st == CASIM_OK) sm_rows(&c, &S, G, ng, dir ? small(&c, C_0) : sum, rows, &nrows);
```

The patch is ~/scratch/casim/c31/p031e.patch on lepton (sha256 40d62ce3…49c4). sm_pw and sm_cancel1 are p031c's
(#10521) unchanged. sm_cancel differs from p031c's in the three ways abacus's two items asked for:
- **Its term array is in the arena,** not on the stack: p031c's `ref t[64]` made sm_cancel's frame 352 B on the host
  and 288 on the S3. p031d took the array out alone: 208 B on the host, and 160 and sm_pw 64 on the S3 (size-s3,
  claim #10537). p031e keeps that frame on the host (208); its S3 frame is measured at the build.
- **The new answer is compacted** (compact(c, mark, r)), so the rows get back the room the cancel used. Without it,
  the three refusals #10521 found became NO_ROOM at 16,384 B, where they had been TOO_LONG: the cancel succeeded and
  then the row probes ran out. With it they print TOO_LONG at 16,384 B and 65,536 B, as at 8462e77. At 8,800 B and
  12,288 B they are NO_ROOM, also as at 8462e77:
  - `(K+1)×(Z-1)^(K+1)×Y^K`, -2 to ∞;
  - `(K+1)×(Z^2+1)^(K+1)×Y^K`, -2 to ∞;
  - `(K+1)×(Z^2-1)^(K+1)×Y^K`, -2 to ∞.
- **The give-way rewinds as taylor_drop does.** p031c's `c->used = mark` left c->small and c->pi pointing into the
  freed room wherever sm_cancel1 made one of the cached constants first (small(c, C_1), for one). ir_reset,
  irs_reset and taylor_drop exist for that. It was found reading the code for the compact, not by a check. No case
  is known to reach it.

| summand, K from lo to hi | 8462e77 | p031e |
|---|---|---|
| `(K+1)×SIN(Z)^(K+1)×Y^K`, -1 to ∞ | `Y×SIN(Z)^2÷(…); Y≠0; Z≠0; ABS(Y×SIN(Z))<1` | `-SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Y≠0; ABS(Y×SIN(Z))<1` |
| `(K+1)×X^(K+1)×Y^K`, -1 to ∞ | `X^2×Y÷(…); X≠0; Y≠0; ABS(X×Y)<1` | `-X÷2÷(X×Y-X^2×Y^2÷2-1÷2); Y≠0; ABS(X×Y)<1` |
| `(K-1)×(SQRT(2)×X×Y)^K÷X`, 1 to ∞ | three terms; `X≠0; ABS(X×Y×SQRT(2))<1` | `-X×Y^2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); ABS(X×Y×SQRT(2))<1` |
| `K×(SQRT(5)÷3×Y×SIN(Z))^K÷SIN(Z)`, 1 to ∞ | `…; Z≠0; ABS(…)<1`, no value at Z = π | `-Y×SQRT(5)÷(Y×SIN(Z)×SQRT(5)-3)-5×Y^2×SIN(Z)÷6÷(…); ABS(…)<1` |
| `(1+K)×(SIN(Z)×X×Y)^K`, -1 to ∞ | `…; X≠0; Y≠0; Z≠0; ABS(…)<1` | `-1÷2÷(…); X≠0; Y≠0; SIN(Z)≠0; ABS(…)<1` |
| `K×EXP(Z)^(K-1)`, 1 to N | `TOO_LONG` | a 180 B answer; `Z≠0; EXP(Z)≠1` |

The twin's `X≠0` goes, and `Y≠0` stays: at Y = 0 the term at -1 is 0×X^0×Y^(-1), which has no value. Where a term
from lo divides by the factor, the answer that now has a value there gets the factor's row from sm_rowf (the last
SIN row above: the term at -1 divides by SIN(Z)×X×Y). That is CAS 026's rule, which skipped the row only because the
answer had no value either.

The cancelled denominator prints in canon's spelling: 1÷(1-X)^2 is `-1÷2÷(X-X^2÷2-1÷2)`, as 8462e77 already prints it
in other terms (work/030-examples.txt:60's first term). The cancel doesn't change that.

## What it fixes from 1.1.2's Not yet

- **The 12** (a missing value: to ∞ from a negative lo, where the term at lo is 0), and the grid #10515 built round
  them: 5 shapes × 7 factors (SIN, COS, TAN, LN, LOG, ACOS, ACOSH), to N and to ∞, 70 sums. 35 had a missing value at
  8462e77, all to ∞; at p031e, none.
- **A row for nothing, to ∞ from a negative bound:** the twin's `X≠0`.
- **A row for nothing, past an irrational constant or a factor that is never 0:** `(K-1)×(SQRT(2)×X×Y)^K÷X` and
  `K×(EXP(Z)×X)^K÷X` from 1 to ∞ lose `X≠0`.
- **A missing value, past a constant beside the factor:** `K×(SQRT(5)÷3×Y×F)^K÷F` from 1 and from 2 to ∞ get the
  sum's value at F's zero, for F each of SIN, COS, TAN, LN, LOG, ACOS and ACOSH (14 sums; from 0 and to N nothing
  moves). It was on casim's list of later units.

Not touched: `(X÷Y)^K÷X` from 1 to N's `X≠0`, COS's redundant `Z≠0`, the empty-sum items, the polynomial factor and
SIN(2×Z) items, and the cap item. Each prints as at 8462e77.

## Measured

With p031c's CLI and p031e's. The two agree byte for byte on every set that judged p031c: the 640, the grid and the
family (6,700 calls). They differ only on the three refusals above. Sets judged on p031e itself say so.

- **Values** (c31/cmp031.py; its controls fire, 464 UNEQUAL and 322 wrong claims with a moved first part, and 24
  claims where the sum has none with the rows taken off): 712 points equal and 308 claims judged, 0 unequal and 0
  wrong, on the 640, the grid and the family together.
- **probe030** (missing, novalue, nothing), in calls, each sum at 65,536 B and 16,384 B:

  | | sums | calls moved | novalue before → after | missing; nothing (either CLI) |
  |---|---|---|---|---|
  | the grid | 70 | 70 (35 sums) | 70 → 0 | 0; 0 |
  | the 640 | 640 | 62 (31 sums) | 8 → 8 | 0; 0 |
  | the family (work/030-family.txt) | 2,640 | 73 | 296 → 280 | 0; 0 |
  | SQRT(5)÷3 beside the factor | 42 | 28 (14 sums) | 28 → 0 | 0; 0 |

  The 640's 8 are CAS 030's cap item (4 sums), unchanged. Of the family's 296, the 16 that go are its 8 sums of the
  12's shape; the 280 left are CAS 030's other classes (`K×Y^K÷F`, the polynomial factors, SIN(2×Z), the cap).
- **sum-values** (p031e): 741/0. Its CAS 025 count of named rows for nothing goes 5 → 4, its CAS 026 letter rows 8 → 7,
  and its #10068 shape 1 → 0.
- **The golden** (p031e): 0 of 10,822 moved.
- **The doc rows** (p031e): 12 of 911 moved, all sums: work/024-examples.txt:27, 46 and 57,
  work/025-examples.txt:43, 52, 64, 66 and 69, work/027-examples.txt:37 and 54, and work/030-examples.txt:60 and 61.
  027:54 and 030:61 are the 12's own row.
- **Text:** every moved answer is shorter, by 4 to 52 B. Four sums go from TOO_LONG to an answer at both arenas:
  `K×EXP(Z)^(K-1)` from 1 and from 2 to N, and `(K-1)×(EXP(Z)×Y)^K÷EXP(Z)` and `(K-1)×(COSH(Z)×Y)^K÷COSH(Z)` from 1
  to N.
  No status goes the other way.
- **sum-values's generator draws** (c31/svcases.py, the five seeds, 640 cases at both arenas): 14 of 1,280 calls
  moved.
- **Frames:** host, 426 as recorded, plus sm_cancel 208 and sm_pw 96 (p031e). S3 (p031d, size-s3): 423 as at
  8462e77, plus sm_cancel 160 and sm_pw 64. No existing frame moves. S3 text 244,724 B (p031d), +1,502 over
  8462e77's 243,222.

The full shape list is work/031-moves.txt: each moved sum, before and after, at 65,536 B, and at 16,384 B where that
differs. It has the grid's 35, the 640's 31 (27 of fn-rows-all, 4 of fn-pow-rows) and the 14 with SQRT(5)÷3.

## Asked

Ruled at #10576: (1) as it prints; (2) they stand, under CAS 026's rule (h031 checks that the sum has no value where
they exclude); (3) yes, the old line in a comment; (4) the seven, and cancel-rewind-keeps-cache named as cx-bit-in-33
is if it survives; (5) Michael's, on the board after acceptance.

1. **`ABS(Z)≠0`.** `(1+K)×(ABS(Z)×X×Y)^K` from -1 to ∞ printed `Z≠0` and prints `ABS(Z)≠0`, which excludes the same
   point. It's CAS 026's factor row, which the answer's new value at 0 no longer skips; the letter's row goes because
   the answer has a value at 0. Lean: as it prints. Making it `Z≠0` would mean a rule for one spelling of one factor.
2. **The rows that come in.** Each is the row the sum needs where the answer now has a value and a term divides:
   `SIN(Z)≠0` and `COS(Z)≠0` for `(1+K)×(F×X×Y)^K` from -1 to ∞, and work/030-examples.txt:60's `SIN(Z)≠0` in place
   of `Z≠0` (`K×SIN(Z)^(K-1)` from 0 to 70: the term at 0 divides). Lean: they stand, as CAS 026's rule prints them.
3. **The 12 doc rows** are pinned again as they now print, each with its old line in a comment. Lean: yes.
4. **The checks planned:**
   - work/031-examples.txt: the grid's 35, the 14, the four TOO_LONG lines and the three refusals at 16,384 B;
   - a CAS 031 block in sum-values: the 12's shapes judged at the factor's zero, the planted answers the old lines;
   - scripts/measure.py: bench rows for the cancel;
   - mutants:
     - cancel-off (sm_cancel returns its sum);
     - cancel-numerator-power (k is n's power, not the least);
     - cancel-first-base (only n's first base);
     - cancel-first-term (only the answer's first term);
     - cancel-keeps-no-room (NO_ROOM isn't given way);
     - cancel-no-compact (the compact taken out: the three refusals' row);
     - cancel-rewind-keeps-cache (`c->used = mark` for taylor_drop).

     The last may survive, since no known case reaches it. If it does, I'll name it as cx-bit-in-33 is named, or look
     for a case.
5. **The release.** The cancel rewords 31 of the 640 and 12 doc rows, and answers four sums that were TOO_LONG. By
   1.1.0's reasoning (answers' bytes change) that is a minor, 1.2.0. It's Michael's card, after h031.

Not in this unit: a cancel across two denominators (a term n×P^(-1)×Q^(-1)), and a power of N (`(X×Y)^(N+1)` over a
sum holding X). sm_pw reads only whole powers, so `-N×(X×Y)^(N+1)÷(X^2-X^3×Y)` keeps its X^2 (work/024-examples.txt:27).

## As built

The build is p031e's three functions with their comments written for the unit, and sm_cancel's last steps put as one
compact call; it prints what p031e prints, byte for byte, on the grid, the 640, the 42 with SQRT(5)÷3 and the family
(6,784 calls at both arenas). Beside the code:
- **The 12 doc rows** are pinned as they now print, each under a comment with the line it printed (#10576 (3)).
- **work/031-examples.txt** (62 rows, scripts/doc-examples.py): the grid's 35, the 14 with SQRT(5)÷3, the four lines
  that were TOO_LONG, the three refusals at 16,384 B (TOO_LONG, the compact's row), four rows for #10576 (1) and (2)
  (SIN(Z)≠0, COS(Z)≠0, ABS(Z)≠0, and EXP(Z), which gets none), and two at 4,096 B where the cancel itself runs out of
  room and gives way (`K×EXP(Z)^(K-1)` from 1 to N, which answers at 8,800 B, and its COSH twin: TOO_LONG, as at
  8462e77).
- **sum-values:** judge_letter4 refuses #10068's shape (the 12's: to ∞ from lo < 0, the term at lo 0) where it named
  it ("missing ∞"), and a CAS 031 block judges it: 5 planted answers (1.1.2's lines for SIN in RAD, COS in GRAD, TAN
  and SIN(2×Z) in DEG, LN from -2) refused and the build's taken; 45 held rows (the grid's five shapes for ROW030's
  factors and SIN(2×Z), and the five planted sums); and 60 seeded (gen_letter6, seed 20261031: gen_letter4's sums from
  -3..-1 to ∞ with q 0 at lo, any of the 18 factors, in each angle mode for SIN, COS, TAN and SIN(2×Z)). CAS 027's
  block takes n5 with the build's line. 846 checks, 0 failed; on 8462e77's CLI, 47 failed, every one the 12's shape (the
  45 held and 2 of the seeded). Across the 18 factors × three of the shapes, 8462e77 fails exactly SIN, COS, TAN, LN,
  LOG, ACOS, ACOSH and SIN(2×Z), the build none.
- **docs/language.md:** a sentence after CAS 027's, with the twin's line.
- **scripts/measure.py:** five bench rows (cancel …): SIN to ∞ from -1, COS in GRAD, SQRT(5)÷3 beside SIN, a line
  brought under the cap, and a compacted one.
- **scripts/mutants.py:** the seven of §Asked (4), 463 in all.
- **tests/frames:** sm_cancel 208 and sm_pw 96; the other 426 as recorded.

make check: green, 219 s, 241,084 KiB (971 doc commands, golden 10,822, sum values 846); again on the tree as committed,
with the two give-way rows, green, 136 s, 239,744 KiB (973 doc commands). make test: green, 20 tests, 118,439 checks,
4:50, 239,720 KiB (claim #10578), on the tree before the two rows, which are test rows only.

- **Mutants** (claim #10578, 2 shards, 19:57–20:13Z): the seven, and CAS 010's cancel-zero-numerator, which the shard
  filter's `cancel-` prefix took in too (caught by tests/golden/corpus.txt:7844). Five of the seven were caught at
  once, by the re-pinned doc rows and work/031-examples.txt. Two survived:
  - **cancel-keeps-no-room:** no check reached the give-way. A search of the build against the mutant's CLI (the
    grid, the 640, the 42 and the family, at 3,072 to 12,288 B, 13,568 calls) found it at 4,096 B only, on four sums,
    where the build prints TOO_LONG and the mutant NO_ROOM. Two of them are pinned above (work/031-examples.txt:104 and
    105); rerun alone, it is caught by them (20:34–20:36Z).
  - **cancel-rewind-keeps-cache:** the same search found no input where it differs from the build. It is named, as
    cx-bit-in-33 is (#10576 (4)): the rewind it removes keeps c->small and c->pi out of freed room, and no known case
    makes a cached constant first inside a cancel that then gives way.
- **Frames:** host 428 as recorded (sm_cancel 208, sm_pw 96 new). S3 (size-s3): 423 as at 8462e77, plus sm_cancel 160
  and sm_pw 64; no frame moved. S3 text 244,756 B, +1,534 over 8462e77's 243,222.
- **The S3 bench** (scripts/device.sh at 16,384 B, /dev/ttyACM2, 20:13–20:32Z): DEVICE PASS, all 1,384 cases byte for
  byte as the host, the five cancel rows among them. The worst stack is 11,360 masked (STU chain X^ 44 d, as at
  8462e77) plus interrupts 400: 11,760 of 12,288. The container's peak was 893,075,456 B.

