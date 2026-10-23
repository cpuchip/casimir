# CAS work unit 027: the shift past a function factor (1.0.1)

**From:** casim · **To:** abacus · **Status:** ruled (#10004); the build waits on h027's hash. The draft below was
measured on a throwaway prototype, p4, over 671b8bf (1.0.0's code), not pushed. The hidden set's hash comes before the build. casim writes the unit; abacus
writes the hidden set and the Accepted section.
- **Why now:** Michael, 2026-10-23, on the missing values 1.0.0's notes name (abacus #9963): "lets do it as a 1.0.1
  fast follow. You made the right call because there is no wrong data just a message we cannot answer at present."
  One unit for 1.0.1; 1.0.0 stays as it is.
- **The question:** at a zero of a function factor of a sum's ratio other than its letter's 0 (SIN and TAN at π, COS at
  π÷2, LN at 1), where the sum has a value, does the answer have it?

## Where it is today

CAS 024's shift sums a letter group q(K)×B^K as q(K+s)×B^s×B^K over lo−s .. hi−s, with s the least that clears q's
negative powers of B's letters, so the answer divides by none of them. Any factor of B that isn't a number or a letter
at a whole power stops it (sm_shift's `other`). So past SIN(Z) the answer divides by SIN(Z), and the letter probe (rule
4b) finds that hole at Z = 0 and prints `Z≠0`. That row says nothing about π, where the answer has no value either,
while the sum has one. CAS 026's factor row isn't printed there, by its rule 2 (decision 90 (3)): the answer has no
value at the factor's zero already.

| Command (RAD) | 1.0.0 prints | The sum at the zero |
|---|---|---|
| `casim sum 'K×(SIN(Z)×Y)^K÷SIN(Z)' K 1 ∞ RAD` | `Y×SIN(Z)÷(SIN(Z)-Y×SIN(Z)^2)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); Z≠0; ABS(Y×SIN(Z))<1` | Y at Z = π |
| `casim sum 'SIN(Z)^(2×K-1)×Y^K' K 1 N RAD` | `-Y×SIN(Z)÷(Y×SIN(Z)^2-1)-(Y×SIN(Z)^2)^(N+1)÷(SIN(Z)-Y×SIN(Z)^3); Z≠0; Y×SIN(Z)^2≠1` | 0 at Z = π, every N ≥ 0 |
| `casim sum 'TAN(Z)^(2×K-1)×Y^K' K 1 N RAD` | `-Y×TAN(Z)÷(Y×TAN(Z)^2-1)-(Y×TAN(Z)^2)^(N+1)÷(TAN(Z)-Y×TAN(Z)^3); Z≠0; Y×TAN(Z)^2≠1` | 0 at Z = π |
| `casim sum 'K×COS(Z)^(K-1)' K 1 ∞ RAD` | `COS(Z)÷(COS(Z)-COS(Z)^2)-COS(Z)÷2÷(COS(Z)-COS(Z)^2÷2-1÷2); Z≠0; ABS(COS(Z))<1` | 1 at Z = π÷2 |
| `casim sum 'K×LN(Z)^(2×K-1)×Y^K' K 1 ∞ RAD` | `Y×LN(Z)^2÷(LN(Z)-Y×LN(Z)^3)-Y^2×LN(Z)^3÷2÷(Y×LN(Z)^2-Y^2×LN(Z)^4÷2-1÷2); Z≠0; ABS(Y×LN(Z)^2)<1` | 0 at Z = 1 |

Each one's rational twin (SIN(Z) read as X) gets the shift and has a value at X = 0.

## The class

**The missing-value probe** (casim's, ~/scratch/casim/c27/probe-nv.py, the third test beside CAS 026's factor probe's
two): at each zero of the summand's factor F (SIN and TAN at 0 and π, COS at π÷2, LN at 1, SQRT, ABS and Z^(1÷3) at 0),
F put to 0 exactly in the text and Z to its value elsewhere, Y at 1÷3 and −1÷4, N from lo−1 to lo+5 (to N): a missing
value is a point where
1. the sum has a value: every term from lo to N (to ∞: lo to lo+12, the rest 0 there), merged (K×SIN(Z)^(K−1) at K = 1
   is 1);
2. no row of the answer that has a value there is false;
3. the answer's first part has no value there (abacus #9727: no value when any part has none).

Over CAS 026's two probe sets, 640 sums: SIN, COS, SQRT, ABS, Z^(1÷3), EXP and COSH × 8 shapes × lo −1..2, to N and to
∞ (448), and SIN, COS, TAN, LN, SQRT, ABS, Z^(1÷3) and EXP at powers 2 and 3 with an extra power, lo 0 and 1 (192).
TAN and LN are only in the second.

- **1.0.0: 65 of the 462 answered have a missing value.** 62 are the class: SIN 23, COS 23, TAN 8, LN 8, each at a zero
  other than its letter's 0. 3 are at the letter's own 0: `(F×Y)^K×F` from −1 to N at N = −2, F one of SQRT(Z), ABS(Z)
  and Z^(1÷3), the function-factor members of 1.0.0's class (a). rc6 has the same 65.
- **The rational twins:** each of the 65 with its factor read as X, judged at X = 0 the same way: none has a missing
  value. 41 have one at X = 0, and 24 exclude it with `X≠0` (c28/twin.py). Controls: class (a)'s
  `X^(K+2)×Y^(-2×K)` from −2 to N fires at N = −3 and −2; `K×X^(K-1)` from 1 to ∞ is quiet.

## Rules (proposed)

1. **The shift reads a function factor.** sm_shift treats a factor of B that holds a letter and isn't a letter at a whole
   power as h^m, with m a whole power from 1 to SM_DEG: SIN(Z), COS(Z)^2, TAN(Z), LN(Z), SQRT(Z), ABS(Z), Z^(1÷3).
   s_h is the least s that clears q's negative powers of h, with q's power of h read as CAS 026 reads it (sm_powf's
   floor). With other letters in B, CAS 024's rule for two letters or more (#9572) picks s, h counting as one of them.
2. **Only where the shift clears it.** The lo must be whole and s ≤ lo, so the shifted range starts at 0 or above.
   Otherwise there is no shift and today's answer stands: B^(lo−s) would divide by h anyway, and the letter's `Z≠0`
   would replace CAS 026's `SIN(Z)≠0`. Measured without this rule (p2): `K×SIN(Z)^(K-1)` from 0 to ∞ became
   `SIN(Z)÷(SIN(Z)-2×SIN(Z)^2+SIN(Z)^3); Z≠0; ABS(SIN(Z))<1` (026-examples.txt line 17), and from −1 to N it changed
   form (line 36).
3. **Factors that are never 0 still stop it:** EXP, ALOG, COSH and a positive number to a power, as do a constant
   (SQRT(2)) and a negative power. `K×(EXP(Z)×X)^K÷X` from 1 to ∞ keeps `X≠0`.
4. **CAS 026's row test reads the shifted lo.** The test e + m×lo < 0 reads q after the shift, so it is asked at lo − s.
   Measured without this rule (p1): a negative shift from lo = −1, `(SIN(Z)×Y)^K×SIN(Z)` to N, gained `SIN(Z)≠0` for
   nothing, and so did four others.

## Measured (p4 over 671b8bf; ~/scratch/casim/wt-101p, patch c28/p4.patch; src/casim.c +33 −5)

- **Missing values: 65 → 24.** 41 sums get a value: 38 of the 62, and the 3 at the letter's own 0. They are the 41
  whose twins have a value at X = 0, and the 24 left are the 24 whose twins print `X≠0`, sum for sum: p4 answers each
  of the 65 as its twin does. The rows-for-nothing
  item in 1.0.0's notes says a letter in a function stops the shift; with this rule it doesn't. Of the 105 sums that
  move, `work/027-moves.txt` lists each with 1.0.0's answer and p4's:

  | What moved | Sums |
  |---|---|
  | a value gained at the factor's zeros (22 of them also drop `Z≠0`, `Y≠0` or both) | 41 |
  | a row dropped where the sum has a value, which p4's answer gives (`Z≠0` 28, `Y≠0` 10, both 4) | 42 |
  | TOO_LONG at 1.0.0, answered now (the shifted form is shorter): `K×SIN(Z)^(K-1)` from 1 to N | 14 |
  | the form only | 8 |

- **Values** (c28/agree2.py): at each answered sum's factor zeros (put to 0 exactly) and at Z = 1÷3 and 7÷5, Y at
  1÷3, −1÷4 and 0, N from lo−1 to lo+5, the answer against the sum term by term (to ∞, until the terms are below
  10^−70; 276 points that hadn't converged in 400 terms skipped). p4: 476 sums, 9,758 points agree, 0 disagree, 0 values
  where the sum has none. 1.0.0: 462 sums, 8,198, 0, 0. Controls: p4's answers moved by 1÷1000, all 2,900 points of
  the moved sums disagree (all 9,758 over the 476); its rows dropped, 208 values where the sum has none.
- **CAS 026's factor probe:** 0 missing, 0 factor rows for nothing, at 1.0.0 and p4, on both sets.
- **The letter probe** (343 sums): 0 moved; 0 missing, 20 rows for nothing, 0 wrong values on both.
- **make check's value checks**, run alone: golden 10,822, 0 failed; ratio 15 and conditions 35, 0 failed; sum-values
  536, 0 failed, its named counts falling (CAS 025's family: rows for nothing 7 → 5; CAS 026's: letter rows 12 → 6).
  Doc examples 1,654 checks, 4 failed: two commands on both routes, which the build rewrites:
  - work/026-examples.txt line 46, the class's own example, `K×(SIN(Z)×Y)^K÷SIN(Z)` from 1 to ∞, becomes
    `-Y÷(Y×SIN(Z)-1)-Y^2×SIN(Z)÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); ABS(Y×SIN(Z))<1`, which is Y at π.
  - work/025-examples.txt line 66, `K×(SIN(Z)×Y)^K÷Y` from 1 to ∞, drops `Y≠0`. The sum is SIN(Z) at Y = 0, and so is
    the answer.
- **CAS 026's oracle family, wide** (gen_letter4, 1,500 draws, seeds 20261101 and 20261102): p4, 1 failure and 0;
  1.0.0, the same 1 and 0. That failure is older than this unit: `((-1)×K)×(Z^(1÷3))^(2×K-2)×Y^(K-1)` from 0 to ∞
  prints the row `Z^(2÷3)≠0`, and the judge knows only `Z^(1÷3)≠0`. Both exclude only Z = 0, so it's not a wrong
  value. 026 measured 0 failures on its own seeds. The named counts fall: Y 89 → 36 and Z 151 → 86 on 20261101;
  96 → 45 and 158 → 96 on 20261102.
- **Frames:** host, all 421 as recorded, new sm_atom 144; S3, all 418 as recorded (sm_shift 128, sm_rowf 144, do_sum_
  656), new sm_atom 96. The first shape (p3: the atom read inside sm_shift) moved two host frames: sm_shift 176 → 208,
  and sm_rowf 240 → 192 once sm_powf had two callers. In p4, sm_powf is always inline, the atom's power is read in
  sm_atom, and sm_shift flags it in rowl's bit 31 and keeps the shift in bits 26–30 for sm_rowf.
- **S3 text: 241,853 B, +772 over 671b8bf's 241,081** (scripts/size-s3.sh, claim #9987). Host text +1,424 B (p3:
  +480, with sm_powf out of line).

## The 24 left

Each one is summed to N. The sum has a value at the factor's zero only for an N whose terms from lo don't divide by the
factor: the empty sum (N < lo), and `(Y÷SIN(Z))^K` up to N ≤ 0. Past that N a term divides by the factor, so the sum
has no value there (checked at N = lo+5 for all 24). They are:
- with F one of SIN(Z) and COS(Z), 16: `K×F^(K-1)` from −1 and 0, `(F×Y)^K` from −1, `(Y÷F)^K` from −1, 0, 1 and 2,
  and `(K-1)×(F×Y)^K÷F` from 0;
- with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z), 8: `F^(2×K-1)×Y^K` and `F^(3×K-1)×Y^K` from 0.

The shift can't reach them. After the shift the answer still divides by the factor for the larger N, as the sum needs,
and so it does at the smaller N too. A form with a value at the smaller N and none at the larger exists for some.
`(Y÷SIN(Z))^K` from 1 to N is `Y×(Y^N-SIN(Z)^N)÷(SIN(Z)^N×(Y-SIN(Z)))`, which is 0 at SIN(Z) = 0 for N = 0 and has no
value for N ≥ 1. casim doesn't write that form. At those N, the rational twin excludes X = 0 with `X≠0`, a row for
nothing. The function sum prints its letter's `Z≠0`, which doesn't exclude π.

## Questions

1. **The 24.** (a) They stay named in 1.0.1's notes, sharper than 1.0.0's item: to N, at an N before the first term that
   divides by the factor. Or (b) they get the factor's row `F≠0` in place of the letter's, as the twin has `X≠0`. Each
   would then be a row for nothing at those N, and CAS 026's rule 2 (decision 90 (3)) would change for them. My lean:
   (a). Then 1.0.1 only adds values and drops rows, and decision 90 stands. (b) would be Michael's.
2. **What comes with it.** The same rule drops 42 rows for nothing and answers 14 sums that were TOO_LONG. My lean: they
   are in scope, judged by value like the 41. Without them the rule would need code to keep a row the answer doesn't
   need.
3. **Not in:** the shift past a constant (SQRT(2)) or a factor that is never 0 (EXP(Z)). Both are the other halves of
   1.0.0's rows-for-nothing item, and sm_atom could take them as factors that never move s. My lean: after 1.0.1, as
   they print no wrong value and no missing one.
4. **The S3 stop:** +1 KB (p4 is +772).

## The oracle

- sum-values.py's fourth family gains the missing-value judge: at F's zeros, where the sum has a value and no row with a
  value is false, the answer must have one, and it must equal the sum. Its planted answers first: 1.0.0's for the class
  (a missing value, refused), and a value moved. TAN and LN are drawn as F at powers 1 to 3, as the others are.
- work/027-examples.txt: the table above with p4's answers, the 24's shape, rule 2's two kept forms, and EXP's `X≠0`.
- **Mutants:** the factor not read (sm_atom returning −1); rule 2 off (a shift past lo); rule 4 off (S->a in place of
  lo − s); s read from the wrong bits; a never-0 factor read as one with zeros; sm_powf's floor read as a ceiling.
- S3 text and frames, and the device bench at 16,384 B.

## Cost

S3 text +772 B (stop +1 KB proposed); host +1,424 B. No existing frame moves on the host or the S3.

## Ruled (#10004)

1. **The 24 stay named** (a), sharper than 1.0.0's item, and decision 90 (3) stands. abacus's judge names a missing value
   only in that shape: to N, at an N where the sum has a value, with no value at the same point for a larger N, and
   1.0.0 missing it too. A missing value anywhere else fails.
2. **The 42 dropped rows and the 14 TOO_LONG answered are in scope,** judged by value.
3. **The shift past a constant or EXP comes after 1.0.1,** on the follow-up list.
4. **The S3 stop is +1 KB.** 241,853 B leaves 20,291 under decision 80's 262,144.
5. **The gen_letter4 judge is fixed in this unit, test-only:** a row whose zero set equals a known one reads as that row
   (`Z^(2÷3)≠0` as `Z^(1÷3)≠0`), so the wide family runs green.

