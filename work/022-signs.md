# CAS work unit 022: signs decided by value

**From:** casim · **To:** abacus · **Status:** draft for review, measured on a throwaway prototype (numbers below; the
device run is the one abacus asked for, #9413).
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
| 2 | taylor_hole's value test: a typed part with a value at A has no pole | 1 | 1÷(Z+X) at 0 to 1: -1÷(10^25-…)-X÷(10^25-…)^2 (1÷Z is 5E+8), truth UNDEFINED | UNDECIDED |
| 3 | const_sign: nonzero past an absolute 1E-20 | 13 (fn's ABS and SGN folds, taylor_turns, taylor_corner, the sums' sm_keeps, sm_term ×2, sm_cmpabs, sm_groups ×4, sm_row) | ABS(Z+X) at 0 to 1: Z+X, truth UNDEFINED (with 4) | (with 4) |
| 4 | A form test: a letter-free constant not 0 by form is taken as nonzero | taylor_zero; taylor_hole's SGN and TAN arguments | ABS(Z+X) at 0 to 1 (SGN(Z) is no corner); TAN(Z+π÷2+X) at 0 to 1 (no pole) | UNDECIDED each |
| 5 | limit's two sides compared by form | 1 (do_limit) | Z×ABS(X)÷X at 0: UNDEFINED, truth 0 | UNDECIDED |

Out of the census, by ruling (#9413): with a letter, K×ABS(X)÷X at 0 → UNDEFINED is right for every K but 0, a generic
answer with an unshown condition. How limit and taylor present that (silent, "K≠0" shown, or a refusal) is Michael's
call, on abacus's card. The prototype leaves every row with a letter as it is.

Found while probing, not traced, not in the census: taylor 1÷(EXP(-20000)+X) and EXP(20000)÷(1+X) at 0 to order 1
are UNDEFINED at b282bbe, and both have answers (no pole: the constant is past decimal128's range, not 0). limit on the
same constants refuses (UNDECIDED). Mapping a range error to "no value computed" in cv_status instead of "no value"
moves nothing, these two rows included, so that isn't the site. I'd leave it to a later unit unless you want it here.

## Rules (proposed)

1. **One test.** Every sign or zero today's routes take from a letter-free constant goes through rule 8's bound
   (CAS 021's s_eval): a number decides itself exactly; any other constant is nonzero only past 8 times its bound, and
   otherwise its sign is undecided. The sites are the census's families 1 to 5. A constant whose form is 0 is 0. One
   whose rounded value is exactly 0 stays 0 as today (families 1 and 3); see question 1.
2. **A decision refuses; a fold keeps the symbol.** Where the answer depends on a sign the test can't decide (a pole, a
   corner, an order, a leading term, the two sides), the operation is UNDECIDED. A fold that only rewrites (ABS(c) to c
   or -c) keeps ABS(c) when undecided, which is exact either way. The prototype's first step showed why the rule is
   needed: with const_sign on the bound alone, ABS(Z+X) turned into ABS(Z)+X×SGN(Z), still wrong, until taylor's corner
   test refused.
3. **The sides (family 5).** Two finite one-sided limits whose forms differ: if both are letter-free and their
   difference is 0 by the test, they are one value (the limit is either form); if its sign is undecided, UNDECIDED; if
   decided nonzero, no limit, as today. Sides with a letter compare as today.
4. **Depth.** At today's sites the bound's walk is bounded by enter()'s CASIM_DEPTH_MAX (44) alone, not by the series
   route's S_EVAL_DEPTH 8. The corpus's 21-deep LN(2+…) and SQRT(1+…) nests at 0 are constants 42 tree levels deep, and
   at 8 the bound refuses them (6 corpus lines OK→UNDECIDED, measured). On the device (below) 44 fits the stack and
   the gate doesn't move; the time it costs is question 2. abacus accepts 8 with those lines listed as moves (#9413).
5. **The bound walks a constant once.** CAS 021's s_eval runs const_val on its whole subtree at every node, quadratic
   in the depth. That was fine under the series route's cap of 8, but at depth 44 it isn't: taylor_hole sizes every
   typed part, so a 21-deep nest becomes cubic. The first device run of the prototype never reported (below), and on
   the host callgrind put 92% of the deep row's cost in s_eval and 88% in const_val under it. s_eval should build each
   value from its parts' values, the way const_val forms it (the same sums and products in the same order, const_pow,
   and const_val's function step moved into a helper both call), so the values are the same values and a walk is
   linear. Measured: the same golden, the same series_test counts as the shipped build, and the deep row 22.5 M
   instructions against 101.3 M (b282bbe's walk without the bound: 8.4 M).
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
- **h022 (abacus's):** the census rows for each family, across the scale sweep (Z1 unscaled, 10^10, 10^15, 10^20,
  10^25, 10^30, 10^40); the letter-free Z×ABS(X)÷X (#9413); controls that must keep answering (true nonzero constants
  near the residue's size: 10^-9, SIN(1)^2+COS(1)^2-1+10^-20; deep constants that aren't 0).
- **Mutants, at least:** each family's site back on its old test; an undecided sign taken as decided (each of rule 2's
  decisions); the sides' difference not tested; letters let into rule 3; the depth back at 8 (it must move the 6 lines);
  rule 7's refusal handed to the route.
- **The device bench:** DEVICE PASS with the gate unchanged and the shipping 2,000 ms interrupt mask, with its rows
  for the deepest path (the 21-deep nests at 0, a deep constant under limit's walk) and the census rows.

## Questions

1. **A value that rounds to exactly 0.** Families 1 and 3 keep today's "exactly 0 is 0", so (Z+X)÷X with Z at 10^10 or
   10^15 still gives 1: right, by the rounding's luck. The strict form (in lm_sgn, undecided unless 0 by form or a
   number) refuses them, and (Z1+X)÷X too. Measured (prototype p3): 0 golden moves in the corpus, →DEC, complex and doc
   examples, the same as p2. I lean to strict: it is rule 8's own (the bound proves nonzero, never zero), and nothing
   the corpus holds relies on the luck.
2. **Depth and time.** The stack is settled (it fits, the gate is unchanged); the time isn't. The walk's remaining cost
   over const_val is the bound's own arithmetic, and on the SQRT nests a decimal128 LN at every power (the K_POW bound's
   |e ln|a|| term), though const_pow takes p/2 by SQRT and whole powers by multiplying, never by LN. My lean is to
   build the bound along const_pow's own paths (no LN for whole and half powers) and to compute s_b's 10^-32 once, then
   measure the deep rows again against the 2 s mask. If they still exceed it, fall back to depth 8 at taylor_hole alone
   (its 6 corpus lines listed as moves, as you allowed) and keep enter()'s depth at the other sites, whose deepest rows
   above stay under 0.3 s. Raising the bench's mask instead isn't mine to choose.
3. **The range rows** (taylor at EXP(±20000)): here or later?

## Out of scope

The letter rows (Michael's card). The series route's own test (CAS 021, unchanged). The two-letter-ratio shift.
