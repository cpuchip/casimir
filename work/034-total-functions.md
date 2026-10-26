# CAS work unit 034: a row whose other letters are inside a function with a value everywhere

**From:** casim · **To:** abacus · **Status:** built, for h034 (abacus #11253: "Yes, CAS 034, no card"; the blueprint,
#11278, OK'd for code at #11279 with four additions, each below). The build is over main da1b7f7 (1.3.0 as published,
with ratio025-negpow-mono-row retargeted). casim writes the unit; abacus writes the hidden set and the Accepted section.
- **From:** 1.3.0's Not yet, "A redundant row the rule doesn't reach": `(1+ABS(Y)×Z)^K` from 0 to N keeps `Z≠0` beside
  `Z×ABS(Y)+1≠1`, and `(1+Z×Y)^K` from 0 to ∞ prints `Y≠0; Z≠0; ABS(Y×Z+1)<1`.
- **The question:** Michael's cas-032-rows card promised a row is dropped "only where a kept row provably excludes the
  same point at every value of the other letters" (abacus #11253). CAS 032 kept that promise by allowing the other
  letters only in a sum, a product or a whole power. A function with a value at every real input keeps it as exactly,
  so this is his (b) carried out, not overturned (abacus records it on his card, where he can veto it).

## Where it is today (da1b7f7)

sm_whole (CAS 032) passes a row's l only where every letter but L is in a polynomial place, so that its value at L = 0
is one constant whatever theirs are. `Z×ABS(Y)+1≠1` holds Y inside ABS and says nothing, nor does `COS(Y×Z)≠1` or, to
∞, `ABS(Z×G+1)<1` for any G: h032's reading made them "lawful, unchanged" (work/032-redundant-row.md).

## The change

**In sm_whole only: a function from the closed list counts as a polynomial place, its argument checked the same way.**
sm_false0 is unchanged: put L = 0, need a letter-free result with a value, and judge it.

**Why it is exact.** Every node sm_whole accepts has a value at every real value of the other letters: sums, products,
whole powers not below 0, and the listed functions of those. So canon's rewrites of r's l at L = 0, 0×u = 0 among them,
keep its value at every point, and a letter-free result is the value r's l takes at L = 0 for every value of the
others: the card's sentence.

**The closed list,** each with a value at every real input, in RAD, DEG and GRAD alike:
- ABS: |x|.
- SIN, COS: the input scaled to radians, then its sine or cosine.
- EXP: e^x; ALOG: 10^x.
- SINH, COSH: (e^x ∓ e^-x)÷2.
- TANH: (e^x − e^-x)÷(e^x + e^-x), whose divisor is above 0.
- ATAN: onto ±π÷2, scaled for the mode.
- ASINH: LN(x+√(x²+1)), where x+√(x²+1) > 0.

**Left out,** each without a value somewhere: TAN (π÷2+kπ), LN and LOG (x ≤ 0), ASIN and ACOS (|x| > 1), ACOSH (x < 1),
ATANH (|x| ≥ 1). Division, INV and SQRT never reach the list: canon writes them as a power, -1 or 1÷2, and the
whole-power test refuses both (so total034-sqrt-in lets the half power in, not F_SQRT). Not claimed: SQ (canon writes
it as ^2, a polynomial place already), SGN (casim makes it only in a derivative; a typed SGN is an opaque name), and
opaque names, which stay out like any function not on the list. A function off the list that holds only L is not
another letter's: `ABS(Z×LN(Y+1)+1)<1` is 1<1 at Y = 0 whatever Z is, so `Y≠0` goes (and `Z≠0` stays).

**The cap** (abacus #11279 (3)). A row that gives way at the cap ('?', CAS 030's) is a function of one letter (SIN, COS,
TAN, LN, LOG, ACOS or ACOSH of a K_VAR), so CAS 034 never asks about it and CAS 032's give-way is unchanged: in
`(2×Y÷SIN(Z))^K+(COS(Z)÷2)^K` from -2, `SIN(Z)≠0` says `Z≠0`, gives way (196 B with it), and `Z≠0` comes back. Where
CAS 034's drops make room, a '?' row is kept (`(2×Y÷SIN(Z))^K+COS(W×Z)^K` from -2: `W≠0` and `Z≠0` said by `COS(W×Z)≠1`,
and `SIN(Z)≠0` fits), and a line TOO_LONG in 1.3.0 fits (`(2×Y÷SIN(Z))^K+(1+ABS(W)×Z)^K` from -1, 191 B).

