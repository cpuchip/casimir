# CAS work unit 037: a closed sum's value where the typed sum has none

**From:** casim · **To:** abacus · **Status:** blueprint, for h037 (abacus freezes it from this file). casim writes the
unit; abacus writes the hidden set and the Accepted section.
- **From:** Michael's card cas037-wrong, ruled 2026-10-31 14:06Z (abacus #11859): "Fix first (CAS 037), then release
  1.5.0 with both." The class was found in CAS 036's acceptance read (abacus #11679) and measured for the card
  (#11680). A 2 h prototype (#11681) was measured and held as it was, outside this repo (#11682, #11683).
- **The question:** a closed sum, to N or to ∞, whose summand holds a power with K in its exponent, or a divisor in K
  alone. Where canon merges the powers or cancels the divisor, the line can have a value at a point where the typed
  sum has none. That is a wrong answer, so its row is hard and never gives way.
- **The reading:** docs/language.md already reads a sum term by term this way: "Summed term by term, the text is read
  with each K put in, so an exponent in K counts too: Σ Y^K×Y^(-K) from -1 to 1 is `3; Y≠0`." The closed form didn't,
  and CAS 037 makes it read the same.

## The class, as 1.4.0 and CAS 036 print it

Found back to 1.3.0, with the same lines at 1.4.0 and at ead17ba:
- **(A) A factor that cancels the base's negative powers.** `(Y×SQRT(Z))^K×SQRT(Z)` from -1 to N prints
  `1÷(Y-Y^2×SQRT(Z))-(Y×SQRT(Z))^(N+2)÷(Y-Y^2×SQRT(Z)); Y≠0; Z≥0; Y×SQRT(Z)≠1`, which is 1 at Y = 1, Z = 0, where
  the term at K = -1 is SQRT(0)^(-1) and has no value. `Y^K×Y` from -1 to N prints `-1÷(Y-1)+Y^(N+2)÷(Y-1); Y≠1`,
  which is 1 at Y = 0.
- **(B) Powers that cancel exactly.** `Y^K×Y^(-K)` from 1 to N prints `N`, and `(Y-1)^(-K)×(Y-1)^K` the same, though
  every term has no value at the base's zero. This holds from any lo.
- **(C) A divisor in K alone that is 0 at a K in range.** `((K+1)^2-1)÷((K+1)-1)` from 0 to N prints
  `5×N÷2+N^2÷2+2`, which is 2 at N = 0, where the one term is 0÷0. `(K^2+K)÷K` from 0 is the same shape. A divisor in
  K that canon keeps is UNDEFINED already: `1÷(K-2)` from 0 to N.

## The change

Two parts, each its own commit. The figures are the prototype's, on ead17ba, and each cost is the S3 text measured by
scripts/size-s3.sh.

1. **The base of a power with K in its exponent** (A and B, +848 B).
   - **td_walk:** a closed sum's `z` entry, CAS 036's base of u^K, is read for any exponent that holds K, as u^e.
   - **td_eneg:** an entry counts where e is negative for some whole K from lo. It reads e at lo, lo+1 and lo+2, then
     asks whether e falls linearly from lo. A divisor's base (e 0 under ÷) always counts.
   - **td_zsaid:** an entry whose base a row already says ≠ 0, expanded or as its negative, is left out. Without it
     `(K+1)×((Z-1)×Y)^K` from -1 to ∞ prints `Z≠1` beside `-Y+Y×Z≠0`, which says it (work/031-examples.txt:120), and
     `(K+1)×(1-Z)^K×Y^K` from -1 goes TOO_LONG on a row `Z-1≠0` says (:123).
   - **td_sum:** where the answer doesn't show the hole, a `z` entry now gets hard rows, as any typed condition does.
     CAS 036 gave it none there. Where the answer shows the hole, CAS 036's own `z` cases (u^K from a lo below 0, a
     divisor's base) keep their give-way rows to N. The wider reading makes none there: the line has no value at that
     point, so the most it can leave is a missing value, CAS 036's class. Made as give-way rows in a first version,
     they took 24 lines' groups past the cap, and those lines lost CAS 036's rows.
   - To ∞ the entries are read too. CAS 036 read them to N only.
2. **A divisor in K alone that canon cancels** (C, +944 B).
   - **td_walk:** a divisor that holds K and isn't the letter K is kept as a `q` entry.
   - **td_kq,** run in do_sum_ before the closed form, while the arena is low: a `q` divisor that the expanded summand
     no longer divides by, linear in K and 0 at a whole K in the sum's range (td_kzero), makes the sum UNDEFINED.
     That is the status `1÷(K-2)` from 0 to N has already. td_sum leaves `q` entries out.
   - **td_kzero** makes its rational with pf_rmake, out of line. Written with rat_make, as the prototype had it, it
     moved the host frame of lm_sum from 112 to 128. src/casim.c says why elsewhere: "a new call to it moves lm_sum's
     frame, CAS 017's lesson".

## Measured (the prototype, on ead17ba)

**S3 text**, from ead17ba's 263,335 B:

| Build | Text (B) | Added |
|---|---|---|
| Part 1 | 264,183 | +848 |
| Parts 1 and 2 | 265,127 | +1,792, 5,209 under the line |

**Frames:**
- **S3:** none of ead17ba's 465 frames moves. td_zsaid (48) and td_eneg (80) are new with part 1, td_kzero (64) and
  td_kq (224) with part 2.
- **Host:** none of ead17ba's frames moves, the 436 recorded among them. td_eneg (128), td_zsaid (80), td_kzero (80)
  and td_kq (320) are new.

**Values where the sum has none** (wrong036.py, the sibling of CAS 036's missing036.py, neither in this repo: each sum
added term by term from the typed summand, each letter at 0, ±1, 2, 1÷2, π÷2 and π, N from lo-1 to lo+2, and to ∞ a
term from lo to lo+6):

| Family | Sums | ead17ba | CAS 037 |
|---|---|---|---|
| CAS 036's: its card's kin and the six of its measure | 8,712 | 13 | 3 |
| Aimed at the class | 456 | 160 | 0 |
| Its letter forms | 72 | 26 | 0 |
| The 1.4.0 mutant search's inputs (whole lo, to N or ∞) | 23,592 | 86 | 21 |

The 24 left are the instrument's own, and each is there at ead17ba too: 18 are EXP(-20000)'s powers, which the
60-digit evaluator takes as 0 (0÷0), and 6 are a summand of N, which it reads as the bound. So the real ones go from
10, 160, 26 and 65 to none. The family's 10 are abacus's shape for SIN, COS, SQRT, ABS and the cube root, from -1 to N
and to ∞.

**Lines moved, both parts against ead17ba,** at 8,800 B (the CLI's default) and at 16,384 B:
- **Families of 10,271 sums:** 10 gain one row. Of the 148 lines that gain rows here and below, 80 are to N and 68
  to ∞.
- **The 1.4.0 mutant search's inputs, 35,388 sum lines:**
  - 138 gain rows (132 one, 6 two). No answer moves.
  - 3 go from OK to UNDEFINED, (C): `((K+1)^2-1)÷((K+1)-1)` from 0 to N and to ∞, and from -2 to N.
  - 6 go from NO_CLOSED_FORM to UNDEFINED: `(-8)^(1÷(-K))` and `(-8)^(1÷(2×K))` from 0 to N and to ∞, and from -2
    to N. Each has no value at K = 0.
  - Refusals change kind with the arena. At 8,800 B, 3 go from TOO_LONG to NO_ROOM. At 16,384 B, 1 does, and 7 go
    from NO_ROOM to TOO_LONG. All 11 are TOO_LONG on both builds at 65,536 B. Four need more arena, the most
    `(1+Z×(Y-1)^2)^(-K)` from -2, from about 6,400 B to 9,400, and seven need less.
- **Golden corpus:** 0 of 10,822.
- **Documented examples:** 3.
  - work/027-examples.txt:19 and :48 gain `Z≠0` and `SIN(Z)≠0`: the K = -1 term of `(SQRT(Z)×Y)^K×SQRT(Z)` and of
    `(SIN(Z)×Y)^K×SIN(Z)` from -1 has no value there.
  - work/033-examples.txt:55 goes from NO_ROOM back to its answer at 2,048 B (below).

**Rows stricter than the sum, past the empty sum** (a check not in this repo, on the 148 lines that gain rows): 17.
Each is (B) from a lo where the exponent is 0, at N = lo, where the one term is u^0×u^0. So `Y^K×Y^(-K)` from 0 to N
prints `N+1; Y≠0`, though at Y = 0 and N = 0 the sum is 1. This is the u^0 exclusion abacus named for CAS 036's rows
from a lo of 0 (#11679 (a)).

**make test,** which runs make check first, with td_kzero on pf_rmake, the 3 rows re-pinned and two mutant anchors
retargeted (below): green, 20 tests, 118,449 checks (293 s; claim #11879).

## What it reverses

- **CAS 027's reading.** work/027-examples.txt:44-46 calls `SIN(Z)≠0` for `(SIN(Z)×Y)^K×SIN(Z)` from -1 "a row for
  nothing", judged on the summand as canon reads it (that file's lines 4 and 5). CAS 037 prints that row, as (A). The
  comment is rewritten.
- **docs/language.md:710-711:** "A term is read as canon reads it, with its powers merged and nothing cancelled
  after". The sentence is the ratio's row test, and it stays. A power with K in its exponent gains its own sentence,
  read as typed, beside the one at :716.

## A miss of mine in CAS 036, found here

work/033-examples.txt:55 is the pin for CAS 025's ratio025-negpow-row-off (951151b). It works by arena: the build
answers `(X÷Y)^K÷Y` from -2 at 2,048 B, and the mutant's probe needs room it doesn't have, so it prints NO_ROOM. CAS
036 put the build's least arena for it at 2,056 B, and its re-pin commit, f126e26, took the row to NO_ROOM. That is
the mutant's line too (its least arena is 2,064 B), so at ead17ba the row tells the two apart no longer. Run through
the mutant's CLI, no documented row fails. I re-pinned it without reading the comment above it.
- 1.4.0's least arena for it is 2,024 B, and CAS 037's is 2,024 again, so the re-pin to the answer makes it a pin
  again.
- Measured under claim #11879: `scripts/mutants.py ratio025-negpow-row-off` at ead17ba says SURVIVED, and on the
  CAS 037 tree with the row re-pinned it is caught by work/033-examples.txt:55. So accepted CAS 036 carries one
  survivor, and CAS 037's re-pin ends it.

## Open, for abacus

1. **(C)'s status.** UNDEFINED for the whole line, as for a divisor canon keeps. From -2, `((K+1)^2-1)÷((K+1)-1)`
   has a value for N = -2 and -1, as `1÷(K-2)` from 0, UNDEFINED at ead17ba, has for N = 0 and 1. Lean: the same
   status.
2. **The 6 NO_CLOSED_FORM lines that become UNDEFINED.** A refusal becomes the status that says why: the term at
   K = 0 has no value. Lean: in.
3. **The 17 rows at N = lo:** under Not yet, with CAS 036's u^0 item. Lean: yes.
4. **The two readings above:** CAS 027's comment and language.md, as written in What it reverses. Lean: yes, since the
   card's ruling takes the class as wrong answers.
5. **The instrument in the repo.** wrong036.py as scripts/wrong-values.py, in make check over the family aimed at the
   class and its letter forms (528 sums, 1.2 s here), with a planted wrong line it must catch. Lean: yes, so the class
   can't come back unseen. missing036.py stays out: it is slow, and CAS 036's gates hold its class.

## Not yet (for the release notes)

- **The u^0 rows at N = lo** (17 in the search's inputs; above).
- **The closed form's own hole** (CAS 036's Not yet, unchanged): `(Y÷TAN(Z))^(K+1)` from -1 to N has no value at
  Y = 0 for N = -2, and `X^(K+2)×Y^(-2×K)` from -2 at X = 0 for N = -3, where the sum is 0. Those are missing values,
  and the typed summand has no condition there.
- **The `÷` spelling:** `Y^K÷Y^K` from 1 is NO_CLOSED_FORM, where `Y^K×Y^(-K)` prints `N; Y≠0`.
- **A divisor of degree 2 or more in K that canon cancels** is read by no `q` rule. The shapes tried are refused
  (`((K^2-1)×K)÷(K^2-1)`, NO_CLOSED_FORM) or UNDEFINED already.
- **Refusals that change kind with the arena** (above).

## The build

**Commits:**
1. Part 1, the `z` reading.
2. Part 2, the `q` divisors, with td_kzero on pf_rmake and the existing raw_letter declaration moved above td_walk
   (the prototype declared it twice).
3. The 3 documented rows re-pinned, and 033:55's comment saying why it pins at 2,048 B again.
4. work/037-examples.txt and the td037 mutants, each built and recorded before its catcher is chosen. The two CAS 036
   anchors the change replaces are retargeted with their meaning kept:
   - td036-z-pow-off: the `z` line off, now the widened one;
   - td036-z-any-lo: td_eneg always says negative, which is u^K read from any lo.
5. scripts/wrong-values.py in make check, if ruled (5).
6. docs/language.md, and this file's Built section.

**Mutants (td037):**
- the wider exponent read as CAS 036's (e = K alone);
- td_eneg reading lo alone (no lo+1, lo+2 or linear fall);
- a divisor's base not counted (td_eneg false for e 0);
- td_zsaid off (caught at work/031-examples.txt:120 and :123 by a hand build), and td_zsaid without the negative
  (:123);
- the not-gated `z` entry with no rows (CAS 036's);
- the wider reading's give-way rows where gated;
- td_kq off;
- td_kzero's range test off;
- td_kzero without the cancelled-divisor test (ib_den).

**Golden:** nothing moves, so no boundary file. The golden checks run as they are.

**Gates:**
- make check and make test;
- `make size` under 270,336;
- the frame tables (S3 and host: td_zsaid, td_eneg, td_kzero and td_kq new);
- the device bench (claim);
- the td037 mutants and the two retargeted td036 ones (claim). The full mutant run at 1.5.0's tree waits for the
  release.
