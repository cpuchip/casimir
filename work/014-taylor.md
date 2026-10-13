# CAS work unit 014: Taylor series

**From:** abacus · **To:** casim · **Status:** ready.
- It's casim's proposal (#7661), from a measured C prototype in scratch over 021d6ef.
- It's first in the rest of v1.0's order (Michael, 2026-10-13: "see if we can knock a few more of those items out
  tonight"): Taylor series, then limits (CAS 015, on the same derivative machinery), sums, integration by parts and by
  substitution, complex roots.
- The budget is decision 80's: 262,144 B for v1.0.

## Why

A Taylor polynomial is first-year calculus's bridge from derivatives to approximation, and v1.0's scope (decision 69)
names series. It also gives CAS 015 its engine: a limit of 0÷0 is a question about the first terms of two series.

## Rules

1. **The op:** casim_taylor(eqn, var, point, n, model, angle, out, cap, arena, arena_cap), and CASIM_TAYLOR in
   casim_run, with the point in repl. **The order goes as text in repl2**, as definite integrals carry their upper
   bound; casim_req doesn't change.
   - ~~The order goes in a new `int order` appended to casim_req after repl2, so older initializers keep compiling~~:
     wrong twice over. First, gcc's -Wmissing-field-initializers (in -Wextra, and the firmware builds with -Werror)
     rejects a positional initializer that leaves the field out (casim #7676). That led to option (a), `, 0` on every
     initializer.
   - ~~Option (a)~~, reversed on casim's measurement #7705: do_irsteps copies the struct (`sub = *rq`), so the field
     grows an existing frame, 544 → 560 B. Its control was the accepted casim.c compiled against the new casim.h alone.
     Rule 8 says no existing frame moves, so (b): the order as text, no struct change, and the firmware's initializer
     is untouched.
2. **The order** n is 0 to 12, as solve's bound. Anything else is BAD_ARG. Order 0 is f(a) alone.
3. **The point** a is a real constant, read exactly as a definite integral's bound (read_bound): π÷3, 1÷2, −2, SQRT(2).
   - A letter in the point is BAD_ARG.
   - An undefined point (1÷0) is UNDEFINED.
   - f may hold letters other than var; they're kept as constants, as derive keeps them.
4. **The engine:** for k = 0..n, evaluate f^(k) exactly at a (check_rewrite, CAS 008's exact values), divide by k!,
   and keep c_k; then differentiate.
   - Each step compacts the arena to the current derivative and the coefficients so far.
   - The derivatives are derive's own, so DEG and GRAD carry π÷180 and π÷200 by the chain rule, as derive does.
5. **Exact coefficients, never rounded.** A coefficient prints in Casimir's canonical form: a terminating decimal
   (0.0390625) or a quotient (1÷3), SQRT(3)÷2, EXP(1), π powers, and never an approximation.
6. **The printed form:** ascending powers of (X−a), the Taylor spelling, under a rule like partfrac's rule 4.
   - c_0, then c_1×(X−a), then c_2×(X−a)^2, …; each c_k canonical; a zero term is left out; a coefficient of 1 is not
     printed, and −1 prints as a leading minus.
   - At a = 0 the powers are of X itself: 1+X+0.5×X^2+…
   - (X−a) is the canonical form of X−a, so a = −2 reads (X+2), and a = π÷3 reads (X-π÷3).
   - When every coefficient is 0 the answer is 0.
   - Not the canonical form of the whole sum: that reorders the terms and splits the linear one (casim #7661 (4)).
7. **Statuses:**
   - UNDEFINED when f or one of the derivatives up to n is undefined at a: LN(X) at 0, 1÷X at 0, SQRT(X) at 0 to
     order 1 and up.
   - A removable singularity is UNDEFINED too in this unit (SIN(X)÷X at 0). Its series is CAS 015's question
     (limits), and this rule may be revised then.
   - **The typed f decides, not its canonical form** (abacus's pre-check, #7700, and casim #7701): X÷X×X at 0 is
     UNDEFINED, although canon cancels it to X. Before canonicalizing f, a is substituted into the typed tree inside one
     canonicalization (sub_var/sub_val, as subst does); a typed f with no value at a is UNDEFINED. This is integrate's
     guarantee too (CAS 013 rule 8, CAS 006): no op answers where the typed input is undefined. The derivatives still
     come from the canonical f, as derive's do. An example row shows it.
   - NO_ROOM when the derivatives swell past the arena (EXP(SIN(X)) to 8 in 8,192 B): never an overrun.
   - TOO_LONG past 191 characters.
   - BAD_ARG for an equation, a letter in the point, or an order outside 0..12.
8. **Bounds, decision 68 with decision 80's budget:**
   - **A host-only inlining shift** (casim #7705): host gcc 15 inlines small() into chain once Taylor's calls exist
     (chain 96 → 112). Bisection found no single call that does it, and a same-size control doesn't. The S3, the real
     target, decides. If the S3's table shows chain unmoved, the host table is re-recorded for chain with this reason.
     If it moves on the S3, the bench's worst stack decides, and a worse worst means more digging, not a raised number.
   - **Frames:** no existing frame moves (the host frame-table gate from CAS 013). New frames: do_taylor 256 B and
     casim_taylor 48 B as prototyped.
   - **Arena:** 8,192 B. Past it the answer is NO_ROOM.
   - **Flash:** the prototype core is +1,333 B, and the estimate for the unit is 3 to 4 KB, steps excluded. Stop and
     report at twice that.
   - **Time:** the S3 bench's per-row times, in the build's report.
9. **Steps, as a second build after the answers are accepted** (casim #7661 (6), accepted as drafted):
   - for each k: DERIVATIVE, a result line f^(k)(X) = … with no sub-walk of derive; AT POINT, f^(k)(a) = v; TERM,
     c_k = v÷k! and its term;
   - then the answer, byte for byte (#6251's rule for every op);
   - the rule ids come from the next free numbers in the steps block (256 and up), added only at the end.

## Acceptance

- **Visible examples,** work/014-examples.txt, written by casim from the rules:
  - SIN, COS, EXP, LN(1+X) and ATAN at 0;
  - TAN at 0 to order 9;
  - LN(X) at 1; COS at π÷3; EXP at 1;
  - SIN(X) in DEG (π÷180 in each coefficient);
  - SQRT(1+X); a product (EXP(X)×SIN(X)); a polynomial (exact to its degree, and 0 beyond);
  - a letter kept as a constant (EXP(K×X));
  - order 0;
  - LN(X) at 0 and SIN(X)÷X at 0 (UNDEFINED), EXP(SIN(X)) to 8 (NO_ROOM), COS at π÷3 to 12 (TOO_LONG), a letter in
    the point and order 13 (BAD_ARG).
- **Hidden, by abacus** (tools/refmodel/gen_cas014.py and judge_cas014.py):
  - random expressions at 0 from X, rational constants, +, −, ×, ÷, integer and fractional powers, EXP, SIN, COS and
    LN, to orders 1 to 10;
  - the truth at 0 is exact power-series arithmetic over fractions (series_truth.py), not derivatives, and every case is
    also checked against mpmath's numerical Taylor at 80 digits and sympy's series before it's kept;
  - points other than 0, and DEG and GRAD, judged numerically: the answer's coefficients against mpmath's Taylor at 80
    digits, relative error under 1e-40;
  - the printed form (rule 6): ascending powers, no zero term, no coefficient written as 1;
  - statuses: UNDEFINED exactly where the series doesn't exist at a; NO_ROOM and TOO_LONG allowed anywhere, counted;
    a removable singularity UNDEFINED (rule 7) or, if CAS 015 revises rule 7, right.
- **Mutants:**
  - k! dropped (or (k−1)!);
  - the sign of a in (X−a) flipped;
  - the angle factor dropped in DEG;
  - a zero term printed;
  - one order short (stopping at n−1);
  - the point rounded to a decimal before evaluating.
- **The device:** the S3 bench byte-identical to the host. Golden byte-identical for every other op, and every earlier
  hidden set rerun.

## Accepted (abacus, 2026-10-13), at ad3c4ee

**The hidden 2,000 on the commit itself,** built on fermion in a capped container from a clean clone (abacus's
gen_cas014 seed 14014, judge_cas014 as tightened for rule 7's amendment):
- **0 failures.**
- ANSWER: 527 right, 255 NO_ROOM, 1 TOO_LONG.
- NUMERIC: 524 right, 107 NO_ROOM, 184 TOO_LONG.
- UNDEFINED 184/184; removable (EITHER) 218 UNDEFINED, all right.

`make check` is green there (276 checks). casim's own report: golden 9,391 with none moved, 165 mutants caught
(the six above among them), and on the S3:
- no existing frame moved (chain 64 B before and after, so the host's 96 → 112 is re-recorded under rule 8);
- text +3,730 B, inside the estimate;
- the bench byte-identical, with the gate unchanged at 11,712 of 12,288.
- The new do_taylor frame is 304 B on the S3, against the prototype's 256. It's a new frame, not on the worst path,
  and is named here.

**Against the 08:00 pre-check build, on the same 2,000 cases,** the commit answers 59 fewer:
- ANSWER: 48 more NO_ROOM;
- NUMERIC: 73 more TOO_LONG, but 62 fewer NO_ROOM.
- The cause is (b) below: each derivative now goes through 384 B of text. These are honest refusals, which rule 7
  allows, but the coverage given up is measured here and is the first follow-up.

**Rulings on casim's questions (#7863):**
- **(a) X×ABS(X) at 0, order 1: UNDEFINED.** Accepted as a documented limitation. The series does exist to order 1
  (f'(0) = 0), so UNDEFINED overstates it. CAS 015 brings CASIM_UNDECIDED. The switch to UNDECIDED for ABS's corner
  under a smoothing factor goes in the small follow-up unit with (b) and (e), not in 015 (casim #7883): Taylor's
  SGN-of-0 check can't yet tell ABS(X), a real corner and rightly UNDEFINED, from X×ABS(X), so a blanket switch
  would be wrong. docs/language.md's Taylor section gains casim's sentence on it in the 015 commit.
- **(b) The 384-byte text per derivative.** Accepted. Follow-up for casim, not blocking: measure the hidden set's
  answers with the two buffers at 384, 768 and 1,024 B against the arena's room for trees, and propose the size. The
  number goes in the docs with the swell sentence.
- **(c) A fractional power of a negative number is UNDEFINED,** as CAS 009's convention. Accepted: one domain rule
  for every op.
- **(d) 0^K stays symbolic with letters** (X^K at 0 prints 0^K), as subst and simplify. Accepted.
- **(e) TAN(π÷2) left unevaluated by subst and simplify.** Ruled: an exact value at TAN's poles, (2k+1)×π÷2, is
  UNDEFINED in every op, as Taylor already does. It's a small fix unit after CAS 015; casim sizes it.
- **(f) NOT_DIFFERENTIABLE for the names derive refuses, and leading zeros in the order ("012").** Accepted.
- **(g) mutants.py re-records the frame table in each copy.** Accepted with thanks. 35 of 108 mutants had been
  "caught" by the frame gate alone since CAS 013. A test that can't fail for the right reason isn't evidence.

**Rule 6, clarified, not waived.** Each term prints as the canonical form of c_k×(X−a)^k, so a symbolic factor of c_k
can follow the power: COS at π÷3 prints -0.5×(X-π÷3)×SQRT(3), and DEG prints X×π÷180. The rule's purpose holds:
- terms ascending;
- no zero term;
- no 1×;
- the linear term whole (casim #7661 (4) was about the whole sum's canonical form).
It matches how every other op prints a product. abacus missed this at the pre-check: the judge's form check doesn't
test factor order within a term, and the pre-check's README showed the line without a remark. If Michael prefers the
textbook spelling (-SQRT(3)÷2×(X-π÷3)), it's a printer change in its own small unit.
