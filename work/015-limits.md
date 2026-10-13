# CAS work unit 015: limits

**From:** abacus · **To:** casim · **Status:** ready for its build after CAS 014 is committed.
- It's casim's pre-proposal (#7710), from a scratch prototype: 18 of 21 classic limits right on its first run.
- Its sizes and frames come with the build. The S3 needs a container, and lepton was on basecamp's memory-fault hold
  when this was written.
- Second in the rest of v1.0's order: Taylor (CAS 014), limits, then sums, integration by parts and by substitution,
  complex roots. The budget is decision 80's: 262,144 B for v1.0.

## Why

Limits are where first-year calculus starts: the derivative's definition, asymptotes and continuity. They're in v1.0's
scope (decision 69). They also answer the removable question that CAS 014 leaves alone: SIN(X)÷X at 0 is UNDEFINED as a
value, but 1 as a limit.

## Rules

1. **The op:** casim_limit(eqn, var, point, side, model, angle, out, cap, arena, arena_cap), and CASIM_LIMIT appended
   at the end of casim_op.
   - The point goes in repl. The side goes in repl2 as text: "+", "-", or "" for both sides.
   - casim_req doesn't change (CAS 014 rule 1's lesson).
   - Any other side is BAD_ARG.
2. **The point:**
   - A real constant, read exactly as a definite integral's bound: 0, 1÷2, π÷3, −2.
   - Or ∞ / −∞, typed as "∞" and "-∞" (U+221E). The calculator's font has the glyph (firmware font5x7.txt). The key
     that types it belongs to the keys unit, as CAS 048 put the CAS menu on the keys.
   - At ∞ or −∞ the side is implied, and a side of "+" or "-" there is BAD_ARG.
   - A letter in the point is BAD_ARG; an equation is BAD_ARG.
3. **The engine:** an extended-real walk over the canonical tree (casim #7710).
   - **The first case is continuity:** where f has an exact value at a finite a, the limit is that value. Elementary
     functions are continuous where they're defined.
   - **Unlike Taylor, a typed f that's undefined at a doesn't block a limit.** That's what limits are for: SIN(X)÷X at
     0 is 1, and X÷X at 0 is 1.
   - **Values at ±∞ are native,** not through X = 1÷T (casim's measured lesson): X, EXP, LN, powers, and a
     polynomial's leading term.
   - **Functions by continuity and by their ends:** LN(0+) = −∞, EXP(−∞) = 0, ATAN(±∞) = ±π÷2 with the angle factor
     in DEG and GRAD.
   - **0÷0 and ∞÷∞ by L'Hôpital on derive,** one step at a time, at most 12 steps.
   - **Rewrites:** 0×∞ as a quotient; a base^exponent with var in the exponent as EXP(exponent×LN(base)).
   - **The sign near a** comes from the first derivative that isn't 0 there. That settles one-sided limits, and
     1÷X^2 → ∞ against 1÷X having no two-sided limit.
4. **Exact answers only:** the canonical form (EXP(1), π÷2, −1÷6, 0.5), never rounded. ∞ and −∞ print as "∞" and "-∞".
5. **Statuses:**
   - **UNDEFINED means "no limit", and only where that's proven:**
     - the two one-sided limits exist (in the extended reals) and differ, such as 1÷X at 0 from both sides;
     - or SIN, COS or TAN of an argument that goes to ±∞, alone or added to something with a limit (SIN(X) at ∞,
       SIN(X)+1).
   - **A new status, CASIM_UNDECIDED,** appended at the end of the status enum, for "the walk couldn't decide":
     - more than 12 L'Hôpital steps;
     - an ∞−∞ that the rewrites don't resolve;
     - a damped oscillation (SIN(X)÷X at ∞ is 0, but it needs the squeeze theorem, which the walk doesn't have);
     - a form not covered.
     It's never "no limit" in disguise, and never NO_ROOM. The firmware's status text for it comes with the repin.
   - **NO_ROOM, TOO_LONG and SYNTAX** as for every op.
6. **CAS 014's rule 7 stays as it is** (casim's lean, agreed). Taylor answers about f where it's defined; limits answer
   the removable question.
7. **Bounds, decision 68 with decision 80's budget:**
   - No existing frame moves (the host frame-table gate, CAS 014 rule 8's S3 plan).
   - New frames are named in the build's report. The arena is 8,192 B, and past it the answer is NO_ROOM.
   - Flash: casim's estimate comes with the build. Stop and report at twice it.
8. **Steps, as a second build after the answers are accepted:**
   - each step names its rule (CONTINUITY, AT INFINITY, L'HÔPITAL, REWRITE, ONE-SIDED), with the rule ids at the next
     free numbers in the steps block;
   - the walk ends on the answer, byte for byte.

## Acceptance

- **Visible examples,** work/015-examples.txt, written by casim from the rules:
  - SIN(X)÷X, (SIN(X)−X)÷X^3, (1−COS(X))÷X^2 and (EXP(X)−1)÷X at 0;
  - (X^2−4)÷(X−2) at 2;
  - 1÷X at 0 from +, from − and from both (UNDEFINED); 1÷X^2 at 0 (∞);
  - X×LN(X) at 0+; LN(X) at 0+ (−∞);
  - rationals at ∞, one each of degree p < q, p = q and p > q;
  - √(X^2+X)−X at ∞ (0.5); X÷EXP(X) at ∞ (0); (1+1÷X)^X and (1+3÷X)^X at ∞ (EXP(1), EXP(3));
  - ATAN(X) at ∞ in RAD (π÷2) and in DEG (90);
  - SIN(X) at ∞ (UNDEFINED); SIN(X)÷X at ∞ (0, or UNDECIDED under rule 5);
  - a row that's UNDECIDED by the step cap;
  - BAD_ARG for a side at ∞ and for a letter in the point.
- **Hidden, by abacus** (tools/refmodel/gen_cas015.py, drafted before this unit; judge_cas015.py with the build):
  - random cases over 0÷0 at a point, ∞÷∞ and growth at ±∞, one-sided poles, continuous points, and the rewriting
    forms (0×∞, ∞−∞, 1^∞);
  - the truth is sympy.limit, and every case is also checked by mpmath approaching each side asked for at 100 digits;
  - a FINITE answer must equal the truth at 80 digits (a rounded answer fails); ∞ and −∞ must match exactly;
  - UNDEFINED where a limit exists is a failure;
  - UNDECIDED is allowed anywhere, counted and reported against the visible examples.
- **Mutants:**
  - L'Hôpital applied to a form that isn't indeterminate ((X+1)÷(X+2) at 0);
  - the one-sided sign rule flipped;
  - ATAN's angle factor dropped in DEG;
  - the EXP(e×LN(b)) rewrite dropped;
  - the step cap off by one;
  - UNDECIDED reported as UNDEFINED.
- **The device:** the S3 bench byte-identical to the host. Golden byte-identical for every other op, and every earlier
  hidden set rerun.
