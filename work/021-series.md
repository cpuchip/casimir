# CAS work unit 021: series

**From:** casim · **To:** abacus · **Status:** built (As built and Measured below), for abacus's run of h021
(frozen at #9394: h021.tsv 345 rows, habs2.tsv 750). The draft was revised after abacus's review (#9386: the four
questions ruled, gaps A–F filled in rules 1, 3, 4 and 8, Cost and the oracle; A and C–F accepted in #9388, and rule 8
rewritten after it).
casim writes the unit; abacus writes h021 and the Accepted section.
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
   route can't do better, today's status stands. TOO_LONG stays with today's route (ruled #9386 (2)).
   - **Holes come first** (#9386 A). Today's taylor (do_taylor) runs in this order:
     1. it parses the text and reads the point;
     2. it applies rule 7 to the typed tree, before canon: each part is valued at a (taylor_check), and a hole is
        UNDEFINED;
     3. it makes the canonical f;
     4. order by order, it values f^(k)(a) and checks f^(k) for holes (CAS 016's corner and 0÷0 rules), then derives.

     So a hole in the typed text is UNDEFINED before any derivative is taken: SIN(X)÷X, EXP(SIN(X))×SIN(X)÷SIN(X)
     to order 9, and a divisor that is 0 only by an identity (SIN(1)^2+COS(1)^2-1+X) are all UNDEFINED at faae625 and
     on the prototype. **The series route runs only after step 2 has finished.** If step 2 itself runs out of room,
     the series route doesn't run, and today's NO_ROOM stands.
   - Inside the series walk, for taylor (#9386 A, D), these give up and never answer:
     - a negative power (a divisor) of a series whose leading order is above 0 at a;
     - a function at a pole, or with no value at u0;
     - a result whose own order is below 0 (a pole at a).

     Rule 7 has already valued every typed part at a, so these should be rare without a typed hole. Where one occurs,
     giving up keeps today's status and costs no answer. For limit, the point is the hole, and they are answered
     (rule 4).
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
     - LN, LOG or an inverse function where f(u0) has no value (LN(X) and LN(SIN(X)) at 0, ATANH(u) at u0 = ±1), and
       an integrand whose order is below 0: a t^-1 term is a logarithm, never a power (#9386 C). Both are guarded, and
       either one is a mutant;
     - a fractional power of a negative leading coefficient, or one that leaves a fractional power of t (Puiseux).
       ASIN and ACOS at ±1 reach this through their radicand's power -1÷2, except where the radicand's order is even:
       ASIN(COS(X)) at 0 is π÷2-ABS(X), whose sides differ at order 1, so UNDEFINED, which is true;
     - a sign or a zero it can't decide (rule 8): K×ABS(X) stays UNDECIDED;
     - an argument with a pole inside EXP, SIN or the like (SIN(1÷X) at 0);
     - an exponent that holds X;
     - and the names derive refuses.
4. **Two sides.** When a sign was read (ABS, SGN, or a fractional power), the series is made again at X = a − t.
   - **taylor:** f(a − t)'s coefficients must be (−1)^k c_k to order n. Then the answer is the c_k. If they differ at
     an order k ≤ n by a decided amount, f has no k-th derivative there: UNDEFINED, as CAS 016's judge reads it. Not
     decided means today's status.
   - **limit:** each side's limit is read off its series: v > 0 is 0, v = 0 is c_0, and v < 0 is ∞ or −∞ by c_0's
     sign. Both sides must agree, as today; a decided disagreement is no limit (UNDEFINED, as today).
5. **Limits at ±∞:** f(±1÷t) at 0 from above, by the same route (ruled in, #9386 (1)).
6. **The text:** taylor prints the c_k through taylor_term, as today, so an answer looks like today's answers.
   - Where both routes answer, the texts are the same on 287 of 304 golden lines. The other 17 are equal in value in
     another form, for example SQRT(SQRT(2))×(X-SQRT(2))÷2÷SQRT(2) for (X-SQRT(2))÷2÷SQRT(SQRT(2)).
   - Every golden line this unit moves can be compared with the derivative route given 524,280 B or with exact rational
     series, and each has the same text (below).
   - The 192 B cap and TOO_LONG are as today.
7. **The stack:** the walk enters through enter() like every walk, so CASIM_DEPTH_MAX bounds it. On the S3 its frame
   is 48 B a level, against canon_'s 48 and deriv_'s 64. The heavy helpers (s_pow 176, s_pair 128) are leaves of
   the recursion, called after a child's walk returns. casim_run's and casim_step's frames must not move.
8. **Deciding zero** (#9386 B; rewritten after #9388, which showed the first version unsound). One test serves every
   decision the route makes. A coefficient is:
   - **zero** only as canon's literal 0;
   - **nonzero** as a rational other than 0, or when its value is past 8 times an error bound carried beside it;
   - **undecided** otherwise, and the route gives up.

   The bound (s_eval) walks the constant as const_val does. It takes each part's value from const_val, the value used
   everywhere, and carries the error:
   - every rounding adds 1E-32 of the magnitude rounded, about 20 times decimal128's half unit; a number and π are
     one rounding each;
   - a sum adds its parts' bounds;
   - a product scales each part's bound by the other factor: |w|·b_t + |t|·b_w + b_w·b_t;
   - a power x^e scales x's relative bound by 2|e| (refused unless |e| times it is under 1÷2), and adds (130 +
     |e·LN|x||) roundings for computing x^e itself;
   - a function scales its argument's bound by twice its derivative's size there, and adds its own rounding,
     1E-32 × (1 + |value| + |argument|). It is refused near a singularity:
     - LN and LOG within 4 bounds of 0;
     - ASIN, ACOS, ACOSH and ATANH within 8 bounds of ±1;
     - TAN where the bound times 1+TAN^2 passes 1÷10;
     - EXP, ALOG, SINH and COSH where the argument's bound passes 1÷2.

     SIN, COS and TAN take the angle factor first, as const_val does;
   - SGN is decided only when its argument is past twice its bound;
   - a letter is undecided;
   - the walk goes at most 8 deep, and a deeper constant is undecided. On the S3 its frame is 176 B a level, against
     canon_'s 48, so it isn't left to CASIM_DEPTH_MAX's 44.

   The decisions it serves:
   - the leading coefficient a ÷, a radical or a fractional power needs (Miller), and ABS's and SGN's sign;
   - rule 4's "differ by a decided amount";
   - limit's sign of ∞;
   - the two sides' disagreement.

   The order v comes only from taking literal zeros off the front. So a coefficient that is truly 0 but not literal
   (SIN(1)^2+COS(1)^2-1) keeps its place, and any decision on it is undecided. Where no decision is needed (a sum, a
   product, a taylor coefficient printed as it is), it is carried as the value-correct constant it is.

   Measured on the prototype, with Z = 10^25×SIN(1)^2+10^25×COS(1)^2-10^25 (exactly 0) and k = 20, 30, 40:
   - The bound gives up on every one of these rows, where deciding from the plain value answers:
     - (1÷X^2-1÷SIN(X)^2)+(Z×10^k)÷X at 0: the plain value gives UNDEFINED;
     - (1÷X^2-1÷SIN(X)^2)+(Z×10^k)÷X^2 at 0+: ∞;
     - ABS(Z×10^k+X)×EXP(SIN(X)) to order 9: a polynomial.

     The truths are -1÷3, -1÷3 and UNDEFINED.
   - EXP(SIN(X))÷(Z×10^k+2+X) to order 9 is TOO_LONG either way.
   - No line moves on the golden corpus or on any seeded judge, with or without the bound.
   - The mutant: deciding from the plain value. The first rows above catch it.
   - ABS(X×(Z×10^k)+X^2)×EXP(SIN(X)) is UNDEFINED with the series route off too: that is today's corner rule
     (taylor_corner), and so the follow-up unit's.
   - **What this test found in today's routes** is reported in "Found in today's routes" below. Rule 8 itself changes
     only the series route.

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
- **S3 text: 232,834 B, +10,759,** before rule 8's bound. The bound adds 2,775 (s_eval 1,620, s_dsize 813, small
  helpers), so the prototype as it stands is 235,705 (+13,630; claim #9389).
  - s_fn 2,262 (derive's table inlined), s_pow 1,278, taylor_series 1,182, limit_series 999, s_pair 809, ser 699,
    and the rest under 600 each.
  - Frames: casim_run 64, casim_step 240, do_taylor 304 and do_limit 208 are unchanged; ser is 48 a level.
  - Moved, all of it to be fixed in the build: chain 64 → 48; casim_taylor and limit_ 48 → 80 (their wrappers); and
    canon now has an out-of-line copy (32), which would be a frame on every level of its recursion.
  - Container casim-size-s3, claim #9381.

## Found in today's routes (while answering B; not this unit's change unless ruled)

~~const_sign, the test today's routes use, decides nonzero when |v| > 1E-20, absolutely. A constant whose terms pass
about 10^20 keeps rounding digits above that, so an identity zero is taken for a number.~~ **Corrected after the build
(a traced prototype, #9408):** three tests, not const_sign alone, take a rounded identity zero for a number. limit's
lm_sgn tests the rounded value for exactly 0; taylor_hole's value test finds no pole where 1÷Z has a value; const_sign
(nonzero past an absolute 1E-20) decides only the ABS row. Z = 10^25×SIN(1)^2 + 10^25×COS(1)^2 - 10^25 is exactly 0
(its rounded value is 2E-9), and at faae625:
- limit (Z+X)÷X at 0 is UNDEFINED, and (Z+X^2)÷X^2 at 0+ is ∞. Both are 1.
- taylor 1÷(Z+X) at 0 to order 1 is -1÷(10^25-…)-X÷(10^25-…)^2, and ABS(Z+X) to order 1 is Z+X. Both are UNDEFINED
  (a pole, and ABS(X)'s corner).
- With 10^10 or 10^15 in place of 10^25, limit gives 1. With 10^20, it gives UNDEFINED. (The rounded Z is exactly 0 at
  10^10 and 10^15, and 2E-14 at 10^20: the 1 is the rounding's luck, not a tolerance.)

These are wrong answers in accepted code (CAS 014–016), not refusals. Fixing ~~const_sign~~ these tests with rule 8's
test would move today's answers, which crosses this unit's boundary. **Ruled (#9388):** it becomes its own small unit after 021,
reusing 021's decider, so the zero test exists once. Its golden moves get listed, and abacus writes its hidden rows
from these four and the 10^k sweep. abacus puts it to Michael as a known issue: only contrived inputs reach it, but
it's a wrong answer, not a refusal.

**A second, found by the build's own rows (the same family).** Today's limit walk compares the two one-sided limits by
their canonical form, and any difference is "no limit". K×ABS(X)÷X at 0 is K from above and -K from below, so faae625
answers UNDEFINED; with K = 0 the limit is 0. **Restated after acceptance (#9412):** with a letter, UNDEFINED is the
generic answer under the library's own convention (simplify K÷K is 1; limit X÷(K×X) at 0 is 1÷K), so the letter rows
are a convention question, not a bug (limit 1÷(K×X) at 0 is UNDECIDED, so limit isn't uniform about a letter). The
letter-free shape is wrong: (10^25×SIN(1)^2+10^25×COS(1)^2-10^25)×ABS(X)÷X at 0 is UNDEFINED, truth 0. So do (K-1)×ABS(X)÷X, K^2×ABS(X)÷X and SQRT(K)×ABS(X)÷X at 0, and
K×ABS(X-1)÷(X-1) at 1. The series route alone says UNDECIDED on each (rule 4's disagreement needs 2×K's sign decided,
and rule 8 doesn't decide a letter). It's an answer in accepted code (CAS 015), so not this unit's to move; the
follow-up unit's decider could test the sides' difference too. The fixed row (1÷X^2-1÷SIN(X)^2)×K×ABS(X)÷X, which
the walk refuses and the route answers UNDECIDED, keeps the route's own test honest (tests/series_test.c).

## Cost (estimate, before the build)

- **About 10–11 KB of S3 text, stop at 12 KB.** That's the prototype's 13.6 KB (rule 8's bound included) minus two
  things it does twice:
  - its two drivers repeat the parse, the point and rule 7's check, where the build runs the series route inside
    do_taylor and do_limit on the canonical f they already hold (about 1.5 KB);
  - s_fn inlines derive's whole table, where the build calls derive's own walk with the placeholder as the variable
    (about 1.5 KB).

  The margin to the stop is thin. If the merges save less, the stop line is where I report: before the build goes
  past it, not after.
- No steps pages (taylor and limit have none), so there's no steps line.
- **Reported as it lands** (#9386 E): S3 text after each piece (the series core, the taylor driver merged into
  do_taylor, the limit driver merged into do_limit, derive's walk in place of its table, and the relative zero test),
  each against the 12 KB stop, with rule 8's bound as its own line. Past 12 KB I stop and report; past 28 KB left for the ratio shift and v1.0's margin, it
  goes to Michael.
- Arena: the route runs after the first route's arena is given back. Its peak on EXP(SIN(X)) to order 9 is 2,064 B.
- Stack: the gate entry points must not move. canon stays inlined everywhere, and the series code reaches it through
  one helper. The S3 bench gets taylor and limit rows that take the series route, so the deepest series path is
  measured on the device.
- After this unit, about 28 KB would be left for the ratio shift and v1.0's margin. The shift is CAS 018's one-letter
  shift taken to a ratio in two letters, so it should be small.

- **Device time** (#9386 F): on a refusal, the S3 runs today's route to NO_ROOM and then the series route. The bench
  times every row (bench_main.c, esp_timer). The report gives the series rows' times beside today's time for the
  same row, and names the slowest, not only DEVICE PASS.

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
  - the series route's UNDEFINED without a decided difference;
  - deciding zero from the plain value (rule 8);
  - LN's give-up removed: f(u0) without a value used, and a t^-1 integrand integrated (rule 3);
  - in taylor, a divisor of order above 0 answered, and a result of order below 0 answered (rule 1);
  - the series route run after rule 7's own check ran out of room (rule 1).
- **The device bench:** DEVICE PASS with the gate unchanged, and series rows in the taylor and limit families.

## As built (where the code differs from the reviewed text, or settles what it left open)

- **The merges.** The route runs inside do_taylor and do_limit, after today's route, with no second parse of the
  point or rule 7:
  - **taylor:** do_taylor's derivative loop overwrites f with each derivative, so on a refusal taylor_ser reads the
    typed text again (parse and canon; rule 7 has already passed it) and works on top of the point and X - A. Keeping
    the canonical f live across the loop instead moved do_taylor's host frame 448 → 480; reading it again, 448 → 464.
    The text buffers go back to the arena first (c.top), and a NO_ROOM because the buffers don't fit goes to the route
    like any other (with no buffers taken).
  - **limit:** limit_ser works on do_limit's canonical f (at -∞ already f(-X)), from the arena mark taken after the
    point; ±∞ is one substitution, X → 1÷X, at 0 from above.
  - A caller's cap of 0 keeps today's status (finish's TOO_LONG): the route needs somewhere to print.
- **derive's walk in place of its table:** s_fn sets the variable to the placeholder letter and calls deriv() on
  f(placeholder), so LN, LOG and the six inverses take derive's own formulas (angle factor, radicands). s_fn is
  1,320 B on the S3 against the prototype's 2,262.
- **canon through ib_canon:** the route's canon calls go through CAS 017's out-of-line ib_canon (a copy of my own was
  folded into it by gcc, and its frame-table entry vanished), so no out-of-line canon appears.
- **SGN is out of the route.** taylor's and limit's text refuse SGN (NOT_DIFFERENTIABLE, as derive does), and no tree
  the route builds holds one (s_fn walks deriv only for LN, LOG and the inverses), so the SGN branches in s_fn and
  s_eval could never run. Rule 3's "ABS(u) and SGN(u)" is ABS(u) alone.
- **CASIM_SER**, a compile-time setting with no runtime switch: 1 as built, 0 compiles the route out (the boundary),
  2 runs it alone (its own oracle). Nothing in the library reads the environment, and the build has no data or bss.
- **Two of the doc's mutants are left out as unreachable**, each run against the checks below and surviving:
  - taylor's "a result of order below 0": rule 7 has valued every typed part at A, so a pole at A is a typed hole, and
    v only rises by literal zeros; the guard stays;
  - "a t^-1 integrand integrated": f(u0) has a value (s_fval), u's order is 0 or more, and f' has a pole only at a
    branch point (ASIN's ±1), where the integrand's order is v_w÷2 - 1, 0 or more for v_w of 2 or more (v_w = 1 is
    Puiseux and gives up first); the guard stays.

## Measured (the build, over faae625's library)

- **S3 text: 233,299 B, +11,224 over 222,075** (10.96 KiB of the 12 KiB stop, 1,064 B under it), from the per-function
  sections (text, literals and tables) of the same compile as the size table:

  | Piece | B | In it |
  |---|---|---|
  | the series core | 4,851 | s_pow 1,154, s_pair 765, ser 699, s_add 444, s_mul 432, s_dint 339, s_new 256, s_scale 218, s_norm 180, the rest under 100 |
  | **rule 8's bound** (the zero test) | **2,908** | s_eval 1,612, s_dsize 881, s_sign 122, s_lt 118, s_b 98, s_m 77 |
  | s_fn on derive's walk | 1,320 | the prototype's, with derive's table inlined: 2,262 |
  | the taylor driver, merged | 970 | taylor_ser 648, do_taylor +322 |
  | the limit driver, merged | 1,105 | limit_ser 490, limit_side 231, do_limit +384 |
  | functions the compiler moved | +70 | sm_q +61, chain -24, 57 others by 1 to 16 |

  That leaves 28,845 of decision 80's 262,144 for the ratio shift and v1.0's margin.
- **S3 frames:** casim_run 64 and casim_step 240 are unchanged, and there is no out-of-line canon; casim_taylor,
  casim_limit and limit_ stay 48. do_taylor 304 → 336 and do_limit 208 → 240 (the route's call and the arena mark).
  chain 64 → 48: moved by gcc's inlining, as on the prototype; a reach for small() through one wrapper didn't hold it.
  It is smaller, and the gate's worst didn't move. New: ser 48 a level, s_eval 176 (8 levels at most), s_pow 176,
  s_pair 128, s_dsize 112, limit_side 80, s_mul 80, s_fn 64, limit_ser 64, s_add 64. On the host, do_taylor 448 → 464
  and do_limit 304 → 320 (the frame table re-recorded).
- **The device (XIAO S3, both host and chip at 12,288 B):** DEVICE PASS, 1,322 cases, 0 different from the host. The
  gate is unchanged: worst 11,360 masked (STU chain X^ 44 d) + 384 for interrupts = 11,744 of 12,288, 544 left. The
  series rows' stacks peak at 5,168 B (SQRT(1+ nested 20, the bound 8 deep under 43 levels of the walk).
- **Device times** (median of 15, casim_run as the calculator calls it). "Today" is the same 19 rows on the same chip
  with the route compiled out (CASIM_SER=0), DEVICE PASS too:

  | Row | today ms | today | build ms | build |
  |---|---|---|---|---|
  | EXP(SIN(X)) to 12 | 71.4 | NO_ROOM | 51.1 | OK |
  | SIN(COS(X)) to 10 | 70.9 | NO_ROOM | 47.7 | OK |
  | TAN(X) to 11, DEG | 167.1 | NO_ROOM | 207.3 | NO_ROOM |
  | (SQRT(1+X))÷(2+X^2) to 11 | 98.6 | NO_ROOM | 70.4 | OK |
  | (LN(1+X))×(TAN(X)) to 9 | 105.5 | NO_ROOM | 68.7 | OK |
  | (EXP(K×X))÷(2+X^2) to 7, DEG | 85.2 | NO_ROOM | 50.0 | OK |
  | ABS(X)^3 to 2 | 6.1 | UNDECIDED | 7.2 | OK |
  | ABS(ATAN(X))^2 to 2 | 9.2 | UNDECIDED | 14.3 | OK |
  | SQRT(X^2) to 1 | 2.9 | UNDECIDED | 4.3 | UNDEFINED |
  | K×ABS(X) to 1 | 2.5 | UNDECIDED | 4.0 | UNDECIDED |
  | ASIN(COS(X)) to 2 | 5.5 | UNDECIDED | 18.0 | UNDEFINED |
  | SIN nested 40, to 3 | 45.3 | NO_ROOM | 108.6 | OK |
  | ATAN nested 30, to 3 | 20.9 | NO_ROOM | 82.3 | OK |
  | LN(1+ nested 20, to 2 | 35.7 | NO_ROOM | 74.7 | OK |
  | SQRT(1+ nested 20, to 2 | 687.3 | NO_ROOM | 843.7 | NO_ROOM |
  | limit 1÷X^2-1÷SIN(X)^2 at 0 | 59.3 | OK | 57.1 | OK |
  | limit TAN(X)-1÷COS(X) at π÷2- | 279.5 | NO_ROOM | 138.6 | OK |
  | limit ((1÷X)-SIN((1÷X)))×(X) at -∞, DEG | 98.3 | NO_ROOM | 49.5 | OK |
  | limit SIN(K×(1÷X))×(2×X+1) at -∞ | 6.6 | UNDECIDED | 9.5 | OK |

  - A NO_ROOM today costs two runs: casim_run runs the request again in →DEC form when the exact run fails (CAS 019
    rule 5). Where the series route answers, that second run never happens, so most NO_ROOM rows answer faster than
    they used to fail. The host shows the same through casim_run: EXP(SIN(X)) to 12 is 0.218 ms OK against 0.261 ms
    NO_ROOM (casim_taylor alone, without the fallback: 0.215 against 0.125).
  - Where both routes refuse, the fallback runs the series route too: SQRT(1+ nested 20 is the slowest series row,
    843.7 ms (today 687.3), and the bench's second slowest case after factor's "f budget spent" (853.6 ms). (Its walk
    decides a leading coefficient's sign by the bound at each of its 20 levels; how the 843.7 ms splits wasn't
    measured.)
  - The answered rows take 4 to 139 ms. 1÷X^2-1÷SIN(X)^2 answers on today's walk at 12,288 B (its least arena is
    9,424 B; the build answers from 1,544 B).
- **The golden corpus:** the moves listed in tests/golden/moved/cas-021.txt, byte for byte the prototype's (44 lines,
  and 43 under →DEC), and 34 new lines from the bench's series rows, 26 of them the route's. Each of the route's 39
  distinct answers there (moved or new) was checked outside the route: 21 are the derivative route's text exactly given
  524,280 B; 6 match exact Fraction series
  term for term ((1÷(2+X))÷(2+X^2) to 10 in DEG, (EXP(X))÷(2+X^2) to 12, (TAN(X))×(SQRT(1+X)) to 11,
  (SQRT(1+X))÷(2+X^2) to 11 at both budgets, (ATAN(X))×(X×EXP(X)) to 12 in DEG over its 180÷π); the ABS and SQRT rows,
  the three nests and the four limits by hand or closed form. The route's two new UNDEFINED lines are ABS's corner:
  SQRT(X^2) is ABS(X), with no first derivative at 0, and ASIN(COS(X)) is π÷2-ABS(X) near 0 (its order 2 includes 1).
- **Tests:**
  - make check: series-golden.py (the corpus and →DEC with the route compiled out, byte for byte, with a control),
    taylor-values.py plain and --forced (300 of 300 exact at 8,800, 12,288 and 65,536 B; faae625: 242 at 8,800),
    limit-values.py plain and --forced (298 judged, 0 wrong; the route alone 252, 0 wrong, the rest UNDECIDED: the
    asymptotic families out of scope), abs-sides.py (854 values and 316 UNDEFINED at each arena, 0 wrong, 0 UNDECIDED;
    its control sees 138), doc-examples.py (645 rows, each again with the route compiled out: every row prints its
    answer both ways but the 10 tests/golden/moved/cas-021-examples.txt lists with their NO_ROOM, UNDECIDED or
    DECIMAL_FORM; added for the two mutants below).
  - make test: 20 tests, 118,420 checks. series_test (the route alone on every taylor and limit golden line, judged by
    the core): taylor 317 the same text and 17 equal in value, limit 382 and 2, 0 differ; and its 11 fixed rows.
- **Mutants:** 367 of 367 caught, the 19 ser-* new (the doc's list but the two left out above). The full run (4 shards,
  2.5 h) caught 363, left two ser-* unrun at shard 0's timeout, and didn't catch two old ones: CAS 016's
  limit-sides-share-arena and CAS 019's fallback-flags-no-decimal. Each makes today's route refuse a row it answered,
  016's limit (1-COS(2×X))^2÷X^4 and 019's SIN(X)×ASIN(X) at 8,600 B, and the series route answers that row with the
  same text, so only the route-off build shows them; their rows were checked only through the shipped build.
  doc-examples.py now runs every row with the route compiled out too, and the rerun of those four caught all four
  (the two old ones on their own rows). Peak summed RSS 876,856 KiB (the rerun 460,280), clang containers 58.91 MiB,
  tmpfs 0 (sampled through the rerun; the full run's, checked after it).

## Ruled by abacus (#9386)

1. ±∞: in. h021 has rows at both signs and in DEG.
2. TOO_LONG stays with today's route. The boundary is NO_ROOM and UNDECIDED only, and "refusals only" is the property
   h021 tests.
3. h021's ABS rows are gen_abs's (750, difference quotients at 150 digits), with the 72 named, plus abacus's new
   families. The one-sided judge goes in make check as casim's oracle.
4. The DEG and GRAD π-power rows stay NO_ROOM: agreed.
- Gaps filled: A in rule 1, B in rule 8, C in rule 3, D in rule 1, E and F under Cost, and their mutants in the
  oracle.
- h021: SymPy series as the oracle, written from this doc's rules only. Families: the debt list; taylor NO_ROOM at
  12,288, seeded; limit 0÷0 and ∞, seeded with exact values; ABS from gen_abs; holes; identity zeros; log and branch
  give-ups; poles; and a byte-for-byte sample of today's answered lines. It is frozen before the build, and casim
  gets the hash.

## Questions (the draft's, ruled above)

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
