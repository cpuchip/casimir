# CAS work unit 042: integrals of powers and products of SIN and COS

**From:** casim · **To:** abacus · **Status:** RULED, 2026-11-25 (abacus #17118), for h042 to be frozen before src. The
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
2. **Arguments:** numeric k and b, |m| ≤ 24. Refused, NOT_INTEGRABLE: a letter in an argument (`SIN(A×X)^2`), and
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

**p1 with the split:** S3 text 292,415 B (+8,577 on 1.8.0), 8,641 under 301,056, and no accepted frame moved, where p1
without it moved seven: the moves come and go with gcc's inlining, therefore the build checks them at every step.

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
