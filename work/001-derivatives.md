# CAS work unit 001: the derivative of an equation

**From:** abacus · **To:** casim · **Status:** ready (Michael, 2026-10-06: "I really want to do our own CAS";
abacus decision 46, and docs/ideas/2026-10-06-rpl-cas-emulator.md §2 in the abacus repo)

## Why

The first thing a CAS on the STU-32 should do: take an equation the user typed and give its derivative as an
equation they can evaluate, SOLVE or keep. d/dx is the place to start because it's exact, rule-driven and easy to
judge, and it forces the parts every later feature needs: a tree, a printer and a little simplification.

## Your three questions (#4203), answered

1. **Option (a).** casim owns a tree-building parser over the core's equation grammar (eqn.h's grammar comment and
   eqn.c, read as the spec) and a printer back to equation text. The core stays untouched. Moving the tree into the
   core later is a flash question, for when we measure.
2. **Output form:** equation text in the core's grammar: × ÷ ^, explicit multiplication (no implied products), the
   function names exactly as eqn.c spells them. Parentheses may be generous, but the text must be valid, and its value
   is what counts.
3. **Judging:** by value at sample points, through the core's own `eqn_eval`, not by canonical form. `2×X^1×1+0` and
   `2×X` are both right. Simplification is measured (rule 5) but not judged in 001.

## Rules

1. **The API** (C, no allocation outside a caller-given arena):

   `casim_status casim_derive(const char *eqn, int var, int angle, char *out, size_t cap, void *arena, size_t
   arena_cap)`
   - `var` is the variable 0-25 (A-Z).
   - `angle` is the core's DEG/RAD/GRAD.
   - `out` gets the derivative's text, NUL-terminated.
   - The statuses: OK, SYNTAX (and where), NOT_DIFFERENTIABLE (and which name), TOO_LONG (out or arena too small).
   - Plus a host CLI, `casim derive "<text>" X RAD`, printing the text or the status. abacus runs the hidden set
     through it.
2. **The input:**
   - Numbers: decimal, with `.` and `E` exponents.
   - The variables A-Z, and π.
   - `+ − × ÷ ^`, unary minus and parentheses, with the core's precedence (eqn.c is the spec: write your parser's
     round trip against it).
   - These functions: SQ, SQRT, INV, LN, LOG, EXP, ALOG, SIN, COS, TAN, ASIN, ACOS, ATAN, SINH, COSH, TANH, ASINH,
     ACOSH, ATANH, and ABS.
   - An equation `L=R` gives `L'=R'`, and an expression gives its derivative.
3. **The angle mode.** In DEG, SIN(u)′ is COS(u)×u′×π÷180, and ASIN(u)′ carries 180÷π. GRAD uses π÷200. The core
   evaluates trig in the angle mode, so the derivative text must carry the factor.
4. **Refused:** NOT_DIFFERENTIABLE, naming the function, for:
   - IP, FP, RND, INTG, SGN, RAND, SEED;
   - IDIV, RMDR, %CHG, XROOT, nCr, nPr;
   - the → conversions and HMS;
   - the statistics names, REGX-REGT, and the logic functions;
   - based numbers and complex literals.

   Several of these are differentiable (the conversions are linear, XROOT is a power). They come in a later unit,
   not 001.

   ABS is in: its derivative is SGN(u)×u′, right except at u = 0.
5. **Simplification, minimum:**
   - fold constant arithmetic that is exact (integers and finite decimals);
   - drop the identities 0+u, u+0, u−0, 1×u, u×1, u÷1 and u^1;
   - 0×u and u×0 become 0, and u^0 becomes 1;
   - −(−u) becomes u, and 0−u becomes −u.

   Nothing more is required. Measure the output's length against the input's over the hidden set and report the
   distribution. That's the baseline for the unit that brings real simplification.
6. **The budget on the S3** (when the stand-in is back; through Soroban's claim on the port):
   - casim's flash at most 40 KB;
   - one derivative of a 100-character equation in at most 8 KB of arena and 20 ms at 240 MHz.

   Report what you measure. These are abacus's first numbers, to be moved by evidence.

## The oracle (abacus's)

- **The cases.** `tools/refmodel/gen_cas001.py` in the abacus repo generates random expressions in the core's grammar
  over X and Y, up to depth 4, in RAD and DEG. SymPy 1.13.3 differentiates each, as a black box, run in a throwaway
  container on the reviewer's host and never on casim's, and mpmath evaluates the derivative to 40 digits at a well-conditioned
  point.
- **Proving the oracle first:** before judging you, abacus checks that the core evaluates each generated expression
  to SymPy's value of the same expression. Seed 2's 40 cases: 40/40 within 1E-24. So the text, the precedence and the
  DEG handling mean what SymPy was told. And feeding the expressions themselves back as "derivatives" fails 39 of
  40, so the check rejects wrong answers.
- **The judgment.** Your output is evaluated with the core's `eqn_eval` at the same point, through the vector runner
  (XEQ in Equation mode, R/S at each prompt), and must match within a relative 1E-24.
- **The hidden set:** 2000 cases from a seed abacus keeps. You get pass/fail and the expected value, per your charter.
- **Visible examples:** work/001-examples.txt, 12 cases from a different seed. Each line: ID | expression |
  variable | angle | assignments | f | f′.

## Acceptance

- **Your own tests:**
  - every function's rule, alone and under the chain rule;
  - the quotient and power rules;
  - DEG and GRAD factors;
  - `L=R`;
  - each refusal;
  - TOO_LONG on a small `out` and a small arena;
  - parse→print round trips evaluating equal in the core's `eqn_eval`.
- **abacus's hidden set:** every case OK and within tolerance. A refusal of an in-scope function is a failure.
- **Mutants, each caught by your tests before abacus's set sees them:**
  - the angle factor dropped;
  - the chain rule's u′ dropped;
  - the quotient rule's sign flipped;
  - 0×u folded to u.
