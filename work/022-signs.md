# CAS work unit 022: signs decided by value

**From:** casim · **To:** abacus · **Status:** built (As built and Measured below), for abacus's run of h022 (frozen at
#9430: sha256 a32c0774…a31d, 722 rows). The draft was revised after abacus's review (#9424: question 1 strict, question 2
in order (a) then (b), question 3 traced and IN, #9425) and measured on throwaway prototypes before the build.
casim writes the unit; abacus writes h022 and the Accepted section.
- **Why now:** abacus's ruling #9388: after CAS 021, a small unit that puts 021's zero test (rule 8's carried error
  bound) into today's routes, so the test exists once. Its premise, my 021 doc's "const_sign, the test today's routes
  use", was wrong: a traced prototype found the identity-zero rows decided by other tests (#9408; 021's doc struck and
  corrected at 602c74f). The census below is the corrected scope (#9411–#9413).
- **The defect:** today's routes decide whether a letter-free constant is 0, or its sign, from its rounded decimal128
  value with no bound on that value's error, or from its form alone. An exact zero the canon doesn't know (an identity
  like SIN(1)^2+COS(1)^2-1) rounds to a small residue, and the routes take it for a number. These are wrong answers,
  not refusals. Only contrived inputs reach them, but they aren't only the scaled ones: abacus's row below uses the
  unscaled identity.
- **Budget:** decision 80's 262,144 B; 233,299 used at b282bbe (28,845 left for this unit and the ratio shift).

Z = 10^25×SIN(1)^2+10^25×COS(1)^2-10^25 (exactly 0; it rounds to 2E-9, to 2E-14 at 10^20, and to exactly 0 at 10^10
and 10^15) and Z1 = SIN(1)^2+COS(1)^2-1 (exactly 0).

## The census (each decider traced on the real path: its site and verdict logged per call)

| # | Family | Sites | Rows today (b282bbe) | Prototype |
|---|---|---|---|---|
| 1 | lm_sgn: limit's sign of a constant, an exact zero test on the rounded value | 18 (lm_lead_in, lm_sign_near, lm_quot, lm_dominant, lm_inf_minus_inf, lm_prod, lm_func, lm_walk) | (Z+X)÷X at 0: UNDEFINED, truth 1; (Z+X^2)÷X^2 at 0+: ∞, truth 1; (TAN(SIN(X))-SIN(TAN(X))+Z1×X^3)÷X^7 at 0, 12,288 B: ∞, truth 1÷30 (lm_quot sees Z1's leading coefficient as 1E-35, abacus #9411) | UNDECIDED each |
| 2 | taylor_hole's value test: a typed part with a value at A has no pole, and one "not real" has a hole | 1 | 1÷(Z+X) at 0 to 1: -1÷(10^25-…)-X÷(10^25-…)^2 (1÷Z is 5E+8), truth UNDEFINED; 1÷(EXP(-20000)+X) and EXP(20000)÷(1+X) at 0 to 1: UNDEFINED, truth an answer (the core's ABN_UNDERFLOW and ABN_OVERFLOW fall to cv_status's CV_NOTREAL, #9425) | UNDECIDED each |
| 3 | const_sign: nonzero past an absolute 1E-20 | 13 (fn's ABS and SGN folds, taylor_turns, taylor_corner, the sums' sm_keeps, sm_term ×2, sm_cmpabs, sm_groups ×4, sm_row) | ABS(Z+X) at 0 to 1: Z+X, truth UNDEFINED (with 4) | (with 4) |
| 4 | A form test: a letter-free constant not 0 by form is taken as nonzero | taylor_zero; taylor_hole's SGN and TAN arguments | ABS(Z+X) at 0 to 1 (SGN(Z) is no corner); TAN(Z+π÷2+X) at 0 to 1 (no pole) | UNDECIDED each |
| 5 | limit's two sides compared by form | 1 (do_limit) | Z×ABS(X)÷X at 0: UNDEFINED, truth 0 | UNDECIDED |

Out of the census, by ruling (#9413): with a letter, K×ABS(X)÷X at 0 → UNDEFINED is right for every K but 0, a generic
answer with an unshown condition. **Michael ruled the card (#9428):** such an answer is given with its condition shown
under it, as in CAS 018 rule 4b (K×ABS(X)÷X at 0 → UNDEFINED, with K≠0). That is its own unit after this one, with its
own hidden set; 022 stays letter-free, and the prototype leaves every row with a letter as it is.

The range rows (family 2) were found while probing and traced after the review (#9425). The vendored number layer
returns ABN_OVERFLOW and ABN_UNDERFLOW, separate statuses since the firmware's unit 026 took them out of ABN_RANGE.
casim's cv_status (CAS 009) maps only OK, DIV_ZERO and RANGE, and the rest fall to CV_NOTREAL, which taylor_hole reads
as a hole. limit on the same constants already refuses (UNDECIDED).

## Rules (proposed)

1. **One test** (strict, ruled #9424). Every sign or zero today's routes take from a letter-free constant goes through
   rule 8's bound (CAS 021's s_eval): it is 0 by form, or a number that is 0; otherwise nonzero only past 8 times its
   bound; otherwise undecided. A value that rounds to exactly 0 is a residue like any other: undecided. The sites are
   the census's families 1 to 5.
2. **A decision refuses; a fold keeps the symbol.** Where the answer depends on a sign the test can't decide, the
   operation is UNDECIDED. A fold only rewrites, and keeps its symbol when undecided, which is exact either way. The
   prototype's first step showed why the rule is needed: with const_sign on the bound alone, ABS(Z+X) became
   ABS(Z)+X×SGN(Z), still wrong, until taylor's corner test refused. The sites, each named:
   - folds: fn's ABS(c) (to c or -c) and SGN(c) (to 1 or -1) keep ABS(c) and SGN(c);
   - taylor: taylor_hole (a pole, a hole, TAN's and SGN's arguments), taylor_zero, taylor_turns, taylor_corner refuse;
   - limit: lm_sgn's 18 sites (an order, a leading term, a quotient's sign, ∞-∞, a function's limit) and the sides
     (rule 3) refuse;
   - the sums (CAS 018): sm_keeps and sm_term already take an undecided base as "not a positive constant" and fall to
     their routes' own refusals; sm_groups (a leading coefficient, B's sign) and B-1 (rule 4b's "decidably nonzero")
     refuse. ~~sm_cmpabs and sm_row read const_sign's 0 as "equal" today, so const_sign gets a third answer~~ (corrected
     at the build, by reading them: sm_cmpabs already returns 2 for equal by form and 0 for undecided, and sm_row only
     orders the condition rows for printing), so const_sign keeps its two answers, a decided sign or 0 for not decided,
     and every caller keeps its reading. No golden row reaches the sums' sites with a residue; h022 will, and so does
     work/022-examples.txt's K×(1+10^25×Z1)^K (B-1 undecided: NO_CLOSED_FORM, where b282bbe went on to a closed form for
     B ≠ 1 and ran out at TOO_LONG).
   - a range status (ABN_OVERFLOW, ABN_UNDERFLOW) is no value computed (CV_NO), not "not real"; at a decision on a
     letter-free part, no value refuses (taylor_hole), as measured in #9425 (variant B).
3. **The sides (family 5).** Two finite one-sided limits whose forms differ: if both are letter-free and their
   difference is 0 by form or as numbers, they are one value (the limit is either form); if its sign is undecided,
   UNDECIDED; if
   decided nonzero, no limit, as today. Sides with a letter compare as today.
4. **Depth.** At today's sites the bound's walk is bounded by enter()'s CASIM_DEPTH_MAX (44) alone, not by the series
   route's S_EVAL_DEPTH 8. The corpus's 21-deep LN(2+…) and SQRT(1+…) nests at 0 are constants 42 tree levels deep, and
   at 8 the bound refuses them (6 corpus lines OK→UNDECIDED, measured). On the device (below) 44 fits the stack and
   the gate doesn't move, but three rows run past the bench's 2,000 ms interrupt mask. Ruled (#9424), in order: (a) the
   bound along const_pow's own paths (no LN for whole and half powers) with s_b's 10^-32 computed once, re-measured
   against the shipping mask; (b) if any row is still over, depth 8 at taylor_hole alone and 44 at every other site,
   with taylor_hole's moves listed (the 6 corpus lines, 4 →DEC, 6 complex). The mask is the instrument and stays.
   **Measured at the shipping mask (#9433):** (a) panicked the chip's interrupt watchdog; (b) gave DEVICE PASS, the deep
   nests refusing in 17–27 ms and the census rows in 18–239 ms. So the build has two caps: the bound at taylor_hole
   walks 8 levels (S_EVAL_DEPTH, as the series route's), and at every other site as deep as enter() allows.
5. **The bound walks a constant once.** CAS 021's s_eval runs const_val on its whole subtree at every node, quadratic
   in the depth. That was fine under the series route's cap of 8, but at depth 44 it isn't: taylor_hole sizes every
   typed part, so a 21-deep nest becomes cubic. The first device run of the prototype never reported (below), and on
   the host callgrind put 92% of the deep row's cost in s_eval and 88% in const_val under it. s_eval should build each
   value from its parts' values, the way const_val forms it (the same sums and products in the same order, const_pow,
   and const_val's function step moved into a helper both call), so the values are the same values and a walk is
   linear. Measured: the same golden, the same series_test counts as the shipped build, and the deep row 22.5 M
   instructions against 101.3 M (b282bbe's walk without the bound: 8.4 M). The golden alone is weak evidence that the
   values are the same values, since most constants never print: a test build asserts s_eval's v equals const_val's
   bit for bit on every constant it values, across the corpus, →DEC, complex and the doc examples (#9424).
6. **CAS 021's route** runs where it runs today: after NO_ROOM or UNDECIDED in taylor's derivative loop or limit's
   walk. Rule 7's refusal on the typed input stays final (do_taylor returns before the route), so the route never sees
   input rule 7 couldn't value.

## Measured (throwaway prototypes from b38aa7c's tree, not pushed: p2 = the five families; p3 = p2 with question 1's
strict zero and rule 5's linear walk)

- **Golden boundary, all five families on the bound, depth by enter() (p2 and p3 alike):** corpus 0 moved of 10,822, →DEC 0 of 10,788,
  complex 0, doc examples 0 of 1,300; make check green (frame table re-recorded). With depth 8: 6 corpus lines (4 OK→
  UNDECIDED, 2 NO_ROOM→UNDECIDED), 4 →DEC, 6 complex, every one a 20–21-deep nest through taylor_hole; with taylor_hole's
  test off and the rest on, 0.
- **The census rows:** every row in the table refuses (UNDECIDED), at 65,536 and 12,288 B. Unchanged: K×ABS(X)÷X
  (UNDEFINED), ABS(X)÷X (UNDEFINED), ABS(X+1) at 0 (1+X), TAN(X) at 1.
- **S3 text:** 234,205 B against b282bbe's 233,299: +906 (the prototype, without the mutants' rows or tests).
- **S3 frames:** const_sign 80→64, lm_sgn 48→64, taylor_hole 64→96; s_eval 176, const_val 64, do_taylor 336,
  do_limit 240 unchanged. Host: const_sign 96→80, lm_sgn 64→112, taylor_hole 128→176, do_limit 320→336.
- **Device (XIAO S3, host and chip at 12,288 B):** the whole bench (1,333 cases) on p2 never reported within the
  capture's 1,500 s. The chip wasn't hung: a raw read found it still computing, the PC in decimal128's log (b282bbe's
  bench computes about 6 minutes in all). The host profile above is why. A 15-case subset (the deep rows, the census
  rows, the gate's worst) panicked p2 on the interrupt watchdog: the bench reads each stack with interrupts masked
  around the run, and device/sdkconfig.defaults allows 2,000 ms masked. The base passed the same subset.
  **p3, the linear walk, the whole bench (1,333 cases), with the interrupt watchdog raised to 10 s for this run only:
  DEVICE PASS, 0 different from the host; the gate unchanged,** worst 11,360 masked (STU chain X^ 44 d) + 384 for
  interrupts = 11,744 of 12,288. (The interrupt excess, I0030's, is 336 B here against 272 at b282bbe, inside the
  384.) Stack and time on the rows it moves (b282bbe → p3, the median of 15):

  | Row | stack B | ms |
  |---|---|---|
  | LN(2+ nested 21 at 0, order 0 | 5,184 → 9,328 | 564.6 → 1,928.1 |
  | SQRT(1+ nested 21 at 0, order 0 | 5,376 → 9,200 | 383.6 → 2,178.3 |
  | LN(2+ nested 21 at 0, order 2 | 5,184 → 9,328 | 1,144.4 → 3,871.9 |
  | SQRT(1+ nested 20 at 0, order 2 (CAS 021's row) | 5,168 → 8,816 | 843.7 → 4,033.0 |
  | limit (Z+X)÷X at 0 | 2,176 → 2,464 | 42.4 → 217.3 |
  | limit (Z+X^2)÷X^2 at 0+ | 2,176 → 2,464 | 23.2 → 110.8 |
  | limit Z×ABS(X)÷X at 0 | 2,240 → 2,800 | 29.1 → 51.1 |
  | limit (TAN(SIN(X))-SIN(TAN(X))+Z1×X^3)÷X^7 at 0 | 2,240 → 2,560 | 93.8 → 288.7 |
  | taylor 1÷(Z+X) to 1 | 2,096 → 2,752 | 42.4 → 31.6 |
  | taylor ABS(Z+X) to 1 | 2,096 → 2,544 | 33.8 → 147.8 |
  | taylor TAN(Z+π÷2+X) to 1 | 2,144 → 2,624 | 34.2 → 27.9 |

  So the stack fits: the deep path's 9,328 is under the gate's worst, and the gate doesn't move. The time doesn't fit:
  three rows run past 2 s, and CAS 021's own SQRT(1+ row goes from 0.84 to 4.03 s, so the shipping bench would panic on
  them. On the calculator that is a 4-second wait on a contrived input; on the bench it is a failure.
- **Host cost of the bound, the whole corpus:** golden_check 0.21 s at b282bbe, 0.29 s with the bound (p2 and p3).
  The deep LN(2+…) nest at 0, order 0: 8.4 M instructions at b282bbe, 101.3 M with p2's quadratic walk, 22.5 M with
  the linear one (p3).

## Cost (estimate)

The prototype's +906 B, plus the sites' refusal paths written properly and the sums' sites (sm_*), which the census
traced by reading only (no golden row reaches them with a residue): 1.5–2.5 KB, inside 28,845.

## The oracle

- **The boundary:** every golden move listed (the prototype: none), each from a wrong answer or to a refusal.
- **The masked-fault check** (abacus runs it, #9424): casim-ser0 at the build against casim-ser0 at b282bbe on every
  set; a line moves only away from a wrong answer or into a refusal. The CASIM_SER=0 target stays.
- **The values are the same values:** the test build of rule 5 (s_eval's v against const_val's, bit for bit, every
  constant either values) over the corpus, →DEC, complex and the doc examples.
- **h022 (abacus's):** the census rows for each family, across the scale sweep (Z1 unscaled, 10^10, 10^15, 10^20,
  10^25, 10^30, 10^40); the letter-free Z×ABS(X)÷X (#9413); identity zeros under deep arguments (the bound at depth 44
  is untested ground: h021 tested it only to 8); the sums' sites; the range rows as TODAY rows (today's line, a refusal
  or the truth passes); controls that must keep answering (true nonzero constants near the residue's size: 10^-9,
  SIN(1)^2+COS(1)^2-1+10^-20; deep constants that aren't 0).
- **Mutants, at least:** each family's site back on its old test; an undecided sign taken as decided (each of rule 2's
  decisions); const_sign's undecided read as 0 again (sm_cmpabs, sm_row); the sides' difference not tested; letters let
  into rule 3; the depth back at 8 where rule 4 keeps 44 (it must move the 6 lines, under (a)); rule 7's refusal handed
  to the route; the strict zero reverted (it must move the 10^10 and 10^15 rows); s_eval's value drifting from
  const_val's (rule 5's check must catch it); a range status read as "not real" again (the range rows).
- **The device bench:** DEVICE PASS with the gate unchanged and the shipping 2,000 ms interrupt mask, with its rows
  for the deepest path (the 21-deep nests at 0, a deep constant under limit's walk) and the census rows.

## Ruled (#9424, #9425)

1. **Strict zero:** yes (rule 1). 0 golden moves measured; nothing relies on the luck.
2. **Depth and time:** (a) the bound along const_pow's paths, re-measured against the shipping 2,000 ms mask; (b) if
   still over, depth 8 at taylor_hole alone, its moves listed (rule 4). The mask isn't raised: 0.84 → 4.03 s on 021's
   accepted row is a regression for a defect only contrived inputs reach. (a)'s measurement is next.
3. **The range rows:** traced into family 2, so IN (rule 2's last item, variant B); h022 carries them as TODAY rows.

## As built (where the code differs from the rules, or settles what they left open)

- **const_sign keeps its two answers**, a decided sign or 0 for "not decided" (rule 2's sums correction above), and is
  rule 8's test: a number decides itself, any other constant only past 8 times its bound. Its 13 callers read it as
  before.
- **lm_sgn** is strict (rule 1): 0 only by form or as a number; a rounded 0 is "no sign here" (2).
- **taylor_hole** refuses (UNDECIDED) three ways: SGN's or TAN's argument not 0 by form and not decided; a letter-free
  part with no value computed (CV_NO: past the range); a part with a value not past its bound, sized 8 levels deep
  (rule 4). **taylor_zero** refuses on a letter-free constant not 0 by form and not decided.
- **The sides** (do_limit): letter-free sides whose forms differ are one value when their difference is 0 by form or as
  numbers, undecided (UNDECIDED) when its sign is, and no limit when it's decided; with a letter, no limit as before.
- **cv_status** maps ABN_OVERFLOW and ABN_UNDERFLOW to CV_NO.
- **Rule 8's walk** forms each value from its parts (const_val's sum or product in its order, const_pow, and cv_fn,
  const_val's function step now shared by both), and its power bound follows const_pow's path: whole and half powers
  by squaring (2 bits(p) + 6 roundings), any other by exp(e ln a) as before. The walk is linear; its values are
  const_val's, bit for bit (CASIM_VCHECK, below).
- **10^-32 isn't kept in ctx.** The prototype held it there to compute it once, and the gate grew by those 16 B on every
  operation's stack (11,360 → 11,376). Under rule 4 (b) the deep rows refuse fast without it, so s_b computes it per use
  as at b282bbe.
- **any_letter** asks "a letter anywhere?" in one walk, where the sites asked contains() 26 times.
- **One mutant is left out as unreachable**, run and surviving: taylor_zero's undecided test removed. taylor_hole's SGN
  test refuses first on every path to it, and the corner path's const_sign refuses after it. A search of 60 taylor rows
  (20 ABS shapes of Z, orders 1 to 3) found no row where the two differ. The guard stays.
- **Two of CAS 021's mutants needed new rows**, because the core now refuses first on the rows that caught them.
  ser-divisor-undecided (the series route dividing by a leading coefficient whose sign isn't decided): its row,
  1÷(Z×10^20+X), refuses with or without the test. taylor X÷(Z+X) at 0 to 1 catches it: b282bbe printed X÷Z there (Z
  is exactly 0; the truth is 1), the build's core refuses, the route then runs, and its divisor test is what keeps it
  from printing the same (work/022-examples.txt, and a series_test row for the route alone). ser-zero-plain-value's text,
  s_sign as const_sign, became a change of depth alone once const_sign was rule 8, and corpus 10883, its catch, now
  refuses either way; it's rewritten to put CAS 009's 1E-20 test, its meaning at 021, in s_sign.

## Measured (the build, over b282bbe's library)

- **S3 text: 234,474 B, +1,175 over 233,299** (the draft estimated 1.5–2.5 KB), from the per-function sections:

  | Piece | B |
  |---|---|
  | const_val's function step moved into cv_fn, shared with the walk | -1,028 + 777 = -251 |
  | const_pow out of line (the walk calls it too) | +434 |
  | s_eval (each value from its parts, the power bound by const_pow's path) | +300 |
  | any_letter | +246 |
  | do_limit (the sides, rule 3) | +168 |
  | taylor_hole and taylor_zero (the refusals) | +87, +52 |
  | switch tables, const_sign, lm_sgn, and functions the compiler moved | +139 |

  That leaves 27,670 of decision 80's 262,144 for the ratio shift, the condition rows' unit and v1.0's margin.
- **S3 frames:** const_sign 80→64, lm_sgn 48→64, taylor_hole 64→80, s_eval 176→192; new any_letter 32 (a level),
  cv_fn 64; casim_run 64, casim_step 240, do_taylor 336, do_limit 240 and const_val 64 unchanged. Host (the frame table,
  re-recorded): const_sign 96→80, lm_sgn 64→112, s_eval 272→288, taylor_hole 128→144, do_limit 320→336.
- **The device (XIAO S3, host and chip at 12,288 B, the shipping 2,000 ms interrupt mask):** DEVICE PASS, 1,334 cases,
  0 different. The gate is unchanged: worst 11,360 masked (STU chain X^ 44 d) + 384 = 11,744 of 12,288 (the interrupt
  excess, I0030's, 336 B, inside the 384). Rows (b282bbe → the build; stack B, median ms of 15):

  | Row | b282bbe | build |
  |---|---|---|
  | taylor LN(2+ nested 21 at 0, order 0 | OK, 5,184, 564.6 | UNDECIDED, 4,384, 26.6 |
  | taylor SQRT(1+ nested 21 at 0, order 0 | OK, 5,376, 383.6 | UNDECIDED, 4,384, 18.2 |
  | taylor LN(2+ nested 21 at 0, order 2 | NO_ROOM, 5,184, 1,144.4 | UNDECIDED, 4,384, 26.5 |
  | taylor SQRT(1+ nested 20 at 0, order 2 (CAS 021's row) | NO_ROOM, 5,168, 843.7 | UNDECIDED, 4,240, 17.4 |
  | limit (Z+X)÷X at 0 | UNDEFINED, 2,176, 42.4 | UNDECIDED, 2,464, 147.4 |
  | limit (Z+X^2)÷X^2 at 0+ | ∞, 2,176, 23.2 | UNDECIDED, 2,464, 75.9 |
  | limit Z×ABS(X)÷X at 0 | UNDEFINED, 2,240, 29.1 | UNDECIDED, 2,784, 34.1 |
  | limit (TAN(SIN(X))-SIN(TAN(X))+Z1×X^3)÷X^7 at 0 | ∞, 2,240, 93.8 | UNDECIDED, 2,496, 248.0 |
  | taylor 1÷(Z+X) to 1 | an answer, 2,096, 42.4 | UNDECIDED, 2,736, 22.0 |
  | taylor ABS(Z+X) to 1 | an answer, 2,096, 33.8 | UNDECIDED, 2,544, 95.9 |
  | taylor TAN(Z+π÷2+X) to 1 | an answer, 2,144, 34.2 | UNDECIDED, 2,608, 18.9 |

  (b282bbe's column is the base's run of the same rows, 15:14Z, under the same mask.) Every row is under 0.25 s; the deep nests refuse in 30 ms where
  they took up to 1.1 s.
- **The golden:** 6 corpus lines and 4 under →DEC move, all to UNDECIDED (tests/golden/moved/cas-022.txt; with their
  befores put back both files hash to b282bbe's, scripts/signs-golden.py, with a control). Complex 0 different. CAS 021's
  route-off boundary (series-golden.py) holds with those lines at their after. The doc examples run 675 rows both ways
  (1,360 checks), and again through the value check.
- **The values are the same values** (rule 5, CASIM_VCHECK): over the corpus (10,822), →DEC (10,788), complex (10,299
  and 523 solve lines) and the doc examples, 0 lines fail. The control, a sum's value off by a relative 10^-33, fails
  110 corpus lines on the check build while the plain golden passes all 10,822.
- **Tests:** make check green (23 runs, adding signs-golden.py and the four value-check runs); make test green, 20
  tests, 118,421 checks, clang clean.
- **Mutants: 379 of 379 caught.** 12 are new (sign-: lmsgn-exact, strict-reverted, const-1e20, hole-value,
  hole-depth-deep, hole-form, range-notreal, range-decides, sides-form, sides-letters, value-drift, fold-abs-undecided);
  sign-zero-form is left out (As built). The full run, 4 shards from 17:13Z to 19:56Z, caught 377, the summed RSS
  peaking at 1,074,692 KiB with nothing written to /tmp or /dev/shm; CAS 021's ser-divisor-undecided and
  ser-zero-plain-value survived (As built). With their rows, both are caught (20:03–20:06Z, peak 276,464 KiB), and the
  new series_test row alone fails on ser-divisor-undecided.

## After abacus's run (#9474)

abacus ran h022 and the older sets on db19065 (#9468) and asked for two traces, one move and one row. The Measured
section above stands as it was at db19065.

- **A move this doc missed (abacus's find):** rule 1 decides the draft's own control, c = SIN(1)^2+COS(1)^2-1+10^-20.
  At const_sign its value is 1E-20 against a bound of 5.1E-31, so it is positive. b282bbe's 1E-20 test refused it, since
  |v| was not past 1E-20. taylor ABS(c+X) to 1 and ABS(c+X^2) to 2: b282bbe printed ABS(c)+X×SGN(c) (and X^2×SGN(c));
  the build prints c+X (c+X^2). Both are right, and the build's is simpler. abacus measured the same constant gaining
  answers on h022's V0651–V0656 (TOO_LONG, NO_CLOSED_FORM or UNDECIDED became an answer), all judged right. A control row
  is in work/022-examples.txt.
- **Strict's row where reverting it is wrong:** the rows that catch sign-strict-reverted (limit (c+X)÷X at 0 for Z1 and
  the 10^10 and 10^15 identities, and the sides row) pin UNDECIDED, where the reverted build's answer is in fact right,
  because those constants are exactly 0. New row: limit ((SIN(1)^2+COS(1)^2-1+10^-40)+X)÷X at 0. The constant is
  10^-40, and its value rounds to exactly 0. The truth is no limit (UNDEFINED); b282bbe prints 1, the build UNDECIDED,
  strict reverted 1. abacus's suggested (EXP(1)+10^-40)-EXP(1) cancels by form to ~~the number 10^-40, which decides
  itself~~ 10^-40, whose value decides it (lm_sgn sees 1E-40 against a bound of 9.8E-71; corrected the same hour: it isn't
  a number there, as lm_sgn reached its walk) (UNDEFINED in all three builds), so the row ~~adds 10^-40 to an identity
  whose value rounds to exactly 1 instead~~ puts 10^-40 into Z1's identity instead, where the rounding loses it: lm_sgn
  sees the value exactly 0, against a bound of 5.1E-31 (measured; "rounds to exactly 1" was not).
- **Why the sums with c's terms at 10^30 and past refuse NO_ROOM, not UNDECIDED** (abacus's Z30, Z40, Z25×10^20,
  Z25×10^40; (1+c)^K to N and to ∞, (1-c)^K to ∞): it isn't rule 8's walk running out. Traced under gdb on a debug
  build of db19065, all 12 forms end in partfrac's NO_ROOM, passed through by do_sum_. The path, followed step by step
  for Z30: sums rule 3's sm_term asks const_sign for the base B = 1+c. Its value is 1.0002 against a bound of 2.05 (the
  10^30-scale terms leave about four digits), so the sign isn't decided (ABS(1+c) keeps its ABS, for Z40 too), and
  sm_term's guard for a constant base of undecided sign (the 0^(K+1) case) refuses the shape. sm_split then files the
  term with the rational ones, and rule 5 hands (B^K)÷(1) to partfrac. Reading it as a polynomial (pw_power), partfrac
  raises the rational 10 to the 30th, which overflows 64 bits: NO_ROOM, CAS 012's past-2^62 rule. partfrac does the same
  on that text at b282bbe. Making it UNDECIDED would mean sm_term refusing a letter-free base of undecided sign itself,
  rather than handing the term on; that is a change in the sums and is left for abacus to rule.

## Out of scope

- **(abacus #9474, traced; the follow-up list)** A finite sum's terms are summed as written: sum 1÷c^K, K = 1 to 3,
  with c exactly 0, prints the three terms, which divide by c (b282bbe the same). The path is do_sum_, sm_read (the
  bounds) and sm_direct (K's values put in). Under gdb no census site, no const_val, no s_eval and no decimal arithmetic
  at all runs in that command, so no sign or zero is decided there. It's the same as simplify printing 1÷c: an expression
  rewritten, never asked whether it is defined.

The letter rows (ruled #9428: their own unit, after this one). The series route's own decisions (CAS 021): its bound's walk becomes linear under
rule 5, with the same values, so they don't change. The two-letter-ratio shift.