| Sum | 1.3.0 | CAS 034 |
|---|---|---|
| `(1+ABS(Y)×Z)^K`, 0 to N | `…; Z≠0; Z×ABS(Y)+1≠1` | `…; Z×ABS(Y)+1≠1` |
| `(1+ABS(Y)×Z)^K`, 0 to ∞ | `-1÷Z÷ABS(Y); Y≠0; Z≠0; ABS(Z×ABS(Y)+1)<1` | `-1÷Z÷ABS(Y); ABS(Z×ABS(Y)+1)<1` |
| `(1+Z×Y)^K`, 0 to ∞ | `-1÷Y÷Z; Y≠0; Z≠0; ABS(Y×Z+1)<1` | `-1÷Y÷Z; ABS(Y×Z+1)<1` |
| `COS(Y×Z)^K`, 0 to N | `…; Y≠0; Z≠0; COS(Y×Z)≠1` | `…; COS(Y×Z)≠1` |
| `(ATAN(Y×Z+1)÷45)^K`, 0 to N, DEG | `…; Y≠0; Z≠0; ATAN(Y×Z+1)÷45≠1` | `…; ATAN(Y×Z+1)÷45≠1` |
| `(1+TAN(Y)×Z)^K`, 0 to N | `…; Z≠0; Z×TAN(Y)+1≠1` | unchanged |
| `(1+ABS(TAN(Y))×Z)^K`, 0 to N | `…; Z≠0; Z×ABS(TAN(Y))+1≠1` | unchanged |

## Measured (the build against 7efd2af's CLI)

- **The 640** (c25's fn-rows-all and fn-pow-rows) at 65,536 and 16,384 B: 0 of 1,280 calls move. **probe030's family:**
  0 of 5,280. Neither holds a function of one letter in a product with another.
- **CAS 034's family** (8 shapes, (1+F(Y)×Z)^K and its kin, × 21 functions: the list, SQ and SGN, and the ones left out
  with LOG; lo -1 to 2, to N and ∞; and 6 two-letter polynomial ratios: 1,392 sums): 934 of 2,784 calls move at 65,536
  and 16,384 B, 717 at 4,096 and 2,048 B, 0 bad. The judge is said032's (each drop said by a kept row false at L = 0
  with a value, the other letters at generic and at special values: 0, ±1, ±2, 1÷2, ±π÷2, π), with ALOG, SGN and SQ
  known to its evaluator: without them it called 64 right ALOG drops bad, the instrument's gap. 1,238 rows dropped, by
  the function holding the other letters: ABS 870 (most of them to ∞), SIN, SINH, ATAN, TANH and ASINH 48 each, COS,
  EXP, COSH and ALOG 32 each; a function off the list appears in a sayer only holding L alone. TAN let in: 990 move, 48
  bad, every one TAN's. The build and the throwaway prototype (whose list also held SQ and SGN) print the same family.
- **The golden corpus:** 0 of 10,822 calls move. **The doc examples:** 3 rows, work/032-examples.txt 74 to 76, CAS
  032's "lawful, unchanged" pins, re-pinned in their own commit (abacus #11279 (4)).
- **Frames:** the host table's 436, none moved. The S3's, on the prototype (claim #11272): 433, none moved, against
  7efd2af's.

## The oracle

- **work/034-examples.txt:** 46 rows. Each listed function once; SIN, COS and ATAN in DEG and GRAD, and rows whose
  constant at the letter's 0 is the mode's own (ATAN(1) is 45 in DEG, 50 in GRAD; SIN(90), COS(60), SIN(100), COS(100)),
  with the same summands in RAD, which have no letter rows (abacus #11279 (2)); a listed function of a listed function,
  ABS(SIN(Y)) and EXP(COS(W×Y)) (#11279 (3)); every function left out, and a listed one around such an argument; the
  three cap rows above.
- **scripts/sum-values.py, a CAS 034 block** (seed 20261034; 60 seeded, 14 held): (1+Z×F(P))^K with P Y's polynomial,
  from 0 or 1, to N or ∞, in each angle mode, judged two ways. By the rule: `Z≠0` kept just where F is off the list.
  By the sentence: where `Z≠0` is gone, a kept row is false at Z = 0 with a value at each of 12 special Y (negatives
  for SQRT's hole, π÷2, 90 and 100 for TAN's in each mode), TAN given no value where its cosine is 0 to 40 digits (the
  evaluator's 90-digit π otherwise gives it one near 1E90). To N, the first part is the running sum. The planted answers
  first: 1.3.0's `Z≠0` beside `Z×ABS(Y)+1≠1` (NOT_SAID), and drops beside `Z×TAN(Y)+1≠1` and `SQRT(Y)×Z+1≠1`, refused
  by the rule and, judged as if said, by the sentence. The evaluator gains ALOG. CAS 032's block follows the rule: its
  ABS(Y), EXP(Y) and SIN(Y), its rows to ∞ and three held rows say `Z≠0` now. Falsified on three CLIs: 1.3.0's fails 63
  (42 CAS 034's, 21 CAS 032's), TAN let in 7, the half power let in 4.
- **Mutants** (482): total034-off (no function counts), total034-arg-unchecked (a listed function taken without its
  argument's check), total034-tan-in (abacus #11253), total034-sqrt-in (the half power, #11279 (1): its fault shows only
  below 0), total034-atan-out (ATAN dropped from the list).
- **scripts/measure.py:** 5 "total …" bench rows, the cap's among them (1,399 cases).
- **docs/language.md:** the sums paragraph names the list.

## Asked

- abacus: h034's hash before this pushes (#11279).
