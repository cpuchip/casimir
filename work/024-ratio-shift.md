# CAS work unit 024: the shift for a ratio in two letters

**From:** casim · **To:** abacus · **Status:** built (As built and Measured below), and fixed after h024 (frozen at
#9575: sha256 47c598dc…0a16a0c, 126 rows; one failure, After h024 below). The draft was ruled at #9560, #9563 and
#9572 and measured on a throwaway prototype (p1, over 305bd84) before the build.
casim writes the unit; abacus writes the hidden set and the Accepted section.
- **Why now:** the order abacus set for v1.0 (#9354): complex roots, the series unit, then the two-letter-ratio shift.
  CAS 018 named it when its one-letter shift landed (7fbcefa, abacus #8349): "Y^K×X^(K−1) still shows X≠0"
  (work/018-sums.md, named, not held; casim's limit, #8486). 022 and 023 came in between, ruled by Michael (#9428).
- **The question:** when a sum's ratio is a monomial in two letters or more, does the answer carry an L≠0 row only where
  the sum itself has no value at L = 0, as 018's shift already gives a ratio in one letter?

## Where it is today

Rule 4b (018) adds L≠0 for a letter L of the ratio where the answer has no value at L = 0 (the hole probe). The shift
(sm_shift, abacus #8349) sums a letter group q(K)×B^K whose q holds a negative power of B's letter as
q(K+s)×B^s×B^K over lo−s .. hi−s, so the answer divides by no power of L and the probe finds a value. It reads B with
sm_mono, which takes k×L^m in one letter, so a ratio in two letters is never shifted:

| Command | Prints at 305bd84 |
|---|---|
| `casim sum 'Y^K×X^(K-1)' K 1 N RAD` | `-Y÷(X×Y-1)-(X×Y)^(N+1)÷(X-X^2×Y); X≠0; X×Y≠1` |
| `casim sum 'X^(K-1)×Y^(K-1)' K 1 N RAD` | `-1÷(X×Y-1)-(X×Y)^(N+1)÷(X×Y-X^2×Y^2); X≠0; Y≠0; X×Y≠1` |

At X = 0 the first sum is Y (its K = 1 term, 0^0 = 1, and every later term 0), and the second is 1. Neither row says
where an answer fails that the sum doesn't. To ∞ the row isn't there today (Y^K×X^(K−1) from 1 to ∞ is
`-Y÷(X×Y-1); ABS(X×Y)<1`).

## Rules (proposed)

1. **A ratio that is a monomial in its letters is shifted as 018's is:** B = k × L1^m1 × L2^m2 × …, a rational k and
   each letter at a whole power 1 ≤ m ≤ 12. For each letter, s_L is the least shift that clears q's negative powers of L
   (⌈−e÷m⌉ for the most negative power e), and q becomes q(K+s)×B^s over lo−s .. hi−s as today.
2. **With two letters or more, only the letters a shift of at most lo clears** (lo a whole number; a symbolic lo: no
   shift): s is the largest s_L ≤ lo. A letter whose s_L passes lo has a term that divides by it, so the sum has no value
   at L = 0 and its row stands. Shifting past lo would leave B to a negative power in the answer at N = lo−1, and B is 0
   wherever any one letter is: without this rule the prototype printed, for Y^K×X^(K−1) from 0,
   `1÷(X-X^2×Y)+Y×(X×Y)^N÷(X×Y-1); X≠0; X×Y≠1`, which has no value at Y = 0, N = −1, where the empty sum is 0.
3. **One letter: unchanged.** 018's shift stays as accepted. With one letter, B is 0 only where that letter is, and a
   shift past lo keeps the letter's row, so the same negative power is never reached (question 1).
4. **The rows are rule 4b's, unchanged.** Only the answer they probe changes. A ratio with a letter at a negative power
   (X÷Y) and every other B isn't shifted (question 2).

## Measured (prototype p1, over 305bd84; ~/scratch/casim/wt-024, sm_shift and its call)

- **A probe of 23 two-letter sums** (c24/probe.py), each answer valued exactly against the sum added term by term at X
  and Y in {0, 1÷2, −1÷3, 2, −3÷2} for N from lo−1 to 6, and each L≠0 row against the truth (a term from lo divides by
  L). A typed ÷X is judged as canon reads it ((2×X×Y)^K÷X is 2^K×X^(K−1)×Y^K). At 305bd84: 17 of the 23 carry a row for
  nothing (21 rows), no value wrong. p1 without rule 2: 5 have no value at the empty sum where the sum is 0 (the shift
  past lo). p1: 0.
- **Rows that change** (p1, from 1 to N unless said; every probe row, as printed):

  | Sum | 305bd84's rows | p1's |
  |---|---|---|
  | Y^K×X^(K−1), (X×Y)^K÷X, X^(K−1)×Y^(K+1), K×X^(K−1)×Y^K; X^(K−2)×Y^K and X^(K−1)×Y^K from 2 | X≠0; X×Y≠1 | X×Y≠1 |
  | X^(2×K−1)×Y^K; (X^2×Y)^K÷X^3 from 2 | X≠0; X^2×Y≠1 | X^2×Y≠1 |
  | (2×X×Y)^K÷X | X≠0; 2×X×Y≠1 | 2×X×Y≠1 |
  | (X×Y÷2)^K÷X | X≠0; X×Y÷2≠1 | X×Y÷2≠1 |
  | (−X×Y)^K÷Y | Y≠0; −X×Y≠1 | −X×Y≠1 |
  | X^(K−1)×Y^(K−1); X^(K−1)×Y^(K−2) from 2; X^(K−3)×Y^(K−1) from 3 | X≠0; Y≠0; X×Y≠1 | X×Y≠1 |
  | X^(K−1)×Y^(2×K−1) | X≠0; Y≠0; X×Y^2≠1 | X×Y^2≠1 |
  | X^(K−1)×Y^(K−2) | X≠0; Y≠0; X×Y≠1 | Y≠0; X×Y≠1 (Y's s_L is 2, past lo) |
  | X^(K−3)×Y^(K−1) from 2 | X≠0; Y≠0; X×Y≠1 | X≠0; X×Y≠1 (X's s_L is 3, past lo) |
  | (K+1)×X^(K−1)×Y^(K−1) | TOO_LONG (the answer past 191 bytes) | an answer; X×Y≠1 (the shifted answer is shorter) |

  Unchanged, since a term from lo divides by the letter: Y^K×X^(K−1) from 0, X^(K−2)×Y^K from 1 and (X^2×Y)^K÷X^3
  from 1 (X≠0), X^(K−1)×Y^(K−1) from 0 (X≠0; Y≠0). Y^K÷X keeps Y≠1 alone: every term divides by X, 018's no-row case.
- **The golden:** the corpus moves 3 lines, all `sum Y^K×X^(K-1)` from 1, which lose X≠0 (the answer
  `Y×(X×Y)^N÷(X×Y-1)-Y÷(X×Y-1); X×Y≠1`). →DEC, complex and the series route compiled out move the same 3; the doc
  examples move 1, work/018-examples.txt:142, the row 018 wrote to show the limit. Nothing else moves.
- **The judge:** sum-values.py's reader gave 0 to a negative power as Infinity, not as no value (1÷0 raises). It hadn't
  mattered (its letter answers divide with ÷), but a shifted answer holds B^(N−1). It raises now; sum-values is 329
  checks, 0 failed, on 305bd84 and on p1.
- **S3 text: 239,257 B, +263 over 305bd84's 238,994** (sm_shift 878 → 1,156); 22,887 B left of decision 80's 262,144.
  **Frames:** sm_shift 128 → 96 on S3, 176 → 160 on the host; nothing else moved. The sums' deepest stack can't grow, and
  the device bench isn't on this path; the build reruns it anyway.
- **Rule 2's alternative, measured:** the same cap for one letter too moves 3 more corpus lines and one 018 example
  (K×X^(K−2) from 1 changes form; its X≠0 stands either way).

## Cost (estimate)

S3 +0.3 KB built. No new frame; sm_shift's shrinks.

## The oracle

- sum-values.py gains a two-letter family: q(K) × (c×X^a×Y^b)^K × X^e × Y^f with a and b in 1..2, e and f in 0..−3, a
  polynomial q to degree 2, lo in 0..3, to N and to ∞, judged exactly at a grid with 0 for both letters, N from lo−1, and
  each L≠0 row against the truth (a term from lo divides by L): no row for nothing, none missing. Controls: a row
  dropped, a row for nothing, a value moved.
- work/024-examples.txt: the table above, the kept rows, one letter unchanged (K×X^(K−1), K×X^(K−2) from 1), to ∞.
- Mutants: the multi-letter read off (one letter only again); rule 2's cap off; the cap applied to one letter; a letter
  at a negative power read as positive.
- The S3 frames, and the device bench at the shipping mask with sum rows that shift by two letters.

## Questions

1. **Rule 2 for two letters or more only** (my lean), or for one letter too? Uniform is simpler to state; it moves 018's
   K×X^(K−2) from 1 in form (3 corpus lines and an example) and changes no value or row.
2. **A ratio with a letter at a negative power stays unshifted** (my lean: out, on the follow-up list).
   (X÷Y)^K÷X from 1 prints `…; X≠0; Y≠0; X÷Y≠1`. The sum has a value at X = 0 (1÷Y), so X≠0 is a row for nothing. A
   shift by 1 clears X but puts 1÷Y into q, where Y≠0 stands anyway. It's a different reading of B, and its own probe.
3. **X^K÷Y^K says NO_CLOSED_FORM** (at 305bd84 and fc46c8b), while (X÷Y)^K and X^K×Y^(−K) answer. A refusal, not a
   wrong value: the quotient of two letter powers isn't read as one ratio. My lean: out of scope, on the follow-up list.

## Ruled (#9560)

abacus checked rules 1 and 2 by hand: with s ≤ lo, B's exponent in the answer at the empty sum (N = lo−1) is lo−s ≥ 0,
and a letter whose s_L passes lo has a term at K = lo that divides by it, so its row is the honest one.

1. **Q1: rule 2 for two letters or more only.** With one letter, B is 0 only where that letter's own row already
   excludes, so the cap would only move 018's accepted forms.
2. **Q2: out,** on the follow-up list. That row for nothing is today's, unchanged, not a wrong value.
3. **Q3: out,** on the follow-up list (a refusal).

Conditions for the build:
1. ~~**No shift below 0.** s is taken only to clear negative powers. When every s_L is 0 (X^(K+2)×Y^(K+1) from 0) there
   is no shift, and s is never negative, which would put B^(−1) into q.~~ **Restated (#9572, after casim's #9569: "My
   condition 1 was wrong, and your row shows why"): s lies between the largest s_L of the letters left without a row
   and lo.** The answer is P × (B^(lo−s)×R(lo−s) − B^(N−s+1)×R(N−s+1)), P = c^s × Π L^(a_L×s+e_L); at a letter's 0 and
   N = lo−1 every factor has a value exactly when s ≥ s_L for each row-free letter and s ≤ lo. For lo ≥ 0 that is rule
   2 (no shift when 0 is in range, so today's forms stay). For lo < 0 and a finite hi, s = lo: X^(K+2)×Y^(K+1) from −1
   printed `-X÷(X×Y-1)+X^2×Y×(X×Y)^(N+1)÷(X×Y-1); X×Y≠1` at 305bd84 and p1, with no value at X = 0 and N = −2, where
   the empty sum is 0 (found by the two-letter family, below). To ∞ no shift: there's no N power, and canon merges the
   numeric ones. A closed form's B^(N−s+1−d) is safe exactly when lo−s ≥ d (#9573); this one writes d = 0.
2. **A term is canon's reading of the summand with K put in, nothing cancelled after** (sharpened at #9563, asked at
   #9561), so (K−1)×X^(K−2)×Y^K from 1 has 0×X^(−1)×Y at K = 1, which has no value at X = 0, and X≠0 stands (rule 2
   gives that: s_X is 2, past lo). A typed ÷L that canon merges is read merged: (X×Y)^K÷X is X^(K−1)×Y^K, Y at X = 0
   and K = 1, and X≠0 goes (023's Q3; 018's X^K÷X from 1). h024 judges each letter at its merged power a×K+e.
3. **Three letters and a negative lo are in scope as rule 1 states them:** X^(K−1)×Y^K×W^K, X^(K−1)×Y^(K−1)×W^(K−2) from 1
   (W≠0 stands), and lo = −1 and −2 (no shift; every row it needs). To ∞ too: a row stands wherever a term from lo
   divides.
4. **Mutants:** the four under The oracle, plus the clamp off: ~~a negative s taken~~ s below a row-free letter's
   s_L (#9572).
5. **S3 stop: +1 KB built** (estimate +0.3). Past it, back to abacus before landing.
6. **The device bench,** as planned.

h024 is frozen before the build; only its hash comes to casim.

## As built (where the code differs from the rules, or settles what they left open)

- **sm_shift reads B factor by factor** (numbers skipped, each letter a K_VAR or a whole power 1..12 of one; anything
  else, no shift: a c that isn't rational, SQRT(2), π or EXP(1), is no number factor, so (SQRT(2)×X×Y)^K÷X stands as
  305bd84 prints it), computes s_L per letter from sm_negpow as 018 did, and takes s by the restated condition 1: with
  two letters or more, the largest s_L ≤ lo for lo ≥ 0 (0 when none; to N and to ∞ alike), lo for a negative lo and a
  finite hi, and 0 for a negative lo to ∞ or a symbolic lo. One letter takes the largest s_L as before. It now takes lo
  and whether hi is ∞ from sm_groups.
- **A negative s needs nothing new:** B^s with s < 0 is a power like any other, and the binomials C(j, i)×s^(j−i) are
  exact with a negative s (each partial quotient is a product of consecutive integers over a factorial). |s| is held
  to 12, as s was.
- **The judge's reader** (sum-values.py) gives 0 to a negative power no value (Decimal gave Infinity); 018's 300 seeded
  sums judge the same on 305bd84 and the build (329 checks, 0 failed, before the family).
- **The family is a separate seeded batch** (60 sums, seed 20261022), so the 300 draw as before. Its planted answers (a
  row for nothing, a row missing, a value moved) are refused before it runs. Each letter is written once at its merged
  power, so the text is canon's reading.
- **018's example row** (work/018-examples.txt, Y^K×X^(K−1) from 1, written to show the limit) moved, and its note says
  so; language.md's sums paragraph says where a ratio's letter gets L≠0.
- **mutants.py:** 018's sum-shift-off re-anchored to the new guard (the anchor check in make check named it); seven
  ratio- mutants (The oracle's four, the clamp as #9572 restated it, a negative lo not shifted, a shift to ∞ too).

## Measured (the build, d9aedd2, over 305bd84)

- **The golden:** the corpus moves 3 lines and →DEC the same 3 (tests/golden/moved/cas-024.txt): sum Y^K×X^(K−1) from 1
  loses X≠0. scripts/ratio-golden.py: with them put back, both files hash to 305bd84's, each after is its before's
  status with rows less L≠0, and its control refuses. Complex and the series route compiled out move the same 3. 023's
  and 022's boundaries hold with 024's moves put back first.
- **The two-letter family:** 60 seeded sums, 0 failures on the build; on 305bd84, 13 (12 rows for nothing, and the
  negative-lo empty sum). The probe (c24/probe.py, 23 sums): 305bd84 17 with rows for nothing, the build 0.
- **The examples:** work/024-examples.txt's 22 rows, each answer checked against the sum added term by term
  (judge_letter2; the ruled-out (X÷Y)^K÷X's values at four points); the doc examples are 753 commands, 0 failed both
  ways.
- **S3 text: 239,305 B, +311 over 305bd84's 238,994** (the stop is +1 KB; p1 was +263); 22,839 B are left of decision
  80's 262,144. **Frames:** sm_shift 128 → 112 on S3 (text 878 → 1,200), 176 → 160 on the host; no other frame moved.
- **Tests:** make check green (234,852 KiB, 163 s), with ratio-golden.py and the family; make test green, 20 tests,
  118,421 checks.
- **The device** (XIAO S3, host and chip at 12,288 B, the shipping 2,000 ms interrupt mask; condition 6): DEVICE PASS,
  1,347 cases (023's 1,341 and the 6 ratio rows), 0 different. The worst masked stack is unchanged, 11,360 (STU chain X^
  44 d); this run's interrupt excess measured 384, so the gate reads 11,744 of 12,288. The ratio rows (stack B, median
  ms, out bytes or status):

  | Row | Stack | ms | Out |
  |---|---|---|---|
  | Y^K×X^(K−1) from 1 (a shift by 1) | 2,080 | 7.7 | 43 |
  | K×X^(K−1)×Y^K from 1 (a degree-1 q) | 2,400 | 12.9 | 148 |
  | K^2×X^(K−6)×Y^K from 6 (a degree-2 q, s = 6) | 2,912 | 51.0 | TOO_LONG |
  | X^(K−1)×Y^(K−1)×W^(K−2) from 1 (three letters, W's row) | 2,560 | 11.0 | 63 |
  | X^(K+2)×Y^(K+1) from −2 (a shift by −2) | 2,560 | 9.5 | 55 |
  | (K^2+K)×X^(2×K−3)×Y^(K−1)×W^K from 2 (the heaviest) | 2,912 | 60.8 | NO_ROOM |

  The device's container peaked at 362.7 MiB under a 2g cap, tmpfs 0.
- **Mutants:** ~~the full set, 412, is running from d9aedd2 in 7 shards (started 03:21:51Z); its result comes with
  the report that follows it.~~ Stopped at 04:16Z with 221 caught and none surviving, when the fix below changed the
  source; the full set runs again on it (below).

## After h024 (abacus #9594)

h024 at f3fb26b: 252 runs, one failure, of a class #9560 condition 3 named ("to ∞ too: a row stands wherever a term
from lo divides"). R083, sum K×(X×Y)^K÷Y from 0 to ∞, printed `-X÷2÷(X×Y-X^2×Y^2÷2-1÷2); ABS(X×Y)<1` at both arenas,
as 305bd84 did. Canon reads it K×X^K×Y^(K−1), whose K = 0 term is 0×Y^(−1): no value at Y = 0 (#9563), so Y≠0
belongs. To N the row came from 4b's probe of the answer, which divides by Y; to ∞ the answer has a value at Y = 0.
Everything else held: 38 of today's 40 failing lines right (R083's 2 the rest), every older set byte for byte.

- **The fix (de84a69):** sm_shift marks each letter of B whose true s_L passes a whole lo (its terms from lo divide by
  it), and sm_rows gives that letter L≠0 without the probe, to ∞ as to N. The true s_L comes from the letter's lowest
  power in q, positive ones too (sm_powof replaces sm_negpow): read clamped at 0, a letter q holds at a positive power
  had s_L 0, and X^(K+2)×Y^(K+1) from −1 gained a row for nothing (the examples caught it before the commit).
- **What moves:** no golden line. R083 gains Y≠0, and its twin K×(X×Y)^K÷X from 0 to ∞ gains X≠0. One letter's
  sibling, K×X^(K−1) from 0 to ∞, had its row already: its answer X÷(X−2×X^2+X^3) has no value at X = 0. The
  must-stand rows of #9575 are byte for byte as before.
- **The oracle missed it, and now holds it:** the family's 60 draws never met a q that is 0 at lo with a negative
  power behind it; on the build without the fix they found nothing. A fifth of the draws now take q×(K−lo), and 7 held
  rows of the class run beside them; on the build without the fix, 4 of the 7 fail (Y≠0 or X≠0 missing).
- **Measured:** S3 text 239,559, +254 over f3fb26b and +565 over 305bd84 (the stop is +1 KB); 22,585 B left. Frames:
  do_sum_ 640 → 656 on S3 (the groups' letter mask, 4 groups × 4 B), sm_shift 128 (it was 112 at f3fb26b); on the host
  do_sum_ 800 → 816 and sm_shift 160 → 176. make check green (231,804 KiB); make test green, 20 tests, 118,421
  checks. The device again, since a frame moved: DEVICE PASS, 1,347 cases, 0 different, the gate at 11,744 of 12,288 as
  before; the ratio rows' stacks unchanged (2,080 to 2,912 B), the slowest 63.3 ms; the container 271.5 MiB, tmpfs 0.
- **Mutants:** the full set, 414 (ratio-rowl-off and ratio-rowl-clamped new), runs on de84a69 in 6 shards from
  04:16:44Z; its result comes with the report that follows it.
- **Found, not fixed (named for abacus):**
  - To ∞ with a negative lo there's no shift, and canon leaves a common factor in the answer:
    (K+1)×X^(K+1)×Y^K from −1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, whose X≠0 is for
    nothing (the sum at X = 0 is 0), as on 305bd84. Shifting by lo to ∞ too removes it (`-X÷2÷(X×Y-X^2×Y^2÷2-1÷2);
    Y≠0; ABS(X×Y)<1`) but turned one family sum past the cap (OK → DECIMAL_FORM). My lean: out, on the follow-up list.
  - An irrational c keeps today's rows (#9575's must-stand class), so K×(SQRT(2)×X×Y)^K÷Y from 0 to ∞ lacks Y≠0, as
    on 305bd84. On the follow-up list with the other irrational-c rows.

## Out of scope

Questions 2 and 3, ruled out: on the follow-up list, beside its items from before (#9480 (1), #9482): sum 1÷c^K to 3,
and canon's factored 1÷Z.
