# CAS work unit 035: sum, defint and limit read the typed text's domain

**From:** casim · **To:** abacus · **Status:** built, for h035. The blueprint (abacus #11400, #11401) was revised with
the prototype's numbers (#11433, #11434) and OK'd for the build at #11436, with rulings at #11402, #11421, #11436 and
#11455. limit's degenerate divisor was found in the build (#11460) and ruled in (#11461, #11467); Michael ruled the
domain edge (#11473). The build is over main cda0fcb (4c0523b's src). casim writes the unit; abacus writes the hidden
set and the Accepted section.
- **From:** Michael's card cas-wrong-widen, option (1), the real reading (abacus #11335): a value where the input as
  typed has none is a wrong answer. The errata are on every release page from 0.9.0 to 1.3.0.
- **The question:** sum, defint and limit promise a value only where the input has one. canon's domain rule
  (docs/canonical-form.md) lets it drop a part that has no value somewhere: u÷u is 1, u-u and 0×u are 0, u^0 is 1,
  EXP(LN(u)) and ALOG(LOG(u)) are u, a root raised back is its base. Then the three ops print a value where the input
  as typed has none. limit also read a function outside its real domain as typed: `SQRT(X)` at -1 printed `SQRT(-1)`.

## The change

**A walk over the typed tree beside canon's** (td_walk). Each part with a domain gives a condition on its argument u:

| Typed part | Condition |
|---|---|
| a ÷ u, u^(-n), INV(u) | u ≠ 0 (a product's factors and a power's base each; ABS(u) as u) |
| LN(u), LOG(u) | u > 0, and as a divisor u ≠ 1 |
| SQRT(u), u^(p÷q) with q > 1 | u ≥ 0 (p > 0) or u > 0 (p < 0) |
| ASIN(u), ACOS(u) | ABS(u) ≤ 1 |
| ACOSH(u) | u ≥ 1 |
| ATANH(u) | ABS(u) < 1 |
| TAN(u) | COS(u) ≠ 0, in the angle mode |

Never 0, and so never a ≠ condition: EXP, ALOG, COSH, a nonzero number, a negative power, a letter-free positive base
to a power. Each condition is one of three kinds:
- **in the op's own variable** (K, or defint's and limit's X): the op decides it;
- **in other letters only:** a sum's row, limit's row (CAS 023's), defint's BAD_ARG;
- **constant:** false is UNDEFINED, and not known is UNDECIDED where the answer doesn't show the hole. A typed SGN of a
  constant reads its sign, so `LOG(SGN(π))` has a value.

**Per op:**
- **sum:** a row for each condition in the other letters, appended after the line's rows, unless a row already says
  it (CAS 033's `Y-1≠0` says `Y≠1`). A row also isn't added where the answer has no value wherever the condition fails:
  - for ≠, CAS 033's probe (u put to K in the answer, K at 0, or 1 for u ≠ 1);
  - for >, ≥, ≤, <, the answer holding the same typed function of u.
  A sum summed term by term decides a condition in K at each K, canon reading the typed entry with that K put in, so
  each value splits as a typed one does and an exponent in K is read (#11517; before it, the K was put into canon's
  condition after the split). A closed sum's conditions in K are sm_typed_ok's, as before.
  Rows are hard: a line that doesn't fit is TOO_LONG, once a sum term by term has given way to its closed form (rule
  7; #11517 (3)). Rows ≥ and ≤ are new and, as soroban's probe showed, read, drawn
  and saved by the firmware (#11420); CAS 032's said rule and rows_put read them as themselves.
  The closed form's cancelled divisor is a typed divisor too (#11436 (1)): `K×(SIN(Z)×Y)^K÷SIN(Z)` from 1 to ∞ gains
  `SIN(Z)≠0`.
- **defint:** the interval check's tests on the typed conditions, beside canon's integrand: u's range on the interval
  (lin_range) against each condition; IMPROPER where one fails, as for canon's. A condition in other letters is held
  for the answer: where the answer doesn't show its hole, BAD_ARG (#11402 (2)), as for a letter in an interval's range.
- **limit:** before the walk, each side asked (both for '') of each condition in X:
  - u near the point (its limit from that side), against the condition; at the edge, the sign of the first
    derivative not 0 there;
  - a side with no real value is UNDEFINED, even where the other has values (TD_EDGE_STRICT; Michael's card
    cas-limit-edge kept it so, #11473: `SQRT(X)` at 0 is UNDEFINED two-sided and from below, `0` from above);
  - at ±∞, the condition's own limit;
  - a ≠ condition (u ≠ 0, u ≠ 1, COS(u) ≠ 0) in X alone is a point hole, which no limit needs.
  A condition holding X and a letter: its value at the point is the letter's row; at the edge the first derivative not
  0 gives each side's row (an odd one makes two-sided UNDEFINED); where it has no value at the point, its limit there.
  Conditions in other letters only go into CAS 023's set as its own two ops: u ≥ 0 written u > 0, ABS(u) ≤ 1 as
  1-u^2 > 0 (which prints ABS(Y)<1). That is stricter at the edge, never wrong, and keeps cd_print and cd_note as they
  were (#11455).
- **limit, a ≠ condition holding X and a letter** (td_degen; #11460, #11461). A point hole needs nothing, but where a
  letter's value makes g (u, u-1 or COS(u)) 0 for every X near the point, the text has no value there:
  `SIN(K×X)÷SIN(K×X)` at 0 printed `1`, and at K = 0 it has no value anywhere. In order, each step cheap before the
  next:
  - **k, the letters' expression whose 0 leaves g without X** (td_kill): a product's factor in letters, the one k of
    every term or factor holding X, a function's argument's, a power's base's or its exponent in letters; or, for a
    form a×X+b, a where it holds a letter (`X×Y+X×Z`: Y+Z). Where g at k = 0 is 0, or isn't decided, or holds other
    letters, the row k≠0.
  - **No other letter's value makes g 0 for every X:** g with X as (X-b)÷a, its form's inverse (a a nonzero number or
    one times k), holds no other letter; or g or one of its first three derivatives at the point is never 0 (a nonzero
    number, or with k given a product of k and parts never 0), or two in a row are SIN(u) and COS(u) of one u; at ±∞,
    g's limit is ±∞ or a nonzero number with no row.
  - **Otherwise UNDECIDED** (option (a) of #11461): `(Y×SIN(X)-SIN(Y×X))÷(Y×SIN(X)-SIN(Y×X))` at 0, whose divisor is
    0 for every X at Y = ±1 without losing X.
  The walks come before the derivatives: those cost a nest the arena (`SIN(SIN(Y×X))` took 4,016 B where 640 do).
  k = 0 is tried only for a letter or its power; a k in more letters (Y+Z) or linear in one (Y-1) gets its row
  untried, stricter where g there isn't 0 (`SIN((Y-1)×X)+1` at 0 gains `Y≠1`), which kept the unit in the flash
  budget.
- **limit, the series route:** where the walk refuses and CAS 021's series route answers, do_limit drops the walk's
  arena and its conditions, and so the typed rows, which it now notes again after the route's answer: at 8,800 B
  `1÷X^2-1÷SIN(X)^2+SQRT(Y)^2` at 0 printed `Y-1÷3` with no `Y>0` in every build of this unit before c422632. Found
  by series_test, whose forced route printed `1` for the golden `SIN(K×X)÷SIN(K×X)` lines.

## Examples (1.3.0's line, then CAS 035's)

| Input | 1.3.0 | CAS 035 |
|---|---|---|
| `sum (Y÷Y)^K` from 1 to N | `N` | `N; Y≠0` |
| `sum (Z×EXP(LN(Y))+1)^K` from 0 to ∞ | `-1÷Y÷Z; Y≠0; Z≠0; ABS(Y×Z+1)<1` | `-1÷Y÷Z; Y>0; ABS(Y×Z+1)<1` |
| `sum Z^K×SQRT(4-Y^2)^2` from 0 to ∞ | `-4÷(Z-1)+Y^2÷(Z-1); ABS(Z)<1` | `…; ABS(Y)≤2; ABS(Z)<1` |
| `sum K×(X×Y)^K÷Y` from 1 to ∞ | `…; ABS(X×Y)<1` | `…; Y≠0; ABS(X×Y)<1` |
| `defint SQRT(X)^2` over -2..-1 | `-3÷2` | `IMPROPER` |
| `defint X×Y÷Y` over 0..1 | `1÷2` | `BAD_ARG` |
| `limit EXP(LN(X))` at -1 | `-1` | `UNDEFINED` |
| `limit SQRT(X)` at -1 | `SQRT(-1)` | `UNDEFINED` |
| `limit SQRT(X)` at 0, two-sided | `0` | `UNDEFINED` (from + still `0`) |
| `limit EXP(LN(X×Y))` at 0+ | `0` | `0; Y>0` |
| `limit SIN(K×X)÷SIN(K×X)` at 0 | `1` | `1; K≠0` |

(1.3.0's second line is 4c0523b's `-1÷Y÷Z; ABS(Y×Z+1)<1` after CAS 034.)

## Measured

- **Frames:** none of the S3's 433 or the host's 436 moves; the td_* functions are new. What it took:
  - deriv is always_inline, so the new code calls ib_deriv;
  - the row solver is td's own, so cd_ne and cd_gt keep their one caller, inlined in cd_print;
  - limit's new conditions are CAS 023's ops;
  - td_defint and td_limit parse the text again, so do_defint's raw tree lives no longer;
  - the limit call runs after `base` with no result in the strict build;
  - td's own drop, since a new caller of taylor_drop let gcc fold irs_reset into it;
  - interval_walk `noinline` (#11467): with td_degen, gcc's partial inlining split it on the host (64 to 32, with a
    .part.0 of 64), and no arrangement of the new code held it; the attribute keeps its frame, 64.
  At 336e1ba the S3's 433 are as at 4c0523b (interval_walk 48, do_limit 304), none gone, 28 new.
- **Flash** (S3 text, decision 80's budget 262,144 B): 248,359 B at 4c0523b; 258,461 at 9913750; 262,426 at c422632,
  282 over, so td_degen was cut (td_core does td_nz1's work, k = 0 tried only for a letter): 261,829 at 336e1ba, 315
  left. CAS 036's rows (cas-missing-scope) will need room beyond that.
- **The families** (c36/cmp035.py; the prototype's numbers, #11433, held by the build except limit's ≥ as >):
  - 0 first parts move in 17,883 calls × 5 arenas.
  - TOO_LONG at 65,536 and 16,384 B: 22 lines (rows are hard); at 8,192 B: 4.
  - No new NO_ROOM at 8,192 B.
- **The golden corpus:** 37 of 10,822 lines move, and their 37 under →DEC (tests/golden/moved/cas-035.txt,
  scripts/typed-golden.py): 34 to a refusal, and 3 `SIN(K×X)÷SIN(K×X)` lines at 0 from `1` to `1; K≠0`.
- **The degenerate divisor** (c36/probe-ne: a family of 15 divisors as D÷D, X×D÷D, (X+1)×D÷D and SIN(X)÷X×D÷D at 8
  points, 480 calls, judged by the strict evaluator with the far samples at 10^k+0.37):
  - before td_degen, 318 of its 438 values had no value at some letter's value no row excludes; after, 0 of 410;
  - 28 go UNDECIDED, 26 of them at ±∞ (`SIN(X+Y)-SIN(Y)`, `SIN(Y×X)+Y`, `Y×X+X^2`, `(X+1)^Y-1`) and 2 `(X+1)^Y-1` at
    -1, where its base goes to 0;
  - the golden corpus's 1,551 sum, defint and limit calls and the reach family's 5,060 limit calls at 65,536, 8,192
    and 4,096 B: none goes UNDECIDED, and only the 3 golden lines move.
- **The doc examples:** 26 earlier rows re-pinned, each gaining a row: 25 their cancelled divisor's (work/024 to 031),
  and work/023's `SIN(K×X)÷SIN(K×X)` at 0 its `K≠0`.
- **min-arena** (the prototype's, #11433): up on 74 bench cases of 1,417, down on none, none newly past 8,192 B. The
  build wasn't measured so, and two bench sums term by term with a divisor in K went NO_ROOM at 16,384 B in b9efe8a,
  as h018's did (below, #11517).
  CAS 033's 2,048 B example needs 2,024 B as at 4c0523b (the lists of eight or fewer are on the stack). td_degen moves
  none of the bench's 1,411; its own four rows: `SIN(K×X)÷SIN(K×X)` at 0 512 to 560 B, the 40-deep SIN nests of Y×X
  and of X+Y 4,744 and 4,736 B as before.
- **The device** (336e1ba, scripts/device.sh at 16,384 B, 15:04Z): DEVICE PASS, 1,412 cases, each the same as the
  host's. The worst stack is unchanged, 11,360 masked + 384 = 11,744 of 12,288. The typed rows: the 40-deep SIN
  nests over themselves (td_kill and td_form at depth) 4,848 and 4,592 B of stack, 6.5 and 6.2 ms;
  `SIN(K×X)÷SIN(K×X)` at 0 2,448 B, 3.7 ms.

## The oracle

- **work/035-examples.txt:** 71 rows. A row per condition kind per op, the errata's lines, the answer that shows its
  own hole, the cancel's fit at 4,096 B, defint's three refusals, a typed SGN, limit's sides, edges, ±∞, letters and
  removable hole, 16 for the degenerate divisor (each source of k, the forms that need no row, the refusals), and 3
  where the series route answers at 8,800 B with the typed rows kept.
- **scripts/sum-values.py:**
  - a CAS 035 block: seed 20261035, 60 seeded and 10 held, judged where the typed term at the low bound has no value;
    planted 4c0523b lines refused and CAS 035's taken; 4c0523b's CLI fails it 63 times;
  - the evaluator's three fixes, each with a control that fails without it: a divisor 0 to 40 digits is 0, LN(0) has
    no value, the module's 60-digit context in any thread.
- **scripts/typed-golden.py** (make check): each moved golden line is a refusal where the typed input, valued apart
  from casim, has no value, or its line with a row added that excludes a letter's value where the typed input has none
  near the point. Its row judge has its own control, and with the judge broken (no row ever false) all 6 row moves and
  the control fail.
- **tests/series_test.c:** CAS 021's forced-route row `(-2-X)^(1÷3)` at 0 expected a refusal; the typed reading's
  answer is UNDEFINED, so the row now expects it.
- **Mutants:** 32 (scripts/mutants.py, "td035-"; 514 in all), all caught. At 336e1ba 24 were caught, 7 didn't build
  (a replacement left a function, variable or parameter unused, which -Werror refuses) and td035-kill-factor-none
  survived; the 7 now keep their references, a row catches the eighth, and at 8bb1839 all 8 are caught. The catchers
  are work/035-examples.txt's rows (each degenerate-divisor mutant by its own: the linear k by `(X×Y+X×Z)÷(…)`, the
  letter factor by `(Y×SIN(X)+Y×X^2)÷(…)`, the series route by the three rows at 8,800 B), the golden corpus's moved
  lines (td035-degen-off, row-never, limit-off, side-flip, gate-drops, probe-none, defint-letter, defint-off,
  const-ignored, sgn-undecided), and work/023-025's re-pinned rows.

## Carried in the same docs

- **A sum to N below LO-1** (Michael's card cas-sum-below, #11471): docs/language.md's sum section now says a closed
  form to N holds from N = LO-1, the empty sum, up. Below that it agrees with the subtracting convention wherever both
  have a value, and each can have one where the other has none (work/sum-below-lo.tsv: 320 sums to N, 19 and 3 lines
  of those kinds, no value apart). No src.

## After the first acceptance read (abacus #11517, at b9efe8a)

abacus judged h035 green at 8,800, 16,384 and 65,536 B, and found three things in the older sets:
1. **Lost answers** (h022, sum K from 1 to 3 at 12,288 and 8,800 B): `1÷EXP(-20000)^K`, `1÷(-EXP(-20000))^K`,
   `1÷EXP(20000)^K` and four near-zero divisors (`(SIN(1)^2+COS(1)^2-1)±10^(-30)`, `(10^25×SIN(1)^2+…-10^25)±10^(-12)`
   to the K) went from 1.3.0's value to UNDECIDED. td_sum put each K into canon's condition after td_canon's split, so
   K = 2 asked whether `base^2` is 0, which the sign test can't decide and the answer doesn't hold (it holds `base^-2`).
   The 10^25 lines also needed the primitive part: canon's power of a sum takes its content out (power_sum), so the
   answer divides by `10^25-SIN(1)^2×10^25-…`, the typed base negated.
2. **NO_ROOM where 1.3.0 answered:** h030's `1÷COS(Z)^K` and `X^K÷LN(Z)^K` (K to 12) took a row per K, past the row
   list; h018's 15 rational sums term by term (`(18+12×K)÷(K^4+6×K^3+9×K^2)` from 1 to 31: 2,928 B before, 9,432 B in
   b9efe8a) kept each K's work to the end; h018's `(7÷2+K)^(-2)-…` to ∞ canonicalized each typed 2 of `7÷2` (8,789 to
   8,813 B, past 8,800); h032's `(Y÷TAN(Z))^K+COS(Z)^K` at 4,096 B probed `TAN(Z)≠0` with a copy of the answer (3,976
   to 4,128 B) where CAS 030's `TAN(Z)≠0` row already said it.
3. **A question:** h030's `(ACOS(Z)^2×Y)^K÷ACOS(Z)` from 1 to 12 printed the closed form. The term-by-term line is 185
   bytes and gains `ACOS(Z)≠0` (the typed text has no value at Z = 1, where 1.3.0's line is 0), 198 with it, so rule 7
   gives way to the closed form, whose values abacus found right.

**The fix** (cas035b):
- td_sum puts each K into the typed entry and td_canon reads it, so a value splits as a typed divisor does (u^n is 0
  where u is, a product where a factor is, EXP never 0), an exponent in K is read (`Y^K×Y^(-K)` from -1 to 1 gains
  `Y≠0`; `(-8)^(1÷K)` from 1 to 3, whose second term is SQRT(-8), is UNDEFINED), and each K's work is dropped.
- td_gated probes a sum's primitive part where the sum itself isn't found in the answer.
- td_said reads a CAS 030 `?` row as saying u≠0, since one is made only where the answer has no value there.
- td_walk leaves out a typed number's divisor; canon refuses ÷0 itself.
- td_base holds the u^n and ABS step td_canon took twice.

Every line abacus named now prints 1.3.0's line, except `(ACOS(Z)^2×Y)^K÷ACOS(Z)` (3, above). The nine exact zeros
(`1÷(SIN(1)^2+COS(1)^2-1)^K`) print 1.3.0's expansion too, not UNDECIDED: the answer divides by the same zero, so it
shows the hole, as b9efe8a already did for `1÷(SIN(1)^2+COS(1)^2-1)` without the K. That is for abacus to judge.

Measured at f8cd4dc:
- min-arena of abacus's lines, 1.3.0 and the fix: 2,928 and 2,928; 8,789 and 8,789; 3,976 and 3,976 B.
- S3 text 262,036 B, 108 under the budget (b9efe8a 261,829). S3 frames: the 433 as at 4c0523b, 29 new (td_base);
  td_gated 48 to 80, td_sum 320 to 304. Host frames: the 436 hold (make check).
- make check green; make test green, 20 tests, 118,449 checks.
- The families (c36/cmp035.py, b9efe8a against the fix): 17,243 calls at 65,536, 16,384, 8,800 and 4,096 B, every
  line the same. Its control, 1b66fd0 against the fix on the golden family, shows CAS 035's 37 moves.
- Mutants: 37 ("td035-", 519 in all), all caught; the five new ones each by its row in work/035-examples.txt.
- The device (scripts/device.sh at 16,384 B, 19:30Z): DEVICE PASS, 1,412 cases; the worst stack unchanged, 11,360
  masked + 384 = 11,744 of 12,288.
- The bench, case by case against CAS 034's device run (4c0523b, 1,371 cases in both): b9efe8a had made 18:152 and
  18:153 (`12×(561+…)÷(…)` from 1 to 50, work/018-examples.txt) NO_ROOM at 16,384 B, where 1.3.0 answers in 9,260 B
  (18,848 B in b9efe8a). Its device run passed, since the host printed NO_ROOM too. The fix answers them in 9,260 B
  again, with CAS 034's 3,792 B of stack. Against CAS 034 no case changes status now; 51 need more arena (at most
  1,976 B more, `1÷(` nested 43 deep, its limit at 0+, 4,864 B) and 14 more stack (at most 464 B more).
- work/035-examples.txt: 13 rows for these (84 in all).

## Not yet

- **limit's letter conditions are stricter at the edge.** `SQRT(Y)^2×X` at 1 prints `Y; Y>0` (Y = 0 has a value) and
  `ASIN(Y)×X` at 1 prints `ASIN(Y); ABS(Y)<1` (|Y| = 1 has one), beside CAS 023's own `SQRT(Y); Y>0`. Listed for CAS 036
  (#11455).
- **X×Y-form limits at ±∞** go UNDECIDED: the sign is Y's, and no row is given (`SQRT(X×Y)÷SQRT(X×Y)` at ∞).
- **`SQRT(X^2×Y)^2` at 0** gives `0; Y>0` (Y = 0 has a value).
- **22 lines go TOO_LONG at 16,384 B**, rows being hard. Two families:
  - `K×(1+Z×F(u))^K` from 1 to ∞ with u Y+1, Y-2 or W-1;
  - `(K-1)×(F(Z)×Y)^K÷F(Z)` from 1 to N for ACOSH, ASINH and ATANH, and LN from 2.
- **The missing values** at the empty sum: Michael's card cas-missing-scope, ruled for CAS 036 (rows for the
  function class too, giving way at the cap; #11473).
- **limit's degenerate divisor, refused where it isn't decided:** the 28 of the family above, as UNDECIDED.

## Asked

- abacus: h035 is frozen (#11511: sha256 043ce4abce65a4bcd9cbc351904566800674bbcb372600c7026b0e04ff875d1e, 1,074
  rows); abacus judges it at the gated sha, at 16,384 and 65,536 B, with the older sets and the mutants.
