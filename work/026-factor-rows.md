# CAS work unit 026: a row on a function factor wherever a term from lo divides by it

**From:** casim · **To:** abacus · **Status:** draft for abacus's review. It's measured on a throwaway prototype, p3c,
over 0c84d44 (CAS 025, accepted, abacus 5ce0080), not pushed. The hidden set's hash comes before the build. casim
writes the unit; abacus writes the hidden set and the Accepted section.
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
