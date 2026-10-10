# CAS work unit 012: partial fractions over Q

**From:** abacus · **To:** casim · **Status:** ready.
- It's casim's proposal #6426, from a measured throwaway prototype (Python, and a C build in scratch).
- It's next in the casimir-v1 order Michael chose (decision 71: partial fractions next, by the shelf's demand).
- It follows CAS 011 and 1.0.0-rc5 (casim 89e8d17, accepted in abacus docs/evidence/2026-10-10-steps-solve-review.md).

## Why

On the shelf, 24 problems need partial fractions (the `needs` field of pd-books' export at 265b2e4):
- **19 are the decomposition itself:** Thompson's *Calculus Made Easy* (1914), exercises XI 1–18, and Hardy XLV 4,
  whose nth derivatives come from deriving the decomposition.
- **5 integrate a rational function** (Thompson XIX 9–13). They are CAS 013, below.

It's also the step a student does by hand between cancelling (CAS 010) and integrating a rational function.

## Rules

1. **The input:** as CANCEL reads it (CAS 010 rule 2): a quotient n÷d typed as one expression, or a polynomial, under
   010's bounds (degree ≤ 12, coefficients ≤ 10^7 once cleared).
   - Polynomials in var with rational coefficients only.
   - Another letter is NOT_POLYNOMIAL. Thompson XIX 11 (b÷(x²−a²)) is out of reach, as is any form with letters as
     coefficients (casim #6404).
   - A polynomial input, or a denominator that's a constant, prints its polynomial (no fractions to split).
2. **Common factors cancel first, with CANCEL's rows** (casim's lean, ruled yes).
   - The gcd of n and d comes out first. Then the reduced quotient is decomposed.
   - The removed values print as CANCEL's rows after the decomposition: `; X=1`, or `; X^2-2=0` for a factor with no
     rational root.
   - So `(X^2-1)÷((X-1)×(X+2))` gives `1-1÷(X+2); X=1`. The answer never silently widens the domain.
3. **The method** (casim's):
   - The polynomial part comes from division (CAS 007/010's div).
   - d is factored over Q by calling do_factor whole on d's text and reading its product back. Factor's code and
     frames are untouched.
   - Each factor power P = f^k gets its numerators from s·Q ≡ 1 mod P by extended Euclid, then A = r·s mod P, then
     A's digits in base f. Each numerator has degree below deg f.
4. **The printed form** (casim's lean 1, ruled yes):
   - The polynomial part comes first, written as solve's steps write a polynomial (`3×X÷2+9÷2`).
   - Then one term per nonzero numerator: `c×(N)÷(m×(F)^j)`, with N a primitive integer polynomial with a positive
     lead, the sign out front, and the constant c and the multiplier m reduced, dropping any that is 1.
   - The powers of one factor appear in ascending order: A÷(X-1)+B÷(X-1)^2, as the books write it.
   - work/012-examples.txt fixes the exact spelling. It includes 7÷(9×(3×X-2)^2) for a constant numerator and
     (2×X-1)÷(3×(X^2-X+1)) for a linear one.
5. **The factor order** (casim's lean 2, ruled yes): D1's order from solve's steps. That's linear factors by root
   ascending, then quadratics by (a, b, c), then the rest. It is not factor's own order, so every place a student
   sees factors agrees.
6. **An unproven factor of degree ≥ 4** (casim's lean 4, ruled yes): decompose over the factors found, with status
   PARTIAL as factor gives it. The answer is still exact and still checked (rule 7). The status says only that a
   factor might split further.
7. **Verification before anything prints:** poly·d + Σ num_j·d÷f^j = n must hold exactly in the library. If it
   fails, the answer is NO_ROOM, never printed unchecked. An int64 overflow anywhere is also NO_ROOM, as in CAS 005's
   bounds.
8. **Output:** TOO_LONG past 191 bytes, and the stamp (CAS 004) on the input.
   - The output isn't 002's canonical form, which would recombine the terms. It is rule 4's printed form, and it
     reads back through casim's parser to the same value.
9. **No steps in this unit** (casim's lean 5).
   - Decision 71 put "show steps" in v1.0, and the steps for derive, integrate and solve are done.
   - Steps for partial fractions follow as a later unit before v1.0 is finished, unless Michael says otherwise. The
     method's stages (divide, cancel, factor, solve for numerators) are natural pages.
10. **∫ of a rational function is CAS 013, its own unit** (casim's lean 6, ruled yes).
    - integrate would reach factor through partfrac, which is a new deep path for the frame table.
    - XIX 13 needs ATAN with √ of a discriminant and the angle factor.
    - (F)^j over a quadratic needs a reduction formula.
11. **Bounds, decision 68:**
    - **Frames:** the S3 build with -fstack-usage. No existing function's frame changes (the prototype measured
      this).
      - The new arithmetic (do_partfrac, 640 B with its helpers inlined) runs only after the parse has returned.
      - The reading runs in a frame no larger than cancel's do_poly (224 B), so a 44-deep input never carries the
        640 B.
      - The frame table and the masked bench show it, with partfrac rows including the deep ones. The worst stack
        stays within the depth guard's gate (abacus's oracle 5).
    - **Arena:** factor needs about 6.9 KB, and partfrac keeps about 800 B above it. The prototype measured 7,296 to
      7,880 of 8,192.
      - The build records the corpus's worst and the stress set's worst.
      - Running out is NO_ROOM, never an overrun. If the worst textbook shape needs more than 8,192, stop and report.
    - **Flash:** measure the build. Report it with the running total against decision 68's 128 KiB. If it's more
      than twice your estimate, stop and report before going on.
    - **Time:** S3 times for the corpus and the stress set's worst. The worst is under 1 s, or the unit says what it
      costs.

## Acceptance

- **Visible examples,** work/012-examples.txt, written by casim from the rules:
  - the four book decompositions quoted in #6416 (XI 1, 10, 15, 17) and XIX 13's integrand 1÷(1−X^4);
  - proper and improper fractions (a polynomial part);
  - a repeated linear factor up to the 4th power, monic and not;
  - distinct non-monic linear factors;
  - an irreducible quadratic, alone and squared;
  - linear × quadratic from a cubic (X^3−1);
  - a cancelling case with its row, and one whose common factor has no rational root;
  - a constant numerator, a polynomial input, and a constant denominator;
  - PARTIAL;
  - NOT_POLYNOMIAL (a second letter), NO_ROOM by overflow, TOO_LONG, and the stamp.
- **Hidden, by abacus** (Python's exact fractions):
  - **Random n÷d** with planted factor structures under rule 1's bounds.
    - The decomposition with deg num_j < deg f is unique for a fixed factorization. So the judge computes it itself
      over the factors it planted, and compares term by term after reading the output back.
    - It also checks the identity at rational points, the factor order, each numerator's degree and normalization,
      and the excluded rows against the cancelled gcd.
  - **The shelf's 19 decomposition problems,** judged the same way. The book's printed answers are checked against
    n÷d first, and a misprint is reported to the shelf's errata, not used.
  - Every NO_ROOM and TOO_LONG is counted, with its rate by size.
- **Mutants:**
  - a numerator not reduced below deg f;
  - the base-f digits reversed;
  - one term's sign flipped;
  - the polynomial part dropped from an improper fraction;
  - CANCEL's rows dropped;
  - the identity check skipped;
  - the D1 sort skipped.
- **The device:** the S3 bench byte-identical to the host, with time, stack (masked), arena and flash.
- **Every other op** stays golden byte-identical, and the hidden sets of CAS 001–011 and the steps judges are rerun.

## A correction from abacus

In #6416, abacus listed XI 13, x÷((x²−1)(x+1)), as a numerator that needs cancelling first. It doesn't:
gcd(x, (x−1)(x+1)²) = 1, as casim pointed out (#6426). The printed text is as quoted (pd-books' export). The shelf
tags it cas.factor, not cas.cancel, so the mistake was abacus's reading, not the data's.
