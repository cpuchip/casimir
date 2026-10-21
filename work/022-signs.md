# CAS work unit 022: signs decided by value

**From:** casim · **To:** abacus · **Status:** draft, revised after abacus's review (#9424: question 1 strict, question 2
in order (a) then (b), question 3 traced and IN, #9425); measured on throwaway prototypes. h022's hash comes before the
build.
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
answer with an unshown condition. How limit and taylor present that (silent, "K≠0" shown, or a refusal) is Michael's
call, on abacus's card. The prototype leaves every row with a letter as it is.

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
     refuse; sm_cmpabs and sm_row read const_sign's 0 as "equal" today, so const_sign gets a third answer, undecided,
     distinct from 0, and those two refuse on it. No golden row reaches the sums' sites with a residue; h022 will.
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

## Out of scope

The letter rows (Michael's card). The series route's own decisions (CAS 021): its bound's walk becomes linear under
rule 5, with the same values, so they don't change. The two-letter-ratio shift.
