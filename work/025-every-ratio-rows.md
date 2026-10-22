# CAS work unit 025: a ratio's rows wherever a term from lo divides, whatever else the ratio holds

**From:** casim · **To:** abacus · **Status:** built as ruled (#9687: p1 and ruling 2), for abacus's review against h025.
The draft below was measured on two throwaway prototypes, p1 and p2, over 5ef631e (src as at v1.0.0-rc6); Ruled, As
built and Measured follow it. casim writes the unit; abacus writes the hidden set and the Accepted section.
- **Why now:** Michael's decision 89 (#9671). He amended decision 88 for this one item: it's fixed before 1.0.0, in
  rc7. It's the one known wrong answer in rc6's release notes. The rest of the follow-up list stays past 1.0.
- **The question:** when a term of a sum from lo divides by a letter of its ratio, does the answer carry that letter's
  L≠0 row whatever else the ratio holds (an irrational constant, a letter inside a function, a letter at a negative
  power), as CAS 024 gives it for a ratio of a number and letters?

## Where it is today

CAS 024 (#9594, R083) marks the letters that a term from a whole lo divides by, and their rows stand outright
(sm_shift's `rowl`). Every other L≠0 row comes from rule 4b's hole probe, which asks the answer whether it has a value
at L = 0. sm_shift reads B factor by factor. It skips a number and reads a letter at a whole power 1 ≤ m ≤ 12. Any
other factor makes it return at once, before `g->rowl` is set. So for such a ratio only the probe is left, and an
answer that has a value at L = 0 gets no row, though the sum has none:

| Command (from 0 to ∞) | Prints at 5ef631e (rc6's src) |
|---|---|
| `casim sum 'K×(SQRT(2)×X×Y)^K÷Y' K 0 ∞ RAD` | `-X×SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); ABS(X×Y×SQRT(2))<1` |
| `casim sum 'K×(X×Y)^K÷Y' K 0 ∞ RAD` (R083, a rational c) | `-X÷2÷(X×Y-X^2×Y^2÷2-1÷2); Y≠0; ABS(X×Y)<1` |
| `casim sum 'K×(π×Y)^K÷Y' K 0 ∞ RAD` (one letter) | `-π÷2÷(Y×π-Y^2×π^2÷2-1÷2); ABS(Y×π)<1` |
| `casim sum 'K×(SQRT(2)×X×Y)^K÷(X×Y)' K 0 ∞ RAD` | `-SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); ABS(X×Y×SQRT(2))<1` |
| `casim sum 'K×(SIN(Z)×Y)^K÷Y' K 0 ∞ RAD` | `-SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); ABS(Y×SIN(Z))<1` |
| `casim sum 'K×(X÷Y)^K÷X' K 0 ∞ RAD` | `-1÷2÷(X-Y÷2-X^2÷2÷Y); Y≠0; ABS(X÷Y)<1` |

Each K = 0 term is 0×L^(−1) (condition 2 of CAS 024, #9563: canon's reading with merged powers, nothing cancelled),
so the sum has no value at L = 0. Each answer has one, and its rows hold there. The factors that end sm_shift early:
a constant other than a number (SQRT(2), π, EXP(1), LN(2), 2^(1÷3)), a letter inside a function or at a fraction power
(SQRT(X), SIN(Z)), and a letter at a negative power (X÷Y).

## Rules (proposed)

1. **The rows are read for every ratio.** sm_shift reads each letter that B holds at a whole power 1 ≤ m ≤ 12 and marks
   it when a term from a whole lo divides by it (CAS 024's rule, unchanged), whatever B's other factors. A factor that
   isn't a number or such a letter still stops the shift, as today. (p1: a flag in place of the early return, then
   `g->rowl` before the return.)
2. **Everything else is unchanged.** Forms aren't touched (no shift where there's none today). A symbolic lo keeps the
   probe, as in CAS 024. A letter B holds otherwise (inside a function, at a fraction or negative power) keeps the
   probe too (question 2).

## Measured (p1 over 5ef631e, in ~/scratch/casim/wt-025; p2 in wt-025b)

- **A row and value probe** (c25/probe.py) covered 207 sums:
  - 14 shapes, each with 14 constants. Ten are irrational (SQRT(2), π, EXP(1), −SQRT(2), 2^(1÷3), LN(2), 1÷SQRT(3),
    SQRT(2)÷3, π^2, 3×π) and four rational (1, 2, 1÷2, −3).
  - The shapes have a zero q at lo or none; the ratio is in one letter or two; lo is −1, 0, 1 or 2; the sums run to N
    and to ∞.
  - Eleven more sums: a letter in a function or at a fraction power, a letter at a negative power, R083, and CAS 024's
    follow-up rows.
- **How it judges.** At each letter L = 0, the other letters at 1÷3 and −1÷4, it asks two things:
  - Does the sum have a value? Every term from lo must have one, valued from its merged reading (#9563). To N that's
    for N from lo−1 to lo+5; to ∞, the terms lo to lo+12.
  - Does the answer claim one? It must have a value and every row must hold; a row with no value holds nothing.
- **How it checks values.** Each answer is valued at the letters 1÷7 and −1÷9 wherever its rows hold. It's compared
  with the sum added exactly to N, or to ∞ until six terms in a row fall below 10^−31 of it.
- **Controls, run before any count:**
  - R083's irrational twin shows its row missing on main.
  - R083 itself shows nothing.
  - (K+1)×X^(K+1)×Y^K from −1 to ∞ shows its X≠0 for nothing.
  - Three planted values (a constant, a sign, a power of N) are each caught.
- **Three faults in my own judge,** each caught by a control before a number was kept:
  - The reader had no ABS, and "not readable" counted as "no value", so no row could ever be found missing.
  - The typed text was judged instead of the merged reading, so (K−1)×(X×Y)^K÷X from 1 looked like it had no value
    at X = 0.
  - The value check ignored the rows and added only 80 terms.
- **5ef631e:** 55 of the 207 sums miss a row, 15 carry a row for nothing, and no value is wrong. The 55:
  - 51 with an irrational c, in five shapes, each from 0 to ∞ with q = K: K×(c×X×Y)^K÷Y, K×(c×Y)^K÷Y,
    K×(c×X×Y)^K÷(X×Y), K×(c×X×Y^2)^K÷Y, K×(X×Y÷c)^K÷Y, and K×(X÷(Y×π))^K÷X;
  - 2 with a letter in a function (K×(SQRT(X)×Y)^K÷Y, K×(SIN(Z)×Y)^K÷Y);
  - 2 with a letter at a negative power (K×(X÷Y)^K÷X, K×(X×Y^(−2))^K÷X), which lack X≠0.
- **p1:** 0 missing, the same 15 for nothing, 0 values wrong. Exactly the 55 sums move, each gaining its row, and
  nothing else moves.
  - **To N** none of the 29 sums is missing a row on 5ef631e, and p1 moves none. Two letters to N are TOO_LONG (past
    191 B). With one letter the answer to N divides by the letter, so the probe finds the row.
  - **A rational c** moves nowhere: CAS 024 already reads it.
- **The 15 rows for nothing,** the same on 5ef631e and p1:
  - 10 are (K−1)×(c×X×Y)^K÷X from 1 to ∞ with an irrational c. X≠0 isn't needed, since the merged term at K = 1 is
    0×X^0×Y. A rational c gets no such row, because CAS 024's shift clears it. The irrational-c route misses the shift
    as well as the rows (question 1).
  - 3 are (K−1)×(c×Y)^K÷Y from 1 to N, with c π, 1÷SQRT(3) or π^2.
  - (X÷Y)^K÷X from 1 to N (024's Q2) and (K+1)×X^(K+1)×Y^K from −1 to ∞ are both on the follow-up list.
- **The golden:** 10,822 checks, 0 moved, for p1 and p2.
- **make check:** green on p1 (2:53, peak 235,784 KiB) and on p2 (2:53, 235,200 KiB). It holds no irrational-c sum,
  so p2's forms move nothing it checks.
- **Size:** host -O2 text +64 B (p1) and +320 B (p2). S3 isn't measured yet; the build measures it.
- **h024's must-stand rows** (#9575: one letter, a negative-power letter in B, an irrational c and a symbolic lo, byte
  for byte against 305bd84) move by design in three of those classes, and only where a term from lo divides: an
  irrational c, one letter with an irrational c, and a negative-power letter in B (K×(X÷Y)^K÷X gains X≠0). A symbolic
  lo doesn't move. h024 as frozen will fail those rows. h025 is where they're judged.

## Questions

1. **p1 (rows only) or p2 (a letter-free factor read as a number, so an irrational c is shifted as a rational c is)?**
   - p2 takes the 10 irrational-c rows for nothing too. It leaves 2 rows for nothing (the two on the follow-up list),
     0 missing, 0 values wrong.
   - But it moves 120 of the 207 sums, 65 of them in form only.
   - Of those, 11 answers become NO_ROOM at the CLI's 8,800 B: (K^2−K)×(c×Y)^K÷Y^2 to ∞, whose SQRT(2) case needs
     9,104 B against 8,064 today. SQRT(2), π, EXP(1), π^2 and 3×π answer at 12,288 and 16,384. One more becomes
     TOO_LONG, and 7 TOO_LONG become answers.
   - My lean: **p1 before 1.0**. It fixes the wrong answers and moves no form. The irrational-c rows for nothing aren't
     wrong values, so they'd go on the follow-up list with p2 as their fix.
2. **A letter at a negative power, its own row.** `casim sum '(X÷Y)^K÷Y' K 0 ∞ RAD` prints `-1÷(X-Y); ABS(X÷Y)<1`.
   - The K = 0 term is 1÷Y, so the sum has no value at Y = 0, while the answer has one (−1÷X).
   - Only the ABS row stands between them, and it has no value at Y = 0.
   - `(X÷Y)^K×X` prints Y≠0 beside the same ABS row, because its answer has no value at Y = 0 and the probe finds it.
     K×(X÷Y)^K÷Y is the same.
   - Does a row with no value at L = 0 exclude L = 0, or should Y≠0 print? My lean: print it, as CAS 024's condition 3
     says ("a row stands wherever a term from lo divides"). It's small: read a negative whole power of a letter of B,
     whose terms from lo divide from some K on.
3. **Term by term.** Between whole bounds at most 64 apart, each term is canon's value of the typed text, and canon
   reads 0÷Y as 0. So `K×(π×Y)^K÷Y` from 0 to 3 is `π+2×Y×π^2+3×Y^2×π^3`, with no Y≠0, and a rational c is the same
   (`K×Y^K÷Y` from 0 to 3 is `2×Y+3×Y^2+1`).
   - That's #9474's class (the terms summed as written), but it disagrees with condition 2's reading (nothing cancelled
     after).
   - My lean: **out, named.** The unit is a closed form's rows, and the route term by term reads as simplify does.

## The oracle

- sum-values.py gains a family for this unit, seeded and separate from 024's:
  - q(K)×(c×X^a×Y^b)^K÷X^e÷Y^f, with c from a list of irrational constants (and π-free and rational controls);
  - one or two letters, lo in 0..3, to N and to ∞;
  - each judged on its merged reading for rows (none missing, none for nothing beyond the named ones) and for values.
  - Controls: a row dropped, a row for nothing, a value moved.
- work/025-examples.txt: the table above with p1's rows, R083 and its rational twins unchanged, to N unchanged, and the
  named rows for nothing as they stand.
- **Mutants:**
  - the early return put back (rows not read past another factor);
  - `g->rowl` set after the return;
  - the flag ignored, so B shifts past another factor;
  - a letter in a function read as a whole-power letter.
- The S3 text and frames. The device bench at the shipping mask with irrational-c sum rows, at 16,384 B.
- The README's counts, stale at rc6 (19 programs, 10,463 golden calls, 348 mutants, a 1,065-case bench, the 8,800 B
  arena, the taylor example's decimal form, sums under "doesn't do"), measured at the rc7 bump.

## Cost (estimate)

p1: S3 under +0.1 KB (host +64 B). No frame should move except perhaps sm_shift's, by a byte or two.

## Ruled (#9687)

1. **p1.** Rows only before 1.0: it fixes the wrong answers and moves no form. The 10 irrational-c rows for nothing
   ((K−1)×(c×X×Y)^K÷X from 1) go on the follow-up list with p2 as their fix, beside the 3 to N with c π, 1÷SQRT(3) and
   π^2. h025 counts rows for nothing unchanged from rc6 and doesn't fail them; a new row for nothing fails.
2. **Print Y≠0. A row with no value at a point does not exclude that point.** ABS(X÷Y)<1 at Y = 0 isn't false, it's
   meaningless, so it can't carry the exclusion. A letter of B at a negative whole power is read; wherever a term from
   lo divides by it, its L≠0 row stands, as CAS 024's condition 3 says. An excluded point must be excluded by a row
   that has a value there and is false.
3. **Out, named as simplify's convention:** "Added term by term (whole bounds at most 64 apart), each term is the typed
   text simplified, so it cancels as simplify does: 0÷Y is 0, X÷X is 1." The tension stays visible in rc7's notes and
   the README: K×Y^K÷Y from 0 to 3 has no row, while from 0 to ∞ it has Y≠0.

h025 was frozen before the build (#9694): sha256 b3b70d01767ef3b9499d4856785360ca8f961d5fdb037451e505d1e4b7447493, 124
rows, against e551d3e.

## As built (where the code differs from the rules, or settles what they left open)

- **sm_shift** keeps reading B past a factor that isn't a number or a letter at a whole power 1..12: a flag in place of
  rc6's early return, `g->rowl` set before the flag's return, so the letters' rows stand and the shift is still refused
  (p1's patch, as measured). Its signature and its frames are rc6's: carrying hi in for a whole-hi test moved the host
  frame 176 → 192, so the negative-power test went into the same branch as the flag.
- **A letter at a negative whole power** (ruling 2) sets its row bit for a whole lo, to N and to ∞ alike: the terms
  from lo divide by it from some K on (to N, for N past it), and the ratio's own row has no value at its 0. A whole hi
  is not tested; past it a term from lo would need q to hold L^53 or more, and that answer divides by L anyway.
- **A monomial at a negative power keeps the probe.** With one letter and a rational constant, ((1÷2)×X^(−2))^(K+1),
  the ratio's row is ABS(X)>v, which has a value at X = 0 and is false there, so it already excludes 0, and CAS 018's
  form stands (018's family refused the added X≠0 on the first make check). sm_rows ignores the bit there; sm_mono is
  asked only for a letter B holds, as before.
- **The oracle** (sum-values.py's third seeded batch, 60 sums, seed 20261025, and 10 held rows): q(K)×c^K×X^(a×K+e)×
  Y^(b×K+f)(×W^(K+g)) with c from 10 irrational constants, or a letter at a negative power, or SIN(Z)^K or SQRT(Z)^K,
  lo −2..3, to N and to ∞. Rows are judged by ruling 2: L≠0 must stand wherever a term from lo divides by L. Its planted
  answers come first (rc6's two, a row for nothing, a value moved, refused; the right ones and a named row taken).
  Three classes, each the same at e551d3e, are named and counted, not failed: rows for nothing where the refused shift
  would clear them (or from lo < 0), no value at a letter's 0 for N from lo−1 to −2 (#9700), and a letter in a function
  whose factor a term divides by (#9698). judge_n gained `first`, the first N it compares (the terms before it are
  still added). DECIMAL_FORM (3^(1÷4)'s ratio, as at rc6) counts with NO_ROOM and TOO_LONG.
- **Docs:** language.md's sums paragraph says where 025's rows stand and gives ruling 3's sentence; work/025-examples.txt
  (38 rows) is in doc-examples; measure.py gains 7 bench rows.

## Measured (the build, over 5ef631e)

- **The probe** (c25/probe.py, rows by ruling 2: a row with no value at L = 0 excludes nothing): 343 sums, the draft's
  207 and 13 shapes with a negative-power letter, each lo −1..2, to N and to ∞, and 4 monomials or one-letter irrational
  ratios. rc6's source misses a row in 72; the build in 0, and moves exactly those 72, all to ∞. Rows for nothing: 20
  on both, the same 20 (the named classes). Values: 0 wrong on both. Its controls: a dropped Y≠0, an added X≠0 and a
  moved value, each caught.
- **The oracle family, wide** (gen_letter3, 3 seeds × 2,000 draws): the build 0 failures; rc6 154 missing rows. Named
  per 2,000: ~245 rows for nothing, ~35 with no value below N = −1, 3–4 with a function's letter, the same on both.
- **The golden:** 10,822 checks, 0 moved; →DEC, complex and the series route compiled out the same.
- **make check** green (2:44, 234,436 KiB peak): frames as recorded, the 420 mutant anchors, sum-values 466 checks (60
  + 10 of 025's), doc examples 797 commands (1,606 checks). **make test** green: 20 tests, 118,421 checks (3:55,
  234,972 KiB).
- **S3 text: 239,580 B, +17 over rc6's 239,563.** No S3 frame moved (sm_shift 128, sm_groups 112, do_sum_ 656); host
  frames as recorded; host text +64 B.
- **p3** (abacus #9699, a row on a function factor; not in the build): measured and sent (#9701).
