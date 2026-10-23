Casimir 1.1.0: solve says when it leaves complex roots out. With the complex bit off, and always in model 33, a SOLVED or PARTIAL answer now counts the roots the bit would print, after its status: `(X-2)×(X^2+1)` is `SOLVED, 2 CPLX: X=2`, where 1.0.1 printed `SOLVED: X=2`. 1.1.0's code is CAS 028's, accepted at 8a9b010, with the version text changed. src/casim.h's interface is 1.0's, but those answers' bytes change, which is why this is 1.1.0 and not 1.0.2: a caller that compares solve's status to `SOLVED` exactly should read its first six bytes, or split the line at its first `: `.

## What's new since 1.0.1

- **Roots left out** (CAS 028). Without CASIM_COMPLEX (`--complex`), and in model 33, which reads no complex number, SOLVED's and PARTIAL's status gains `, n CPLX`. n is the roots the bit would print, counted with their multiplicities.
  - `(X^2+1)^2×(X-3)` is `SOLVED, 4 CPLX: X=3`.
  - `X^6-1` is `SOLVED, 4 CPLX: X=-1; X=1`.
  - `(X^2+1)×(X^3-2×X-5)` is `PARTIAL, DEGREE 3 LEFT, 2 CPLX`.
  - In model 33, `(X-2)×(X^2+1)` is `SOLVED, 2 CPLX: X=2`, with the bit or without it.
  - Show steps ends on solve's line: `X^4+X^2=0` ends on `ANSWER: SOLVED, 2 CPLX: X=0 (MULT 2)`.
  - The note goes before the line's first `: `, so a reader that splits there, as the calculator's report does, keeps the roots as they were. The widest status is `PARTIAL, DEGREE 10 LEFT, 2 CPLX`, 31 characters (`(X^2+1)×(X^10+X+1)`): the degree left and n together are at most 12.
- **n counts only the pairs factoring finds.** solve finds the linear and quadratic factors over the rationals and leaves a factor of degree 3 or more whole. That factor adds nothing to n: `X^4+1` is still `PARTIAL, DEGREE 4 LEFT`, as the bit prints none of its roots either.
- **What stays as it was, byte for byte:**
  - with the bit, outside model 33: `(X-2)×(X^2+1)` with `--complex` is `SOLVED: X=2; X=-0i1; X=0i1`, as at 1.0.1;
  - NO REAL ROOTS, which says it already: `X^4+4` is `NO REAL ROOTS`;
  - every other operation, and every solve answer with no root left out.
- **At solve's 191 B, the note gives way.** A line that fits only without the note is printed as at 1.0.1 (Not yet, below).
- **Measured:** 84 of the golden corpus's lines gain the note, in the corpus and again in its decimal twin (44 in model 35, 17 in STU, 23 in model 33), and nothing else moves. On the ESP32-S3 at 16,384 B, the five new bench rows take at most 4,360 B of arena and 1,936 B of stack.

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None.

## How it's tested (at this tag)

- `make test`: 20 programs with 118,439 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte. The 84 lines CAS 028 moves are listed with their before (tests/golden/moved/cas-028.txt), and a check puts each before back and gets 1.0.1's files to the byte.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.0.1; the one new function, put_left_out, takes 80 B on the host and 48 B on the S3.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. New in 1.1.0: solve's n is judged on 2,199 seeded polynomials built from known factors, against their construction, and on 2,190 of them against the same input's answer with the bit, which must add exactly n roots.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 445 planted mutants, each caught by the tests. All 445 were run at this tag's tree and caught, 445 of 445 (work/v1.1.0-mutants.txt): 1.0.1's 435 and the ten new on CAS 028's code.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 028's: its own; CAS 005's 3,000 equations, solve and steps, with the bit off; CAS 020's; CAS 021's to 027's; and CAS 014's, 015's and 018's.
- On the ESP32-S3, every one of the bench's 1,369 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 384 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger; another run of this code measured 400), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 243 KB of Casimir's code at -O2 (242,532 B, 679 more than 1.0.1's).

## Not yet, after 1.1.0

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a sum to ∞ from a negative bound: `(K+1)×X^(K+1)×Y^K` from -1 to ∞ prints `X^2×Y÷(X×Y-2×X^2×Y^2+X^3×Y^3); X≠0; Y≠0; ABS(X×Y)<1`, though the sum is 0 at X = 0;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - past an irrational constant, a factor that is never 0 or a letter at a negative power, the shift that clears a rational ratio's row is refused. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to ∞ keeps `X≠0`, while its rational twin `(K-1)×(X×Y)^K÷X` doesn't, and `K×(EXP(Z)×X)^K÷X` from 1 to ∞ keeps `X≠0`. (Past a letter in a function the shift is made now, above.)
- **A redundant row.** The letter's row can be printed beside a row that already excludes the letter's 0. `K×COS(Z)^(K-1)` from 1 to N prints `Z≠0` beside `COS(Z)≠1`, and COS(0) is 1. It excludes nothing the other row doesn't.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0 (SIN and TAN at π, COS at π÷2, LN at 1), now only where the same sum with the factor read as a letter, its rational twin, prints that letter's row for nothing. Of the 640 probe sums, 1.0.0 had this at 62 and 1.0.1 has it at 24 (the first shape below); the other two shapes were found beyond those sets:
    - to N, at an N before the first term that divides by the factor, where the sum has a value (the empty sum, or a sum of terms that don't divide) and the terms after it divide. `(Y÷SIN(Z))^K` from 1 to N prints `-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; Y÷SIN(Z)≠1`, with no value at Z = π for N = 0, where the sum is 0. Its twin `(Y÷X)^K` prints `X≠0`, which excludes X = 0 at N = 0 too. Over the probe sets there are 24 of these: with F one of SIN(Z) and COS(Z), `K×F^(K-1)` from -1 and 0, `(F×Y)^K` from -1, `(Y÷F)^K` from -1, 0, 1 and 2, and `(K-1)×(F×Y)^K÷F` from 0; with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z), `F^(2×K-1)×Y^K` and `F^(3×K-1)×Y^K` from 0;
    - to ∞ from a negative lo, where the term at lo is 0: `(K+1)×SIN(Z)^(K+1)×Y^K` from -1 to ∞ prints `Y×SIN(Z)^2÷(Y×SIN(Z)-2×Y^2×SIN(Z)^2+Y^3×SIN(Z)^3); Y≠0; Z≠0; ABS(Y×SIN(Z))<1`, which is 0÷0 at Z = π, where every term is 0. It's the function-factor face of the first row for nothing above. A grid of such sums found 12: `(K+1)×F^(K+1)×Y^K` and `(K+1)×F^(K+2)×Y^K` from -1, and `(K+2)×F^(K+2)×Y^K` from -2, with F one of SIN(Z), COS(Z), TAN(Z) and LN(Z);
    - past a constant beside the factor: `K×(SQRT(5)÷3×Y×SIN(Z))^K÷SIN(Z)` from 1 to ∞ prints `Y×SIN(Z)×SQRT(5)÷3÷(SIN(Z)-Y×SIN(Z)^2×SQRT(5)÷3)-5×Y^2×SIN(Z)÷6÷(Y×SIN(Z)×SQRT(5)-5×Y^2×SIN(Z)^2÷6-3÷2); Z≠0; ABS(Y×SIN(Z)×SQRT(5)÷3)<1`, with no value at Z = π, where the sum is Y×SQRT(5)÷3. The constant stops the shift (above), and its twin keeps `X≠0`. The same holds for COS, TAN and LN.
- `X^K÷Y^K` is NO_CLOSED_FORM, while `(X÷Y)^K` answers.
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
