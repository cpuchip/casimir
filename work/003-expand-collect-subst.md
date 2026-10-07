# CAS work unit 003: expand, collect, substitute

**From:** abacus · **To:** casim · **Status:** ready (Michael, 2026-10-07: "go ahead with CAS 003", relayed by
basecamp). It follows CAS 002 and 002b, accepted at fb94fe7 (abacus docs/evidence/2026-10-06-cas-002-review.md).

## Why

The plan's order (abacus docs/ideas/2026-10-06-rpl-cas-emulator.md, section 2):
1. derivatives, done;
2. simplify, done;
3. expand, collect and substitute, this unit;
4. later, exact polynomial solving and simple integrals.

These are what an algebra student does by hand, and tutor's first course (Algebra to Calculus) teaches them. Each
stands on 002's canonical form.

This unit also delivers the wish in CAS 002's ruling 3: names derive refuses become opaque functions for simplify.

## Rules

1. **The API:**
   - **The functions:**
     - `casim_expand(eqn, angle, out, cap, arena, arena_cap)`;
     - `casim_collect(eqn, var, angle, …)`, with `var` a letter A–Z;
     - `casim_subst(eqn, var, repl, angle, …)`.
   - **The CLI:**
     - `casim expand "<t>" RAD`;
     - `casim collect "<t>" X RAD`;
     - `casim subst "<t>" X "<u>" RAD`.
   - **Statuses** are the same as derive's. With `L=R`, each side is treated.
2. **Expand** distributes × over + and raises a sum to a non-negative integer power (multinomial).
   - **Where:** at every depth, inside function arguments too.
   - **The result** is in 002's canonical form, with like terms collected.
   - **Canonical:** two texts with the same expansion print identically.
   - **Not expanded:**
     - exp and log rules: EXP(a+b) stays;
     - trig expansion;
     - a sum in a denominator: a÷(b+c) stays, with b+c expanded inside;
     - a sum to a negative or fractional power: it stays, with its base expanded.
3. **Collect in a variable** prints Σ c_k × var^k with each k at most once, in descending k.
   - **Each c_k** is simplified and free of var.
   - **Printing:**
     - c_k × var^1 prints as c_k × var, and c_0 prints alone;
     - c_k = 1 prints var^k alone;
     - a c_k that is a sum is in parentheses: `(Y+1)×X^2`.
   - **Negative integer powers** are allowed, as k < 0, and they fall in order.
   - **A remainder.** Terms where var appears other than as an integer power (SIN(X), X^0.5, EXP(X)) are grouped,
     simplified, after the powers. The whole is still canonical.
4. **Substitute** replaces every occurrence of var with the replacement as a whole (as if parenthesized), then
   simplifies (002).
   - The replacement may contain var: X → X+1 is the classic.
   - Both sides of `L=R` are treated.
5. **Opaque names** (CAS 002's ruling 3 wish). Simplify, expand and collect treat the names derive refuses (IP, FP,
   RND, INTG, SGN and the rest of your refusal list) as opaque functions:
   - their arguments are simplified;
   - like terms collect (IP(X)+IP(X) → 2×IP(X)).

   **derive keeps refusing them.**

   **The exception:** a name whose value changes at each evaluation (RAND) is never combined or folded. RAND+RAND
   stays as written: it is two draws.
6. **Domain** (002's rule 3): never change a value where the original is defined. Expand and collect are identities,
   and substitution is evaluation.
7. **Idempotent:**
   - expand(expand(t)) = expand(t);
   - collect(collect(t, X), X) = collect(t, X).
8. **Too long is a status, not a truncation.** A result over 192 bytes returns TOO_LONG. Expansion grows: (X+Y+1)^6
   cannot fit.
9. **The budgets** (002b's, unchanged):
   - **stack:** at most 12 KB worst, through the depth guard;
   - **arena:** 8 KB;
   - **time:** 20 ms for a 192-byte input on your S3;
   - **flash:** casim in total at most 40 KB. If that can't hold, say what it costs, and don't just exceed it.

## The oracle (abacus's)

- **The generator.** `tools/refmodel/gen_cas003.py` in the abacus repo writes three kinds of case:
  - **E (expand):** A is a product or power of sums, sometimes inside a function's argument. B is A already
    expanded, its terms shuffled.
  - **C (collect):** A is a polynomial in X whose coefficients hold Y and functions of Y. Each case carries every
    c_k's value at the point.
  - **S (subst):** A is a CAS 002-style expression, and U a small replacement.
- **The self-check, before you're judged** (dev seed 31337, 2000 cases):
  - **The texts:** E's A and B, C's A, and S's A with X replaced by (U) each evaluate to f in the core. 2800 / 2800
    at a relative 1E-24.
  - **The check can fail:** with one `+` flipped to `-` in every B, 761 / 800 go red.
  - **The coefficients:** each C case's coefficients sum back to f, 600 / 600.
- **The judgment:**
  - **(a) Value.** Through the core's eqn_eval, within a relative 1E-24 of f: expand(A), expand(B), collect(A, X)
    and subst(A, X, U).
  - **(b) Canonical expand.** expand(A) is identical text to expand(B), for every E case.
  - **(c) Collect's shape.**
    - Each term before the remainder is c × X^k, with c free of X, each k once and in descending order.
    - Each c's value at the point matches the case's c_k within a relative 1E-24.
  - **(d) Idempotence,** for expand and collect.
  - **(e) The earlier hidden sets.** CAS 001's and 002's are rerun, and their results must not change.
  - **(f) TOO_LONG** is counted and reported. The generator keeps every B under 190 bytes, so on E it should be
    zero.
- **The hidden set:** 2000 cases from a seed abacus keeps.
- **The visible examples:** work/003-examples.txt, 40 cases from seed 3. Each line is one of:
  - `ID | expand | A | B | angle | assignments | f`
  - `ID | collect | A | X | angle | assignments | f | k=c_k;…`
  - `ID | subst | A | X | U | angle | assignments | f`

## Acceptance

- **Your tests:**
  - each rule alone;
  - (X+Y)^3, and an expansion nested inside SIN;
  - collect with a remainder, and with a negative power;
  - subst of X → X+1;
  - IP(X)+IP(X) → 2×IP(X);
  - RAND+RAND left alone;
  - TOO_LONG on (X+Y+1)^6.
- **abacus's hidden set:** (a) to (e), each in full, and (f) reported.
- **Mutants, each caught by your own tests:**
  - expand not entering function arguments;
  - collect printing one power twice (no merge after expanding);
  - subst replacing only the first occurrence;
  - RAND+RAND folded to 2×RAND.
- **The S3's numbers** for rule 9.
