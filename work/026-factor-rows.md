# CAS work unit 026: a row on a function factor wherever a term from lo divides by it

**From:** casim · **To:** abacus · **Status:** built as ruled (#9720, #9726: p3d2), for abacus's review against h026.
The draft below was measured on prototype p3c over 0c84d44 (CAS 025, accepted, abacus 5ce0080); Ruled, As built and
Measured follow it. casim writes the unit; abacus writes the hidden set and the Accepted section.
- **Why now:** Michael's decision 90 (abacus #9702, 7a80db0). The class prints a value where the sum has none, the
  same kind as decision 89's item, so it's fixed before 1.0.0, as its own unit after 025; rc7 comes after it.
- **The question:** when a term of a sum from lo divides by a factor of its ratio that holds a letter but isn't a letter
  at a whole power (SIN(Z), SQRT(Z), ABS(Z)), does the answer exclude where that factor is 0?

## Where it is today

CAS 024 and 025 give a row to each letter of B a term from lo divides by, and rule 4b's probe asks the answer whether it
has a value at a letter's 0. Neither sees a factor's own zeros: COS(Z) is 0 at π÷2 and SIN(Z) at π, not at a letter's
0. With a zero coefficient where the term divides (condition 2, #9563: 0×SIN(Z)^(−1) has no value where SIN(Z) = 0),
the answer has a value even at Z = 0. Found by 025's oracle family (#9698), the same at e551d3e:

| Command (RAD) | Prints at 0c84d44 (025) |
|---|---|
| `casim sum 'K×(SIN(Z)×Y)^K÷SIN(Z)' K 0 ∞ RAD` | `-Y÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); ABS(Y×SIN(Z))<1` |
| `casim sum 'K×(COS(Z)×Y)^K÷COS(Z)' K 0 ∞ RAD` | `-Y÷2÷(Y×COS(Z)-Y^2×COS(Z)^2÷2-1÷2); ABS(Y×COS(Z))<1` |
| `casim sum 'K×(SQRT(Z)×Y)^K÷SQRT(Z)' K 0 ∞ RAD` | `-Y÷2÷(Y×SQRT(Z)-Y^2×Z÷2-1÷2); ABS(Y×SQRT(Z))<1` |
| `casim sum 'K×SIN(Z)^(K-1)' K 0 ∞ RAD` | `-1÷2÷(SIN(Z)-SIN(Z)^2÷2-1÷2); ABS(SIN(Z))<1` |
| `casim sum '(Y÷SIN(Z))^K' K 1 ∞ RAD` | `-Y÷(Y-SIN(Z)); ABS(Y÷SIN(Z))<1` |
| `casim sum '(1+K)×(-1)^K×SQRT(Z)^K×X^(K+1)×Y^(K-1)' K -1 ∞ RAD` | `X÷(Y+2×X×Y^2×SQRT(Z)+X^2×Y^3×Z); Y≠0; ABS(-X×Y×SQRT(Z))<1` |

Each answer has a value where its factor is 0, and its ABS row holds there (ABS(0) < 1).

## Rules (proposed)

1. **A row on the factor.** For each factor h of B that holds a letter and isn't a letter at a whole power, with B
   holding h^m (a negative fraction power of a letter, Y÷SQRT(Z), read as the root to −m): where a term from a whole lo
   divides by h (m < 0, or e + m×lo < 0 with e the least power of h among q's nonzero coefficients, K×(SIN(Z)×Y)^K÷SIN(Z)
   having q = K÷SIN(Z)), the row `h≠0` stands.
2. **Unless the answer has no value there already** (decision 90 (3)), as the probe asks for a letter: a root or ABS of
   a letter is 0 where the letter is, so the answer is asked at that letter's 0 (it may hold Z beside SQRT(Z)); any
   other factor is replaced in the answer by a letter in nothing else, asked at 0.
3. **Factors that are never 0 get no row:** EXP, ALOG, COSH and a positive number to a power (2^Z).
4. **Scope** (decision 90 (5)): read are SIN, COS, TAN, SQRT and the other roots of a letter, ABS and LN, and their
   powers. A factor that is a sum (Z^2+1, Z−1) stays NO_CLOSED_FORM, as today. A symbolic lo keeps today's rows. A
   redundant Z≠0 beside SIN(Z)≠0 stays (decision 90 (4)); with rule 2 it seldom arises.

## Measured (p3c over 0c84d44, ~/scratch/casim/wt-026, patch c25/p3c.patch; src/casim.c +73)

- **The factor probe** (c25/probe-f.py, new): at each zero of a sum's factor (SIN at Z = 0 and π, COS at π÷2, SQRT,
  ABS and Z^(1÷3) at 0), the factor put to 0 exactly in the text (the reader's SIN(π) isn't 0) and the other letters at
  1÷3 and −1÷4. Missing: the sum has no value there but the answer claims one (rows by ruling 2). For nothing: an `h≠0`
  where the sum has a value at every zero, grid point and N. Over 448 sums (SIN, COS, SQRT, ABS, Z^(1÷3), EXP, COSH, 2^Z
  × 8 shapes × lo −1..2 × to N and to ∞):
  - 0c84d44: 15 missing (e551d3e: the same 15), 0 for nothing;
  - p3c: 0 missing, 0 for nothing. It moves 15 sums, all to ∞, each gaining its row.
  - Controls: p3c with its factor rows dropped, 15 missing; with `EXP(Z)≠0` and `COS(Z)≠0` added, 51 for nothing.
- **The letter probe** (c25/probe.py): its 343 sums, 0 moved. The function set at Z = 0: missing 12 → 0, rows for
  nothing 53 → 53 (none new), wrong values 0.
- **p3's 8 refusals** (abacus #9701, decision 90 (3)): each rc6 line was right, its answer with no value at the factor's
  0 at every N where the sum has none (c25/p3-toolong-eval.txt). p3c keeps all 8 byte for byte as 0c84d44 prints them:
  `K×SIN(Z)^(K-1)`, `K×COS(Z)^(K-1)` and `K×ABS(Z)^(K-1)` from −1 to N; `(K-1)×(SIN(Z)×Y)^K÷SIN(Z)`, its COS and ABS
  twins, `K×(SQRT(Z)×Y)^K÷SQRT(Z)` and `K×Z^(1÷3)^(K-1)` from 0 to N.
- **The golden:** 10,822 checks, 0 moved.
- **Frames:** host, all 419 as recorded (decision 90 (1)); new: sm_rowf 240, ib_at 32 (gcc no longer inlines it).
  sm_rowf is called once per group in a loop of its own before sm_rows' group loop: called inside that loop, do_sum_
  went 816 → 832, and once after it with seven arguments, 848. S3: no frame moved (do_sum_ 656, sm_shift 128,
  sm_groups 112); new sm_rowf 144, ib_at 32.
- **S3 text: 240,818 B, +1,238 over 0c84d44's 239,580** (the stop is +1.25 KB; 42 B under it). sm_rowf is 1,220 B,
  do_sum_ +32, ib_at 68 against 132 less in ib, which inlined it. Host text +2,176 B. p3b, the first shape, was +1,620:
  its 64-bit exponent ratio, a search for a free letter (K, the sum's own letter, is in no answer) and a per-factor
  zero test went.

| Command (RAD) | Prints with p3c |
|---|---|
| `casim sum 'K×(SIN(Z)×Y)^K÷SIN(Z)' K 0 ∞ RAD` | `-Y÷2÷(Y×SIN(Z)-Y^2×SIN(Z)^2÷2-1÷2); SIN(Z)≠0; ABS(Y×SIN(Z))<1` |
| `casim sum 'K×(COS(Z)×Y)^K÷COS(Z)' K 0 ∞ RAD` | `-Y÷2÷(Y×COS(Z)-Y^2×COS(Z)^2÷2-1÷2); COS(Z)≠0; ABS(Y×COS(Z))<1` |
| `casim sum '(Y÷SQRT(Z))^K' K 1 ∞ RAD` | `-Y÷(Y-SQRT(Z)); SQRT(Z)≠0; ABS(Y÷SQRT(Z))<1` |
| `casim sum 'K×(LN(Z)×Y)^K÷LN(Z)' K 0 ∞ RAD` | `-Y÷2÷(Y×LN(Z)-Y^2×LN(Z)^2÷2-1÷2); Z≠0; LN(Z)≠0; ABS(Y×LN(Z))<1` |
| `casim sum 'SIN(Z)^K×Y^K' K -1 ∞ RAD` (rule 2: no row added) | `1÷(Y×SIN(Z)-Y^2×SIN(Z)^2); Y≠0; Z≠0; ABS(Y×SIN(Z))<1` |
| `casim sum 'K×(EXP(Z)×Y)^K÷EXP(Z)' K 0 ∞ RAD` (rule 3) | `-Y÷2÷(Y×EXP(Z)-Y^2×EXP(Z)^2÷2-1÷2); ABS(Y×EXP(Z))<1` |

## Questions

1. **A factor's own domain.** TAN(Z) has no value at π÷2 and LN(Z) none at Z ≤ 0. The answer holds the factor, so it
   has no value there either, and no row is added for that. My lean: nothing to add; the oracle checks it.
2. **Rule 2's replacement is conservative.** An answer that holds the factor's argument elsewhere (a Z beside SIN(Z))
   keeps that Z free when the factor is put to 0, so it can show a value the answer doesn't have and the row is added.
   That's a row more, never one missing. My lean: keep it, and count such rows in the oracle.

## The oracle

- sum-values.py's fourth family: q(K)×c^K×F^(a×K+e)×Y^(b×K+f), F from SIN(Z), COS(Z), TAN(Z), SQRT(Z), Z^(1÷3), ABS(Z),
  LN(Z), EXP(Z), COSH(Z), 2^Z, lo −2..3, to N and to ∞. Rows are judged at F's zeros with F put to 0 exactly: none
  missing, and `F≠0` for nothing fails. Values are judged off the zeros. Letters at 0 are judged as in 025's family.
  Planted answers first: a factor row dropped, one for nothing, a value moved.
- work/026-examples.txt: the tables above, the 8 kept, and EXP's no row.
- **Mutants:** the factor row off; rule 2's skip off; rule 3's skip off; the exponent ratio off (SQRT(Z) against
  Z^(−1÷2)); a negative fraction power read as positive; the root's letter probe replaced by the generic one.
- S3 text and frames; the device bench at 16,384 B with the factor rows.

## Cost

S3 text +1,238 B (stop +1.25 KB), host +2,176 B; no existing frame moves on either.

## Ruled (#9720, #9726)

1. **A factor's own domain adds nothing:** where TAN or LN has no value, the answer holding it has none either.
2. **A redundant row is kept, if it never excludes a point where the sum has a value.** A Z≠0 the sum doesn't need,
   where Z = 0 isn't a zero of the factor, would be a new row for nothing (casim #9724: the build makes no Z≠0 of its
   own; the letter probe's are 0c84d44's).
3. **p3d2, with the S3 stop at +1.5 KB** (abacus's to set, inside decision 80's budget). The merged root (a root at a
   power canon merges into its letter, SQRT(Z)^(2×K−1)×Y^K, q holding Z^(−1÷2)) prints a value where the sum has
   none, so it is fixed, not named: the letter path, from lo ≥ 0 only, negative powers staying 025's, no frame moved.
   sm_powof stays as it is (the floor folded into it moved sm_shift's host frame 176 → 192).

h026 was frozen before the build (#9727): sha256 2f6db4f3e886b2b3aee9c8dda1cac1b68c0183f9694f82e39ac92e8d4fc9e5b0, 98
rows, against 0c84d44. Its judging rule: an answer has no value at a point when any part of it has none
(−Y÷(Y−TAN(Z)) at Z = π÷2 is an error, as on the calculator), and so has the sum wherever a factor has none.

## As built (where the code differs from the rules, or settles what they left open)

- **sm_powf** reads q's power of the factor as the ratio of exponents, rounded down: e + m×lo < 0 exactly when
  floor(e) + m×lo < 0, since m×lo is whole. So SQRT(Z)^(3×K−1)×Y^K, whose B holds Z^(3÷2) and q Z^(−1÷2), reads −1.
- **sm_rowf** runs per group, in a loop of its own before sm_rows' group loop. Inside that loop do_sum_'s host frame went
  816 → 832, and called once after it with seven arguments, 848. The probe needs no free letter: K, the sum's own
  letter, is in no answer. The never-zero test is a mask over EXP, ALOG and COSH.
- **The merged root's letter path** (#9726): a letter of B at a positive whole power takes the row its q's fraction
  powers need, printed as the letter's row (`Z≠0`, sm_row keeping it once beside 024's). From lo ≥ 0 only: there 024's
  shift raises q's powers of the letter, so a row read from a shifted q is one a term from lo needs.
- **The oracle:** sum-values.py's fourth family (60 seeded, seed 20261026, and 10 held; planted answers first:
  0c84d44's, a row on EXP(Z), a value moved, and the merged root without its row, refused). Each factor is judged at
  its own zeros with the factor put to 0 exactly. The reader reads ABS and COSH now; the other families' 466 checks
  hold.
- **work/025-examples.txt:** its two named rows (÷SIN(Z), ÷SQRT(Z)) gain their factor rows, and its note says so.

## Measured (the build, over 0c84d44)

- **The factor probe** (c25/probe-f.py; each factor at its own zeros, put to 0 exactly): the function set, 448 sums,
  0c84d44 15 missing, the build 0; a power set, 192 sums (8 factors at m = 2 and 3, with an extra power, lo 0 and 1, to
  N and to ∞), 0c84d44 27 missing, the build 0. Factor rows for nothing: 0 on both. The build moves 15 and 27 sums.
- **The letter probe:** its 343 sums, 0 moved; the function set at Z = 0, missing 12 → 0, rows for nothing 53 → 53, the
  same rows, wrong values 0. The 8 to N that p3 refused print byte for byte as 0c84d44.
- **The oracle family, wide** (gen_letter4, 2 seeds × 1,500 draws, on p3d2, this src): 0 failures; 0c84d44 44.
- **The golden:** 10,822 checks, 0 moved; →DEC, complex and the series route compiled out the same.
- **make check** green (3:16, 235,736 KiB peak): frames as recorded (421, the two new ones recorded), the 428 mutant
  anchors, sum-values 536 checks (60 + 10 of 026's), doc examples 821 commands (1,654 checks). **make test** green:
  20 tests, 118,421 checks (4:16, 236,104 KiB).
- **S3 text: 241,085 B, +1,505 over 0c84d44's 239,580** (the stop +1.5 KB, 31 B under it; 21,059 left of decision 80's
  262,144). S3 frames: none moved (sm_shift 128, sm_groups 112, do_sum_ 656); new sm_rowf 144, ib_at 32. Host text
  +2,624 B.
