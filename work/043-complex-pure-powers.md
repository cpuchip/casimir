# CAS work unit 043: x^n = c under --complex, every root where the line holds it

**From:** casim · **To:** abacus · **Status:** DRAFT, 2026-11-25, for abacus's rulings. The number is abacus's
(#17147). Measured on a throwaway prototype, p1, over 401bbe1 (1.9.0's code), deleted once this is ruled.

- **Why now:** Michael's T113 ("Casim solves it under CPLX"), as abacus set its floor (#17147): under --complex,
  x^n = c never prints less than it does without the bit. It is the first piece of roadmap phase 3, under abacus's
  ruling on the cosine form (work/roadmap.md: (a) canon's own form, π/6 and π/4 reducing through the table, π/5
  staying as written; (b) built by de Moivre and checked before printing; (c) printed only where the whole line fits).
- **The question:** for x^n = c with c a number, can solve under the bit print every root exactly, checked, where the
  line holds them, and where it doesn't, print at least what it prints without the bit?

## Today (1.9.0, RAD)

Under the bit, x^n = c prints what numeric solve's factors over Q give: CAS 020's pair for each quadratic factor, and
PARTIAL for any factor of degree 3 or more. CAS 041's pass (pp_fix), which gives the real roots of x^n = c, doesn't
run under the bit. So the bit prints less than its absence: X^3=2 is PARTIAL, DEGREE 3 LEFT where SOLVED, 2 CPLX:
X=2^(1÷3) without it; X^4=-16 is PARTIAL, DEGREE 4 LEFT where NO REAL ROOTS; X^5=32 is PARTIAL, DEGREE 4 LEFT: X=2
where SOLVED, 4 CPLX: X=2; and the same for X^4=2, X^6=-1, X^8=1, X^9=64, X^12=4096. Already whole under the bit:
X^2=-4, X^3=±1, X^3=8, X^4=1, X^4=16, X^6=1, X^6=64.

## What p1 does

1. **When:** a pass after put_solution, beside CAS 041's pp_fix and only under the bit, as pp_fix is only without it.
   A line already SOLVED with every root (CAS 020's) is left as it is. The typed text is read again by pp_read
   (x^n = c, n ≥ 3, c a nonzero number).
2. **The roots:** the real ones as pp_roots builds them (CAS 041 ruling 7). Each complex pair as ρ×COS(θ) ±
   ρ×SIN(θ)×0i1, ρ = |c|^(1÷n) built as pp_roots builds it, θ = (2k + [c < 0])÷n of a half turn, 0 < θ < 1, in the
   mode's unit (π, 180 or 200), canon's table reducing multiples of π/6 and π/4 (ruling (a)). The parts are two real
   trees; 0i1 is only printed.
3. **The check (ruling (b)):** each root read back in abn at 34 digits: (re + im i)^n against c, each part within a
   relative 1E-24 of |c|; a miss refuses the pass (the line stays as numeric solve left it).
4. **The line (ruling (c)):** SOLVED: then the real roots, then each pair, minus first, by θ. Where it doesn't fit
   191 B, the line without the bit (SOLVED, k CPLX with the real roots, or NO REAL ROOTS), which is the floor.

| equation (RAD) | p1 | bytes |
|---|---|---|
| X^3=2 | SOLVED: X=2^(1÷3); X=-2^(1÷3)÷2-2^(1÷3)×SQRT(3)÷2×0i1; X=-2^(1÷3)÷2+2^(1÷3)×SQRT(3)÷2×0i1 | 102 |
| X^4=-16 | SOLVED: X=SQRT(2)-SQRT(2)×0i1; X=SQRT(2)+SQRT(2)×0i1; X=-SQRT(2)-SQRT(2)×0i1; X=-SQRT(2)+SQRT(2)×0i1 | 104 |
| X^4=2 | SOLVED: X=-2^(1÷4); X=2^(1÷4); X=-2^(1÷4)×0i1; X=2^(1÷4)×0i1 | 66 |
| X^5=1 | SOLVED: X=1; X=COS(2×π÷5)-SIN(2×π÷5)×0i1; X=COS(2×π÷5)+SIN(2×π÷5)×0i1; X=COS(4×π÷5)-SIN(4×π÷5)×0i1; X=COS(4×π÷5)+SIN(4×π÷5)×0i1 | 155 |
| X^6=-1 | SOLVED: X=SQRT(3)÷2-1÷2×0i1; X=SQRT(3)÷2+1÷2×0i1; X=-0i1; X=0i1; X=-SQRT(3)÷2-1÷2×0i1; X=-SQRT(3)÷2+1÷2×0i1 | 119 |
| X^8=1 | SOLVED: X=-1; X=1; X=SQRT(2)÷2-SQRT(2)÷2×0i1; X=SQRT(2)÷2+SQRT(2)÷2×0i1; X=-0i1; X=0i1; X=-SQRT(2)÷2-SQRT(2)÷2×0i1; X=-SQRT(2)÷2+SQRT(2)÷2×0i1 | 154 |
| X^9=64 | SOLVED, 8 CPLX: X=4^(1÷3) (the floor: all nine don't fit) | |
| X^3=1 | SOLVED: X=1; X=(-1-SQRT(3)×0i1)÷2; X=(-1+SQRT(3)×0i1)÷2 (CAS 020's, as at 1.9.0) | 59 |

Measured over n = 2..12 and c in 1, -1, 2, -2, 8, -8, 16, 64, 1÷2, 3, -27, in RAD, DEG and GRAD (363 equations,
work/043-p1-measured.txt): 123 print every root, the other 240 print the line without the bit byte for byte, and none
prints anything else; every root of the 123 is judged right outside casim (z^n against c in Python's complex
arithmetic, n distinct roots). By n, every root fits for n = 2 and 3 at every c tried, for n = 4 at all but -8 and
-27, for n = 6 at 1, -1, 8, 64, 3 and -27, for n = 5 at ±1, and for n = 8 at 1 and 16; n = 7 and 9 to 12 fit at none
of these c, though X^12=4096 does (its twelve roots are integers and table values, 174 B).

**Cost:** S3 text 296,927 B (+3,202 on 1.9.0), 4,129 under 301,056. On the S3 no accepted frame moves (build/s3's .su
against 1.9.0's, by hand); cx_fix is 608 B, of which 192 is its line, which the build moves into the arena. On the host,
scripts/frame-table.py moves one: do_sum_ 816 → 832.

## For abacus to rule

1. **The form of a pair.** p1 distributes ρ: -2^(1÷3)÷2-2^(1÷3)×SQRT(3)÷2×0i1. Two others: ρ factored,
   2^(1÷3)×(-1-SQRT(3)×0i1)÷2, which is CAS 020's shape times ρ and shorter as ρ grows; or CAS 020's own form wherever
   cos and sin are in the table, and the cosine form only for the rest. Lean: ρ factored, (a±b×0i1) in canon's terms
   of the table's values, since it reads as CAS 020's roots do and fits more lines.
2. **Where the line doesn't fit.** p1 prints the line without the bit, complete about the real roots with their
   complex count (SOLVED, 8 CPLX: X=4^(1÷3)). The other reading of #17147: PARTIAL, DEGREE d LEFT with the real roots
   and as many pairs as fit. Lean: the line without the bit, since PARTIAL would say less than the SOLVED it replaces.
3. **The order:** real roots ascending, then each pair minus first, by θ. CAS 020 orders by canon. Lean: by θ, so
   a reader can see the roots go round.
4. **Scope:** c a number, n up to solve's 12, the pure power only (x^n = c, not (x+1)^n = c), and the lines numeric
   solve leaves PARTIAL or short of the floor. Letters (X^3=A under the bit) stay as at 1.9.0. Lean yes.
5. **Room:** S3 text 4,129 B under the line after p1. Michael's rule raises the room per unit on its measured size;
   say whether this unit's measurement goes to him now.
6. **The host frame do_sum_ 816 → 832:** the build hunts it first, as CAS 041's frames were hunted, before bringing
   the table.

## The oracle, proposed

Every x^n = c for n = 2..12 and c over small integers, fractions and their negatives, in three modes and both models:
under the bit, either every root (each judged on the core by value, n distinct, conjugates paired) or the line without
the bit byte for byte; never anything else. Every line 1.9.0 prints whole under the bit unchanged.
