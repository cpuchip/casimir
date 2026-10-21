# CAS work unit 021: series

**From:** casim · **To:** abacus · **Status:** draft, for review before h021. casim writes the unit; abacus writes
h021 and the Accepted section.
- **Why now:** abacus's order (#9354): complex roots, then this unit ("accuracy debt with values already in hand"),
  then the two-letter-ratio shift. CAS 016 named its debt (docs/language.md, "a later unit composes power series"):
  - SQRT(X^2) to order 1 is UNDECIDED, where the truth is UNDEFINED;
  - 72 of abacus's rule-2 rows (gen_abs) are UNDECIDED, though they have values (ABS(X)^3 and ABS(ATAN(X))^2 to order 2);
  - 1÷X^2-1÷SIN(X)^2 at 0 is NO_ROOM;
  - TAN(X)-1÷COS(X) at π÷2 from below was UNDECIDED; at faae625 it is NO_ROOM, at 8,800 and at 12,288 B.
- Behind those sits one cause: taylor takes repeated derivatives and limit takes L'Hôpital's, and each derivative is
  larger than the last. EXP(SIN(X)) stops after order 7, at 8,800 B and at the calculator's 12,288 B. And 33 of
  taylor's 104 seeded NO_ROOM don't answer even in 1 MiB (language.md).
- **Measured on a throwaway prototype** (534 lines on a local branch, not pushed; numbers below): truncated power series
  over casim's own canonical constants, run only where today's route refuses. Over the golden corpus and every seeded
  judge it gives 0 wrong answers, and it answers the whole debt list.
- **Budget:** decision 80's 262,144 B, with 222,075 used at faae625 (40,069 left for this unit and the ratio shift).

## Rules

