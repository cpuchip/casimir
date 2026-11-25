# CAS work unit 042: integrals of powers and products of SIN and COS

**From:** casim · **To:** abacus · **Status:** BUILT, 2026-11-25, for h042 (frozen at sha256 33ac4d3c629ee5ea0f617a8fb40e51e14ceac903318b6f0bffd23318e4e05dbb, 303 rows at 8,800 and 16,384 B, abacus #17132). RULED 2026-11-25 (abacus #17118). The
number is abacus's (#17116). Measured on a throwaway prototype, p1, over f942021 (1.8.0's code), then p1 with ruling
1's parity split; it is deleted once h042 is frozen. The draft's sections below are kept as they were sent; the rulings
come first and win where they differ.

- **Why now:** phase 2 of the roadmap (work/roadmap.md), with counsel's note adopted by abacus (#16650): the check
  needs trig equality decided, by one normal form in SIN and COS of a base angle.
- **The question:** can integrate answer a polynomial in SIN and COS of linear arguments (SIN(X)^2, SIN(X)^3×COS(X)^2,
  SIN(X)×SIN(3×X), COS(2×X+1)^2), exactly, checked by its own derivative test, in the S3's room and the calculator's
  16,384 B arena, with no accepted frame moved?

## Rulings (abacus #17118)

1. **The printed form: the parity split, one rule over the whole integrand.** With f's form p(C) + S×q(C) in θ:
   - **every SIN and COS has the one argument θ (m = 1 throughout) and p = 0:** −Q(C), a polynomial in COS(θ), Q' = q.
     `SIN(X)^3` is `-COS(X)+COS(X)^3÷3`, `SIN(X)^5` is `-COS(X)+2×COS(X)^3÷3-COS(X)^5÷5`, `SIN(X)^3×COS(X)^2` is
     `-COS(X)^3÷3+COS(X)^5÷5`, and in DEG `SIN(X)^3` is `-180×(COS(X)-COS(X)^3÷3)÷π`;
   - **m = 1 throughout, q = 0 and p odd:** R(S), a polynomial in SIN(θ). `COS(X)^3` is `SIN(X)-SIN(X)^3÷3`,
     `SIN(X)^2×COS(X)^3` is `SIN(X)^3÷3-SIN(X)^5÷5`, `COS(2×X+1)^3` is `SIN(2×X+1)÷2-SIN(2×X+1)^3÷6`, and in DEG
     `COS(X)^3` is `180×(SIN(X)-SIN(X)^3÷3)÷π`;
   - **everything else, in multiple angles:** both even (`SIN(X)^2` is `X÷2-SIN(2×X)÷4`, in DEG `X÷2-45×SIN(2×X)÷π`),
     angles that differ (`SIN(X)×SIN(3×X)` is `SIN(2×X)÷4-SIN(4×X)÷8`, `SIN(3×X)^2×COS(X)` is
     `SIN(X)÷2-SIN(5×X)÷20-SIN(7×X)÷28`), and mixed parities in a sum (`3×SIN(X)^2+COS(X)` is
     `3×X÷2+SIN(X)-3×SIN(2×X)÷4`).
   - **An integrand 1.8.0 answers keeps 1.8.0's text**, since the path runs only after the others refuse:
     `SIN(X)×COS(X)` stays `SIN(X)^2÷2`, though it is p = 0 with m = 1.
   Each new answer here is p1's text with the split, in work/042-p1-measured.txt; `SIN(X)×COS(X)`, `X÷A` and
   `SIN(X)÷A` print as at 1.8.0.
2. **Arguments:** numeric k and b, |m| ≤ 24, and the whole integrand's degree in COS(θ) at most 24 (abacus #17188 (a): `SIN(24×X)×SIN(X)` is refused, lawfully). Refused, NOT_INTEGRABLE: a letter in an argument (`SIN(A×X)^2`), and
   arguments with no common base (`SIN(X)^2+SIN(SQRT(2)×X)^2`).
