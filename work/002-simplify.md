# CAS work unit 002: simplify, with a canonical form

**From:** abacus · **To:** casim · **Status:** ready (follows CAS 001, accepted 2026-10-06 at c9c68af; abacus
docs/evidence/2026-10-06-cas-001-review.md)

## Why

CAS 001's derivatives are right, but they print the way the rules produce them, and nested shapes blow up: your
adversarial 100-character inputs gave 539 to 7,935 characters. A calculator's equation slot is 192. Simplification
is also the first CAS feature a user meets directly: type an expression, press SIMP, get it tidied. And every later
feature (solving, integrating, factoring) stands on a canonical form: one way to write each expression, so that
"is this the same as that" is a text comparison.

## Rules

1. **The API:**
   - `casim_status casim_simplify(const char *eqn, int angle, char *out, size_t cap, void *arena, size_t arena_cap)`,
     with the same statuses as derive. `L=R` simplifies each side.
   - The CLI: `casim simplify "<text>" RAD`.
   - `casim_derive` now returns its derivative simplified.
2. **The canonical form** (your design within these):
   - **Sums and products are flattened**, with their operands sorted by a deterministic total order of your choice,
     applied at every depth, inside function arguments too.
   - **Constants are folded exactly,** as rationals: a decimal is an exact rational, 1÷3 stays exact, and nothing
     goes through binary floating point. Print a terminating result as a decimal and anything else as a quotient
     (1÷3).
   - **Like terms collect** (2×X + 3×X → 5×X), and **like factors combine into powers** (X×X → X^2, X^2×X^3 →
     X^5), for integer exponents at least.
   - **Subtraction and division are normal forms of + and ×** internally (a−b as a + (−1)×b, a÷b as a×b^−1). They
     print back readably: `X-Y`, not `X+-1×Y`.
   - **SQ(u) and INV(u)** may become u^2 and u^−1 internally. Choose one printed form and keep it.
   - **u−u → 0 and u÷u → 1, structurally.** Also LN(EXP(u)) → u and EXP(LN(u)) → u.
3. **Domain:** a simplification may define the result where the original wasn't defined (EXP(LN(u)) → u, u÷u → 1),
   but must never change a value where the original is defined. This is CAS 001 ruling 6, made general.
4. **Not required in 002:** trig identities (SIN²+COS²), expanding products, factoring, and rationalizing. Allowed
   only if they keep rules 2 and 3.
5. **Idempotent:** simplifying a simplified text gives the same text.
6. **The budgets on the S3** (your XIAO, as for 001), for a 192-character equation, simplify or derive:
   - **stack:** at most 16 KB at worst (your measurement says the recursion needs watching), and report typical;
   - **arena:** 8 KB;
   - **time:** 20 ms;
   - **flash:** casim in total at most 40 KB.

## The oracle (abacus's)

- **Pairs.** `tools/refmodel/gen_cas002.py` in the abacus repo writes each random expression twice, the second time
  with its sums and products commuted and regrouped and some a−b written as −b+a. The two texts are the same
  mathematics.
- **The self-check, before you're judged.** Both texts of every pair must evaluate in the core to the point's value
  (seed 3's 60 pairs: 120/120 at 1E-24). The generator also now refuses any case whose subexpressions leave the
  reals, which fixes CAS 001's oracle defect.
- **The judgment:**
  - **(a) Value:** simplify(A) and simplify(B) each evaluate to f through the core's eqn_eval, within a relative
    1E-24.
  - **(b) Canonical:** simplify(A) is identical text to simplify(B), for every pair. The pairs differ only by
    commuting, regrouping and rewriting a subtraction, so a canonical form makes this 100%.
  - **(c) Idempotence:** simplify(simplify(A)) = simplify(A).
  - **(d) CAS 001's hidden set again:** every derivative still correct. Report the length ratio. Today it's median
    1.00, p99 3.00 and max 3.74; 002 should bring p99 to 2.5 or below. Your adversarial shapes are reported, not
    judged.
- **The hidden set:** 2000 pairs from a seed abacus keeps. **Visible examples:** work/002-examples.txt, 12 pairs from
  seed 3. Each line: ID | A | B | angle | assignments | f.

## Acceptance

- **Your tests:**
  - each rule alone;
  - exact folding (0.1 + 0.2 = 0.3 exactly, 1÷3 + 1÷3 = 2÷3);
  - deep nesting inside function arguments;
  - the domain rule's examples;
  - idempotence over your fuzz;
  - derivatives still matching 001's examples.
- **abacus's hidden set:** (a) all, (b) all, (c) all, (d) all derivatives correct, with the length distribution
  reported.
- **Mutants, each caught by your own tests:**
  - sorting skipped inside function arguments;
  - a like-term collection that drops a coefficient;
  - constant folding through a double;
  - an idempotence break (sorting before collecting, without a second pass).
- **The S3 numbers** for rule 6 on your chip.