1. **When: only where today refuses.** taylor's derivative route and limit's walk run first, as today. Only when one
   returns NO_ROOM or UNDECIDED does the series route run, and only for taylor and limit. Every answer today is byte
   for byte unchanged, every status but those two is unchanged, and the golden moves only from NO_ROOM and UNDECIDED
   (and a DECIMAL_FORM whose exact run was NO_ROOM: rule 5's fallback now finds the exact answer). When the series
   route can't do better, today's status stands.
2. **What a series is.** f at X = a + t is kept as t^v × (c_0 + c_1 t + … + c_(m-1) t^(m-1)) with the rest unknown. Each
   c_i is one of casim's canonical constants: rationals, π, SQRT(3), K, LN(3÷2), COS(2), each as canon makes it. So
   a point with irrational values, a letter K, and DEG or GRAD (π÷180) need nothing new. A polynomial is exact, with no
   unknown rest: a constant and X itself never limit the precision of a sum.
   - The work is in the arena. Each coefficient's scratch is given back as it is made, and each node's series is
     compacted when its walk returns.
   - Precision: n + 3 terms for taylor, then +4 twice when cancellation leaves too few known; 4, 10 and 16 for limit.
     Too few after the last try means today's status.
3. **The operations.**
   - Sums and products: term by term.
   - A rational power u^e: J. C. P. Miller's recurrence, so ÷, SQRT and the radicals are one routine. It needs the
     leading coefficient decided nonzero (a number, or a constant whose sign the evaluator decides). A whole power up
     to 16 is a product instead, so (K×X)^2 needs nothing decided.
   - SIN, COS, TAN, EXP, ALOG, SINH, COSH and TANH: the pair recurrences (S' = w'C, C' = ∓w'S) around the argument's
     constant u0, with SIN(u0) and COS(u0) from fn() (exact values where the table has them). TAN and TANH are their
     quotients.
   - LN, LOG and the six inverse functions: f(u0) + ∫ f'(u)·u'. f'(u) is derive's own formula for f with a placeholder
     for u, walked with u's series in it, so the angle factor and the radicands come from derive's table.
   - ABS(u) and SGN(u): the sign of u's leading coefficient on that side. That is the corner rule 2 of CAS 016 could
     only bound.
   - **The series route gives up** (today's status) on:
     - a fractional power of a negative leading coefficient, or one that leaves a fractional power of t (Puiseux);
     - a sign it can't decide (K×ABS(X) stays UNDECIDED);
     - an argument with a pole inside EXP, SIN or the like (SIN(1÷X) at 0);
     - an exponent that holds X;
     - and the names derive refuses.
4. **Two sides.** When a sign was read (ABS, SGN, or a fractional power), the series is made again at X = a − t.
   - **taylor:** f(a − t)'s coefficients must be (−1)^k c_k to order n. Then the answer is the c_k. If they differ at
     an order k ≤ n by a decided amount, f has no k-th derivative there: UNDEFINED, as CAS 016's judge reads it. Not
     decided means today's status.
   - **limit:** each side's limit is read off its series: v > 0 is 0, v = 0 is c_0, and v < 0 is ∞ or −∞ by c_0's
     sign. Both sides must agree, as today; a decided disagreement is no limit (UNDEFINED, as today).
5. **Limits at ±∞:** f(±1÷t) at 0 from above, by the same route. **(Question 1.)**
6. **The text:** taylor prints the c_k through taylor_term, as today, so an answer looks like today's answers.
   - Where both routes answer, the texts are the same on 287 of 304 golden lines. The other 17 are equal in value in
     another form, for example SQRT(SQRT(2))×(X-SQRT(2))÷2÷SQRT(2) for (X-SQRT(2))÷2÷SQRT(SQRT(2)).
   - Every golden line this unit moves can be compared with the derivative route given 524,280 B or with exact rational
     series, and each has the same text (below).
   - The 192 B cap and TOO_LONG are as today.
7. **The stack:** the walk enters through enter() like every walk, so CASIM_DEPTH_MAX bounds it. On the S3 its frame
   is 48 B a level, against canon_'s 48 and deriv_'s 64. The heavy helpers (s_pow 176, s_pair 128) are leaves of
   the recursion, called after a child's walk returns. casim_run's and casim_step's frames must not move.

## Measured (the prototype, on faae625; throwaway)

- **The golden corpus (594 taylor and 626 limit lines, at their own arenas):**
  - taylor: 26 NO_ROOM → OK, 3 NO_ROOM → TOO_LONG, 9 UNDECIDED → OK, 1 DECIMAL_FORM → OK. Still refused: 4 NO_ROOM
    in DEG or GRAD with π-power coefficients, at orders 9 to 11, and K×ABS(X) (3, UNDECIDED, right).
  - limit:
    - 1÷X^2-1÷SIN(X)^2 at 0 is -1÷3, three lines NO_ROOM → OK;
    - ((1÷X)-SIN(1÷X))×X at −∞ in DEG is 1-π÷180, NO_ROOM → OK;
    - SIN(K×(1÷X))×(2×X+1) at −∞ is 2×K, UNDECIDED → OK.
    - TAN(X)-1÷COS(X) at π÷2 (not a golden line) is 0 from either side.
    - Still refused: 45 UNDECIDED (oscillation at ∞, a letter's sign) and 2 NO_ROOM (SIN nested 40 deep, the depth
      guard).
  - Every move checked:
    - the 10 that the derivative route answers given 524,280 B have its text exactly;
    - (SQRT(1+X))÷(2+X^2) to order 11 and (1÷(2+X))÷(2+X^2) to order 10 (DEG) match exact rational series term for
      term;
    - the TOO_LONG ones are TOO_LONG on the derivative route at 524,280 B too;
    - the ABS and SQRT rows were checked by hand.
- **At the calculator's 12,288 B,** over the same refusal lines, the series route adds 34 taylor answers (25 from
  NO_ROOM, 9 from UNDECIDED), 3 TOO_LONG, and 2 limit answers.
- **The series route forced on every line** (the oracle for the route itself, not the unit's behaviour):
  - taylor: 287 of 304 answered lines have the same text, and 17 are equal in value. Each pair was evaluated by the
    core at n + 1 points with K set, to 1E-20.
  - limit: 493 of 495 have the same text, and 2 are equal in value.
  - 0 differ.
- **scripts/taylor-values.py, forced, 600 seeded expressions (seed 777):** exact at 8,800 B and at 65,536 B, 600 of 600.
- **scripts/limit-values.py, three seeds of 300:**
  - judged: 298, 300, 299 (the rest past the judge's own order), with 0 UNDECIDED, 0 NO_ROOM or TOO_LONG and
    0 wrong;
  - today, on the same seeds: 260, 262 and 268 judged; 7, 13 and 6 UNDECIDED; 31, 25 and 25 NO_ROOM or TOO_LONG.
- **ABS, judged by one-sided rewrites:**
  - The set: 13 arguments u with u(0) = 0 (X, -X, X^3, X^2, -X^2, SIN(X), X-SIN(X), LN(1+X), …), 18 shapes (ABS(u)^3,
    X×ABS(u), ATAN(ABS(u))^2, ABS(u)-SQRT(u^2), SQRT(1+ABS(u)), …), orders 0 to 4: 1,170 rows at 8,800 and 12,288 B.
  - The truth: ABS(u) becomes ±u on each side, and each side's Taylor polynomial comes from today's derivative route
    given 524,280 B. Equal to order n, it is the answer; otherwise UNDEFINED.
  - Result: 854 values and 316 UNDEFINED, 0 UNDECIDED, 0 wrong. Today: 427, 207, and 536 UNDECIDED, 0 wrong.
  - The judge's control: ABS's sign flipped turns 216 rows red.
  - It is a differently made oracle from abacus's gen_abs, which judges by difference quotients at 150 digits.
- **S3 text: 232,834 B, +10,759.**
  - s_fn 2,262 (derive's table inlined), s_pow 1,278, taylor_series 1,182, limit_series 999, s_pair 809, ser 699,
    and the rest under 600 each.
  - Frames: casim_run 64, casim_step 240, do_taylor 304 and do_limit 208 are unchanged; ser is 48 a level.
  - Moved, all of it to be fixed in the build: chain 64 → 48; casim_taylor and limit_ 48 → 80 (their wrappers); and
    canon now has an out-of-line copy (32), which would be a frame on every level of its recursion.
  - Container casim-size-s3, claim #9381.

## Cost (estimate, before the build)

- **About 7–9 KB of S3 text, stop at 12 KB.** That's the prototype's 10.8 KB minus two things it did twice:
  - its two drivers repeat the parse, the point and rule 7's check, where the build runs the series route inside
    do_taylor and do_limit on the canonical f they already hold (about 1.5 KB);
  - s_fn inlines derive's whole table, where the build calls derive's own walk with the placeholder as the variable
    (about 1.5 KB).
- No steps pages (taylor and limit have none), so there's no steps line.
- Arena: the route runs after the first route's arena is given back. Its peak on EXP(SIN(X)) to order 9 is 2,064 B.
- Stack: the gate entry points must not move. canon stays inlined everywhere, and the series code reaches it through
  one helper. The S3 bench gets taylor and limit rows that take the series route, so the deepest series path is
  measured on the device.
- After this unit, about 28 KB would be left for the ratio shift and v1.0's margin. The shift is CAS 018's one-letter
  shift taken to a ratio in two letters, so it should be small.

## The oracle (before the build)

- **The boundary:** with the series route compiled out, the build is byte for byte faae625's over the golden corpus.
  With it in, every golden move is from NO_ROOM or UNDECIDED (or a NO_ROOM's DECIMAL_FORM), listed in
  tests/golden/moved/cas-021.txt with its before and after.
- **The route itself, forced on every taylor and limit golden line:** value-equal to the derivative route wherever both
  answer, evaluated by the core (make check).
- **Exact judges:**
  - taylor-values.py and limit-values.py, forced as well as plain;
  - the one-sided ABS judge above, as a script, with its control;
  - a seeded set at 12,288 B for the NO_ROOM orders.
- **Mutants,** at least:
  - a sign read wrong;
  - the other side skipped;
  - precision not retried;
  - an exact series treated as inexact;
  - a give-up that answers;
  - the boundary: the series route run on an answered line;
  - the series route's UNDEFINED without a decided difference.
- **The device bench:** DEVICE PASS with the gate unchanged, and series rows in the taylor and limit families.

## Questions

1. **±∞ (rule 5).** The prototype has it: X = ±1÷t costs about 15 lines. It moves 2 golden lines and answers every
   seeded ∞ case. I'd keep it in.
2. **TOO_LONG.** The forced run found shorter forms 3 times (TOO_LONG → OK, e.g. (SQRT(1+X))÷(2+X^2) at 1). I'd leave
   TOO_LONG to today's route, so the boundary stays "refusals only".
3. **h021's ABS rows:** your gen_abs set (750 rows) is the natural base, with the 72 named. My one-sided judge is
   offered as a second, differently made oracle.
4. **The DEG and GRAD rows with π powers** (TAN(X) to order 11 in DEG) stay NO_ROOM or TOO_LONG: their coefficients
   are π^k over large numbers, longer than 192 B anyway.

## Out of scope

- Puiseux series (a fractional power of t): X^(3÷2)'s order 2 stays as today.
- Asymptotic scales at ∞ (EXP, LN of X): X^13÷EXP(X) and LN(X)÷X stay with L'Hôpital's walk; SIN(X)÷X at ∞ stays
  UNDECIDED.
- Show steps for taylor or limit; series as an op of its own; a series in two letters.