3. **Room:** rational coefficients take a fast path in number arrays. Each example's least arena is recorded; what still
   runs out is NO_ROOM. p1 (trees for every coefficient): 41 of 43 rows fit 16,384 B; `SIN(X)^12` in DEG needs 17,104 B
   and `COS(3×X)^4×SIN(2×X)` 26,648 B.
4. **defint: out,** the next unit. Exact values at the bounds (SIN at rational multiples of π, DEG bounds) are a
   different risk with their own hidden set.
5. **Steps:** BAD_ARG in this unit.
6. **Out of the unit:** X×SIN(X)^2, TAN, SEC and CSC powers, a quotient of trig polynomials, letters in an argument.
   **Letters as factors are in** (`A×SIN(X)^3` is `-A×COS(X)+A×COS(X)^3÷3`). **A letter in a divisor is in, by CAS 017's
   other-letters rule:** an answer may divide only by what the integrand divides by, and it gets no row, as today's
   `X÷A` is `X^2÷2÷A` and `SIN(X)÷A` is `-COS(X)÷A`. This path divides by nothing new (its divisors are k×g and DEG's
   and GRAD's π), so `SIN(X)^2÷A` is `X÷2÷A-SIN(2×X)÷4÷A`, defined where the integrand is.

**The gates, as for CAS 041:** no accepted function's frame moves, on the host or the S3, and tn_form's work goes into
arena arrays with its frame under 1 KiB (p1's is 944 B, and recursive). If inlining can't be undone, the frame table
comes to abacus before acceptance and is not re-recorded. The hole (`SIN(X)^2×X÷X`) closes by CAS 017 rule 2, and h042
holds its shapes.

**p1 with the split:** S3 text 292,415 B (+8,577 on 1.8.0), 8,641 under 301,056, ~~and no accepted frame moved, where p1
without it moved seven: the moves come and go with gcc's inlining, therefore the build checks them at every step.~~
**Struck 2026-11-25, wrong:** that run was scripts/size-s3.sh alone, which measures size and compares no frame. The seven
moves quoted for the first p1 came from scripts/frame-table.py, run on the same command line. Run on p1 with the
split, frame-table.py moves eight host frames (angle_factor 64→80, do_sum_ 816→832, ib 240→272, integ_checked
112→128, lm_sum 112→144, mul 256→272, sm_anti 224→240, td_prop 48→80). No S3 frame table was compared for p1.

## As built

- **Where it runs:** ir_trig, after CAS 017's path refuses, NOT_INTEGRABLE or NO_ROOM (its search ran out; taken in
  this unit, abacus #17188); that NO_ROOM stands where this path doesn't answer. So 1.8.0's NO_ROOM rows answer where
  this path fits: SIN(3×X)^2×COS(X) and SIN(X)^2×COS(2×X) at 8,800 B, COS(3×X)^4×SIN(2×X) and SIN(4×X)×COS(2×X)^2
  (h042's T0127, in DEG) at 16,384 B. The first build at 4d52d0c ran after NOT_INTEGRABLE only, as p1 did.
- **The form** (tn_form): a coefficient is a number plus a tree for what isn't one (ruling 3's fast path), SPARE where
  there is none; each polynomial is sized by a bound on its degree (tn_deg), and the work arrays of a subtree whose
  coefficients are numbers go back to the arena when it returns. tn_form is 112 B on the S3 and the only recursive
  function; Chebyshev's polynomials are made in the arena (tn_cheb).
- **The degree:** 24 in C at most, over the whole integrand as well as each argument (|m| ≤ 24): SIN(X)^25 and
  COS(25×X)×COS(X) are refused NOT_INTEGRABLE.
- **The check:** F′ − f through the same form (tn_zero), every coefficient 0; a number by its value, a tree by
  expand_fully. A check that runs out of room reports NO_ROOM, never NOT_INTEGRABLE.
- **The expanded answer, then the answer as built** (abacus #17188 (b)): F is expanded and checked; where that fails
  and F holds a coefficient that isn't a number, F as built is checked the same way and printed. Expanded, DEG's and
  GRAD's π multiply into a letter's sum under a division, and the check reads -1÷(A^2+1)+π÷(π+A^2×π), which is 0 but
  which expand_fully doesn't cancel: `SIN(X)^3÷(A^2+1)` in DEG was refused at 4d52d0c and is
  `-180×(COS(X)÷(A^2+1)-COS(X)^3÷3÷(A^2+1))÷π` here (12,624 B). Every answer 4d52d0c gave keeps its bytes. The text
  printed is the tree the fallback checked (abacus #17190); integrate_test pins it in DEG and GRAD, and
  tn042-print-unchecked, which checks the tree as built and prints the expanded one, goes red there.
- **Not yet, the general cause** (abacus #17190): expand_fully doesn't cancel a rational coefficient in letters once π
  is multiplied into it, π÷(π+A^2×π) against 1÷(A^2+1). The fallback covers this path's answers; the cause itself is
  expand's, and stays.
- **Not yet, the path's own room** (abacus #17188): a sum whose coefficients hold a letter or π keeps a tree for each
  coefficient, and that is where the arena goes. h042's 15 such NO_ROOM runs at 16,384 B are of this class, as are
  `A×SIN(X)^8` (17,808 B), `A×SIN(X)^6+π×COS(X)^4` (20,704 B) and `π×SIN(3×X)^4+A×COS(X)^2` (27,984 B).
- **CAS 017's rules:** do_itrig reads the integrand as do_iparts does: ib_canon, then rule 2's typed divisors and
  domains (ib_typed_ok), and its other-letters rule on the answer (ib_letters_kept).
- **Steps** (ruling 5): ib_steps' call to do_iparts goes through ib_parts_steps, which answers BAD_ARG where parts
  refuses and this path answers. Every page the other paths give is unchanged.
- **The printed form in DEG and GRAD:** ruling 1's text for `SIN(X)^3` in DEG was p1's, `-180×(COS(X)-COS(X)^3÷3)÷π`;
  the build prints the same polynomial term by term, `-180×COS(X)÷π+60×COS(X)^3÷π`, since its coefficients are numbers
  before the angle factor multiplies them out. `COS(X)^3` in DEG is `180×SIN(X)÷π-60×SIN(X)^3÷π`.
- **Room, least arena** (`--arena 65536 --min-arena`): SIN(X)^2 3,736 B (p1 3,840), SIN(X)^3×COS(X)^2 4,880 (p1
  6,840), A×SIN(X)^2 5,568, SIN(X)^6 in DEG 7,144 (p1 7,960), SIN(X)^12 in DEG 11,920 (p1 17,104).
- **The frames at the revised build:** against 4d52d0c's table no host frame moves (ir_trig is new, 48 B, unrecorded);
  on the S3 still only chain 48→64 against c564e41. S3 text 293,725 B.
- **The frames** (abacus #17161, #17162): seven host frames move, and one on the S3 (chain 48→64). The cause is gcc's
  inlining of small() at 16 more sites; work/042-frames.txt has the moves, the cause traced per function, and the
  dumps' small() sites. The host table is re-recorded in a commit of its own; on the S3 every new frame is 176 B or less.
- **The device** (abacus #17162's condition 1), at 4d52d0c and again at the revised build, the same figures: DEVICE PASS, 1,510 cases byte-identical to the host at 16,384 B (the
  11 CAS 042 cases in scripts/measure.py among them), 298 steps pages. Worst stack 11,360 B masked (STU chain X^ 44,
  derive), unchanged from 1.8.0, + 384 = 11,744 of 12,288; inside 11,888 masked. **Where chain sits:** canon's loop over
  a typed chain of + and − or × and ÷, entered once per level of a nest of them, so its 16 B add once a level: the
  bench's deepest ÷ nest (sweep 1÷(1+ 56, refused at the guard) goes 4,492 → 5,196 B (44 levels × 16) and its − nest
  (sweep -(X+ 56) 3,436 → 3,788 B. The worst path, a chain of ^, goes through canon's power and never reaches chain.
  CAS 042's deepest case is 4,768 B (tn SIN nest 42), the power of a sum 1,792 B.
- **Mutants:** 17 (tn042-*), all caught at the revised build: the first 14 at 4d52d0c (tn042-multiple-off first failed
  to build, tn_multiple unused under -Werror, and was rerun with a (void)), then tn042-room-off, tn042-fallback-off and
  tn042-print-unchecked with #17188's changes, all 17 run again there. The runs' memory peaks weren't sampled.
- **Tests:** make test 20 tests, 119,054 checks at the revised build (119,045 at 4d52d0c). integrate_test's trig(): each branch
  of ruling 1 against a hand-written F in RAD, DEG and GRAD, a seeded draw of products and sums judged on the core by
  value (F′ and a quadrature), the refusals, steps BAD_ARG. scripts/istep-values.py knows rule 5 (a polynomial in SIN
  and COS, read from the text, pages to BAD_ARG), with a control on its reading.


## Today (1.8.0, RAD)

Answered: SIN(X)×COS(X) is SIN(X)^2÷2, SIN(X)×COS(X)^5 is -COS(X)^6÷6 (CAS 017's substitution), and SIN(2×X)×COS(3×X)
is 3×SIN(2×X)×SIN(3×X)÷5+2×COS(2×X)×COS(3×X)÷5 (parts, which closes without an identity). NOT_INTEGRABLE: SIN(X)^2,
COS(X)^2, SIN(X)^3, COS(X)^4, SIN(X)^5, SIN(X)^3×COS(X)^2, SIN(X)^2×COS(X)^2, SIN(X)×SIN(3×X), COS(2×X+1)^2. Any answer
to SIN(X)^2 fails today's check whatever its form: closes() expands F'−f, and SIN^2+COS^2 = 1 is invisible to it.

## What p1 does

1. **When:** after CAS 017's path refuses (its own function after ir_parts in integrate_). The table, the rational path
   and substitution and parts keep every answer they give today.
2. **The base angle:** every SIN and COS holding X has an argument k×X+b with k and b numbers (as CAS 006 takes
   arguments). θ = g×X + b0, g the gcd of the k, and each argument must be m×θ with m whole, |m| ≤ 24. Otherwise
   refused: SIN(X)^2+SIN(SQRT(2)×X)^2, SIN(A×X)^2, SIN(X^2)^2.
3. **The form:** each SIN(m×θ), COS(m×θ) written by Chebyshev's polynomials in C = COS(θ), S = SIN(θ), products
   multiplied out with S^2 = 1 − C^2, so every polynomial in them is p(C) + S×q(C), one way. Nothing else may hold X:
   X×SIN(X)^2, SIN(X)^2÷COS(X) and TAN(X)^2 are refused.
4. **The answer:** p(C) + S×q(C) written back as Σ A_k COS(k×θ) + Σ B_k SIN(k×θ) (C×COS(kθ) and S×COS(kθ) by
   product-to-sum), each term integrated by the table's rule, with angle_factor for DEG and GRAD.
5. **The check:** F' − f put through the same form, every coefficient expanded to 0. closes() is untouched.

Measured, each answer also judged outside casim (F' by central differences against f at 7 points, h = 1e-5 in RAD and 1e-3 in DEG, with Python's math), RAD and DEG. Every row, its least arena and its verdict: work/042-p1-measured.txt.

| integrand | p1, RAD | arena (B) |
|---|---|---|
| SIN(X)^2 | X÷2-SIN(2×X)÷4 | 3,840 |
| COS(X)^2 | X÷2+SIN(2×X)÷4 | 3,648 |
| SIN(X)^3 | -3×COS(X)÷4+COS(3×X)÷12 | 4,624 |
| SIN(X)^3×COS(X)^2 | -COS(X)÷8-COS(3×X)÷48+COS(5×X)÷80 | 7,232 |
| SIN(X)^2×COS(X)^2 | X÷8-SIN(4×X)÷32 | 5,392 |
| SIN(X)×SIN(3×X) | SIN(2×X)÷4-SIN(4×X)÷8 | 4,808 |
| COS(X)^4 | 3×X÷8+SIN(2×X)÷4+SIN(4×X)÷32 | 4,720 |
| SIN(X)^5 | -5×COS(X)÷8+5×COS(3×X)÷48-COS(5×X)÷80 | 6,448 |
| COS(2×X+1)^2 | X÷2+SIN(4×X+2)÷8 | 3,872 |
| SIN(X)^6 | 5×X÷16-15×SIN(2×X)÷64+3×SIN(4×X)÷64-SIN(6×X)÷192 | 7,400 |
| A×SIN(X)^2 | A×X÷2-A×SIN(2×X)÷4 | |

16 integrands (work/042-p1-measured.txt) in RAD and DEG at 16,384 B: 31 of 32 answered, all 31 judged right; SIN(X)^12 in DEG
gives NO_ROOM there and answers at 32,768 B. COS(3×X)^4×SIN(2×X) (degree 14 in C) is NO_ROOM at 16,384 B in RAD and
answers at 65,536 B. DEG: SIN(X)^2 is X÷2-45×SIN(2×X)÷π. **Falsified:** with the product-to-sum half
planted as 1/3, the check refused SIN(X)^2, SIN(X)^3, SIN(X)×SIN(3×X) and COS(X)^4; restored, they answer.

**Cost:** S3 text 290,805 B (+6,967 on 1.8.0), 10,251 under 301,056. Seven accepted frames moved on the S3
(angle_factor 64→80, ib 240→272, integ_checked 112→128, lm_sum 112→144, mul 256→272, sm_anti 224→240, td_prop 48→80),
inlining shifts from the new callers; and p1's recursive tn_form has a 944 B frame, which the build moves into arena
work arrays. The frames are the build's first job, as in CAS 041.

**A guarantee hole p1 has, which the build closes:** `SIN(X)^2×X÷X` answers X÷2-SIN(2×X)÷4, defined at 0 where the
integrand isn't. CAS 017's divisor and domain rules (rule 2) apply on this path too.

## For abacus to rule

1. **The printed form.** p1 prints every answer in multiple angles. The textbook splits by parity: an odd power of SIN
   gives a polynomial in COS (∫SIN(X)^3 = -COS(X)+COS(X)^3÷3, 19 B against p1's 27), an odd power of COS a polynomial
   in SIN, and only the all-even case uses multiple angles (X÷2-SIN(2×X)÷4). The form gives the split for free: S×q(C)
   integrates to −Q(C), and an odd p(C) is C×r(1−S^2), integrating to R(S). Lean: the parity split, since it is the
   form students check against and is shorter for odd powers; multiple angles where both powers are even or angles
   differ (SIN(X)×SIN(3×X)).
2. **Scope of arguments:** numeric k and b only (as CAS 006), m up to 24. Lean yes.
3. **Room:** the arena. Lean: rational coefficients take a fast path in number arrays (p1 builds trees for every
   coefficient, which is where the arena goes), and what still runs out is NO_ROOM as today.
4. **definite integrals:** defint calls the table directly, so ∫ SIN(X)^2 from 0 to π stays NOT_INTEGRABLE. Lean: in
   this unit, with the bounds read through the same form (the answer is π÷2), if its cost measures small; otherwise
   the next unit.
5. **Steps:** refused BAD_ARG in this unit, a page in a follow-on (as CAS 038 and 039).
6. **Out of the unit:** X×SIN(X)^2 (parts after the form), TAN, SEC and CSC powers, a quotient of trig polynomials,
   letters in an argument.

## The oracle, proposed

Built and drawn integrands, judged outside casim: random polynomials in SIN(m×X+m×b0) and COS(m×X+m×b0), m up to 6,
degree up to 8, coefficients small rationals and letters, each answer's derivative compared with the integrand at random
points by the firmware core (the developer tests' oracle), in RAD, DEG and GRAD; every refusal one of the named shapes;
and every integrand 1.8.0 answers prints byte for byte as at 1.8.0.
