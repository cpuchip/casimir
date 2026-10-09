# CAS work unit 010: polynomial division, GCD and cancelling over Q

**From:** abacus · **To:** casim · **Status:** ready.
- It's casim's proposal #5584, with a measured throwaway prototype.
- It's the next item in the casimir-v1 order proposed to Michael (a calculator's quotient, remainder and GCD commands). If Michael's
  ruling on the card changes the order, this unit waits.
- It follows CAS 009 (accepted at casim 110479a, abacus docs/evidence/2026-10-09-cas-009-review.md).

## Why

Dividing one polynomial by another, finding their common factor, and cancelling it from a fraction are what a student
does by hand before partial fractions and limits. (X^2−1)÷(X−1) → X+1 is the step a limit like lim X→1 stands on.

## Rules

1. **The input:** the CAS 005 walk, as typed, under its bounds (degree ≤ 12, coefficients ≤ 10^7).
   - Polynomials in var with rational coefficients only.
   - Anything else (another letter, var inside a function or a denominator) is NOT_POLYNOMIAL, as in CAS 005 and
     007.
   - The second polynomial travels in repl, read in its own repl_model, as subst reads it.
2. **The operations:**
   - **CASIM_PDIV (f, g):** the quotient q and remainder r over Q, with f = q×g + r and deg r < deg g.
     - out is `q; r`: two rows, separated by solve's "; ". Each row is a plain expression the user can store or
       evaluate.
     - The firmware labels the rows QUOT and REM outside the text, as it labels +C.
     - r = 0 prints as 0. g = 0 is UNDEFINED (CAS 008's status).
   - **CASIM_PGCD (f, g):** the greatest common divisor.
     - **Normalized as the primitive integer polynomial with a positive leading coefficient** (casim's (c)): 2×X+1,
       not X+0.5. That matches FACTOR's factors. So gcd(2×X+2, 4×X+4) is X+1, and work/010-examples.txt says why
       (over Q a gcd is defined up to a constant).
     - gcd(f, 0) is f's primitive part; gcd(0, 0) is 0; a constant gcd is 1.
   - **CASIM_CANCEL (t):** a quotient of two polynomials, typed as one expression (num÷den). It's reduced by their gcd.
     - out is the reduced quotient, then the values the cancelling removed, as rows:
       - each rational root of the cancelled gcd as `X=a` (in ascending order, each once);
       - or, when the gcd has a factor with no rational root, that factor as an equation `X^2-2=0`.
     - The firmware labels these rows (e.g. "NOT AT"). The student's expression was undefined there, and the
       reduced one isn't, so the rows keep that fact. Limits will read them.
     - A gcd of 1 prints the quotient in canonical form with no rows.
   - **LCM: out** (casim's (e)). It's f×g÷gcd, and a student who needs it can say so; decision 68 asks for the
     smallest.
3. **The GCD's method, casim's modular algorithm, and its proof:**
   - f and g are cleared to primitive integers.
   - Every 31-bit prime used divides neither leading coefficient.
   - Primes are combined by Garner into int64, overflow-checked.
   - The smallest modular degree seen is kept.
   - **The candidate is accepted only if it divides both f and g exactly** (CAS 007's div_poly). It then divides the
     true gcd, and its degree equals the smallest modular degree, which is at least the true gcd's degree. So it is
     the gcd. Write this argument in a comment at the candidate test.
   - A candidate that fails the test tries more primes, up to a cap. At the cap the answer is NO_ROOM, never an
     unproven gcd.
4. **Division over Q** uses casim's own rat_add and rat_mul, not new helpers.
   - An int64 overflow anywhere is NO_ROOM, as CAS 005's bounds.
   - casim measured 21% of random degree-12 pairs with coefficients up to 10^3 overflowing. Those answers would mostly
     pass 191 bytes anyway; report the measured rate for the build.
5. **Verification, before anything is printed** (derive-back's twin):
   - PDIV: q×g + r = f exactly, and deg r < deg g;
   - PGCD: the candidate test of rule 3;
   - CANCEL: the reduced numerator × gcd = the numerator, and the same for the denominator, exactly.
   - A failed check is a refusal, never a printed answer.
6. **Output:** 002's canonical form, TOO_LONG past 191 bytes, and the stamp (CAS 004) on both inputs.
7. **Bounds, decision 68:**
   - **Flash:** the prototype measured 3,057 B (GCD) + 2,463 B (division, before reusing rat_*), and estimated 2–3 KB
     for the plumbing, printing and cancelling. Measure the build. casim's own caution is that the estimate may be 2×
     low: if the build passes 16 KB, stop and report before going on.
   - **Frames:** gcd_int's 736 B of mod-p polynomials move to the arena, as FACTOR's do, so the frames stay ≤ 224 B.
     The worst stack stays within the depth guard's 12,288 B. Derive's path isn't touched, so the 47 → 46 recalibration
     isn't due here.
   - The arena and S3 times are measured. The worst is under 1 s, or the unit says what it costs.

## Acceptance

- **Visible examples,** work/010-examples.txt, written by casim from the rules:
  - PDIV:
    - exact (X^2−1 by X−1 gives X+1; 0);
    - with a remainder (X^3+2 by X−1 gives X^2+X+1; 3);
    - a rational quotient (X^2 by 2×X+1);
    - deg f < deg g (q = 0, r = f);
    - g a constant;
    - g = 0 (UNDEFINED).
  - PGCD:
    - a common linear factor;
    - a common quadratic (X^2+1);
    - coprime (1);
    - with 0;
    - contents that differ (2×X+2 and 4×X+4 give X+1).
  - CANCEL:
    - (X^2−1)÷(X−1) gives X+1; X=1;
    - a double cancel, (X−1)^2×(X+2)÷((X−1)×(X+3)), whose rows are X=1;
    - an irrational common factor, (X^2−2)×X÷(X^2−2), whose rows are X^2-2=0;
    - nothing to cancel;
    - a constant over a polynomial.
  - A NOT_POLYNOMIAL input, NO_ROOM by overflow, and the stamp.
- **Hidden, by abacus** (Python's exact fractions, no SymPy needed for the judge):
  - random pairs with planted common factors and remainders, under rule 1's bounds;
  - PDIV judged by f = q×g + r and deg r < deg g in exact rationals;
  - PGCD against Euclid's algorithm in exact rationals, normalized as rule 2;
  - CANCEL judged by the identity and by the excluded set against the cancelled gcd's roots;
  - every NO_ROOM counted, with its rate by coefficient size.
- **Mutants:**
  - the modular degree not minimized (an unlucky prime's larger degree kept);
  - the candidate test skipped;
  - Garner's overflow check removed;
  - the remainder's degree not reduced below deg g;
  - the gcd's sign not normalized;
  - CANCEL's excluded rows dropped.
- **The device:** the S3 bench, byte-identical to the host, with time, stack, arena and flash.
