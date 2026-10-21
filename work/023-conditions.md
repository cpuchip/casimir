# CAS work unit 023: conditions shown under limit and taylor answers

**From:** casim · **To:** abacus · **Status:** draft, for abacus's review; measured on a throwaway prototype (p1, over
fc46c8b, not pushed). h023's hash comes before the build.
casim writes the unit; abacus writes h023 and the Accepted section.
- **Why now:** Michael's ruling on the letter card, through abacus (#9428): when a limit or taylor answer holds only for
  K≠0, "the answer is given and the condition is shown under it, as in CAS 018 rule 4b", with the example K×ABS(X)÷X at
  0 → UNDEFINED, with K≠0. 022 stayed letter-free, and abacus asked for this unit's draft once 022 was in (#9480).
- **The question:** where do limit and taylor go on past a letter they can't decide, and which condition does each
  answer need, written the way CAS 018 rule 4b writes the sums'?

## The census (sites found by reading; each example's site confirmed under gdb on the prototype)

022's census found the places where a letter-free sign or zero was decided. The letter cases sit at the same places:
where 022 asks "a letter here?" and goes on, today's answer assumes something about the letter. A probe of 54 letter
rows (31 limits, 23 taylors, ~/scratch/casim/c23/probe.py) judges each answer numerically at K = 0, ±1, ±2, 1÷2, ±π÷2,
π and 3. On fc46c8b, 29 of the 54 are wrong, or have no value, at some K they don't exclude; the other 25 hold for every
K or refuse.

**Limit (the walk):**
1. **lm_quot, a finite ÷ a letter's finite.** lm_sgn says 2 for a letter, and lm_quot divides: X÷(K×X+X^2) at 0, after
   one L'Hôpital step, is 1÷(K+2X) → 1÷K. It needs the denominator's value ≠ 0.
2. **L'Hôpital's quotient.** n'÷d' is put in canonical form, and a letter factor of d' cancels: SIN(K×X)÷(K×X) → 1,
   (X-SIN(X))÷SIN(K×X) → 0 (sites 1 and 2 both record there). It needs each letter factor of d' ≠ 0.
3. **The leading terms at ∞ (not reached).** kn÷kd for a letter kd would need kd ≠ 0, but the walk refuses first: a
   letter times a term going to ∞ has no sign (by reading, lm_prod's UNDECIDED; the probe's ∞ rows with a letter
   leading coefficient are UNDECIDED), as letter-free residues are there (022's follow-up).
   No probe row reached it, so the prototype's row there never fires; h023 can look for a path.
4. **The sides (022 rule 3).** With a letter, sides whose forms differ are "no limit": K×ABS(X)÷X → UNDEFINED. It needs
   their difference ≠ 0 (2K: K≠0).
5. **The value at the point (lm_at, continuity), at every level of the walk.** (K+X)÷(K-X) at 0, and at 0+ or 0-, is
   valued as K÷K, which canon makes 1 (it is -1 at K = 0). It needs each divisor's value there ≠ 0. The prototype first
   recorded this only at do_limit's top and missed the one-sided rows; it is recorded in lm_at's caller in the walk now.
6. **The answer's own denominators.** canon factors a letter constant out of the typed input (SIN(X)÷(K×X) is
   (1÷K)×SIN(X)÷X, SQRT(X^2+1)÷(K×X) at ∞ likewise) and the walk carries it: the answer 1÷K needs K≠0.

**Taylor (rule 7's letter cases).** Rule 7 values every part of the typed f at the point. Where a part's value holds a
letter, today it is skipped, and the answer assumes it:
7. **a divisor or INV:** 1÷(K+X) → 1÷K-X÷K^2 needs K≠0;
8. **ABS's and SGN's argument (the corner):** ABS(K+X) → ABS(K)+X×SGN(K) needs K≠0;
9. **TAN's pole:** TAN(K+X) → TAN(K)+X÷COS(K)^2 needs COS(K)≠0;
10. **LN, LOG, SQRT, a fractional power:** LN(K+X) → LN(K)+X÷K needs K>0 (a domain, not only K≠0; question 2).

Not sites: the refusals stay refusals (limit (K+X)÷X at 0 is UNDECIDED: ∞ or -∞ by K's sign, and a sign isn't a
condition this unit states, as CAS 018 rule 4b's item 7 says for Σ A×K). The series route (021) runs only after a
refusal, and its divisor test refuses a letter leading coefficient (s_sign of a letter is 0); no probe row reached an
answer from it.

## Rules (proposed)

1. **The shape is CAS 018 rule 4b's.** The answer, then each condition as a row after "; ": `1÷K-X÷K^2; K≠0`. The
   status stays OK. TOO_LONG when the answer and its rows pass 191 bytes. Glyphs: ≠ (U+2260), and > and < (rule 4's
   domains), as 018 already prints them.
2. **Where rows come from.** Limit: the walk's sites 1, 2, 4 and 5 record what they took as not 0 (letter-holding,
   var-free) in an append-only list in lwalk; lm_combine's give-back drops an abandoned attempt's rows with its arena;
   after the walk, the answer's own denominators (6) are added. Taylor: after the answer, one walk of the typed f at the
   point (the tree rule 7 reads, kept below the loop's base) records 7–10. Both only when the input holds a letter
   besides the variable.
3. **A row, written plainly** (018 rule 4b's way, generalized from B≠1 to e≠0). A product is 0 where a factor is: each
   letter factor gets its own rows, and constant factors (π, numbers) none (K×π≠0 is K≠0). A whole power's rows are its
   base's ((K-1)^2: K≠1). k×L^m + c of one letter is solved for L: K≠1, and K≠-1; K≠1 for K^2-1; none when it is never 0
   (K^2+1). Anything else is e≠0 as canon prints it (COS(K)≠0). Rows are listed by letter, then by value, each once.
4. **Domains (question 2).** LN's, LOG's and SQRT's argument, and a fractional power's base, must be > 0: a×L+b solved
   (K>0, K<1), else e>0. A > row says the ≠ row too, which is then dropped (LN(K+X): K>0, not K>0; K≠0).
5. **UNDEFINED with rows (question 1).** K×ABS(X)÷X at 0: the ruling's example. The prototype prints
   `UNDEFINED; K≠0` with status OK, to show the rows; see question 1 for the status.
6. **The sums: an undecided base refuses (abacus #9480).** sm_term refuses a real, letter-free base whose sign isn't
   decided with UNDECIDED itself, instead of handing the term to partfrac (whose 64-bit reading said NO_ROOM): the
   (1+Z30)^K rows. A number base (0^(K+1)) and i^K are unchanged (NO_CLOSED_FORM).
7. **Not changed:** letter-free answers (022's). A letter in the point (taylor at A, limit at K) stays BAD_ARG.
   canon's reading of the typed input (question 3).

## Measured (prototype p1, over fc46c8b; ~/scratch/casim/c23/p1, patch c23/p1-patch.py plus edits)

- **The probe:** fc46c8b 29 of 54 rows wrong at some K they don't exclude; p1 1, limit LN(K+X) at 0 → LN(K), which
  needs K>0: p1 writes rule 4's domain rows for taylor only, and the build writes them for both. Rows appear on those 29
  and on no other row. Some, before → p1:
  - limit K×ABS(X)÷X at 0: UNDEFINED → UNDEFINED; K≠0. (K^2-1)×ABS(X)÷X: UNDEFINED; K≠-1; K≠1.
  - limit SIN(K×X)÷(K×X) at 0: 1 (wrong at K = 0) → 1; K≠0. (X-SIN(X))÷SIN(K×X): 0 → 0; K≠0.
  - limit (K+X)÷(K-X) at 0, 0+ and 0-: 1 (wrong at K = 0: it is -1 there) → 1; K≠0. (K+X^2)÷(K-X) at 1+:
    (K+1)÷(K-1); K≠1.
  - limit SIN(X)÷((K-1)×X) at 0: 1÷(K-1) → 1÷(K-1); K≠1. SQRT(X^2+1)÷(K×X) at ∞: 1÷K → 1÷K; K≠0.
  - taylor TAN(K+X) to 1: TAN(K)+X÷COS(K)^2 (wrong at ±π÷2) → …; COS(K)≠0.
  - taylor LN(K+X) to 1: LN(K)+X÷K (no value at K ≤ 0) → …; K>0. SQRT(K+X): …; K>0.
  - taylor ABS(K+X) to 1: ABS(K)+X×SGN(K) (wrong at 0) → …; K≠0. 1÷(X-K) at 1: …; K≠1.
  - unchanged, right for every K: SIN(K×X)÷X → K, (EXP(K×X)-1)÷X → K, (1-COS(K×X))÷X^2 → K^2÷2, COS(K×X) → 1,
    1÷(K^2+1+X) (no real zero), EXP(K×X), 1÷(K×X+1).
- **The golden:** the corpus moves 8 lines, all letter limits over SIN(K×X) gaining K≠0 (9692, 9776, 9863, 9936, 9944,
  9969, 9976, 10031); →DEC and complex the same 8 inputs; the doc examples 1 row (022's K×ABS(X)÷X control → its
  rows), both ways. Nothing letter-free moves. The sums' change (rule 6) moves none (i^K first did, before rule 6
  excluded i).
- **S3 (p1):** text 240,554, +6,061 over fc46c8b's 234,493. p1 shares nothing with the sums' rows (its own row type,
  solver and printer); the build reuses smrow, sm_row and sm_print's loop, so I expect 3–4 KB. 27,651 B are left.
- **S3 frames (p1):** do_limit 240 → 352 (the rows in lwalk and a row array), do_taylor 336 → 384; new: the typed-tree
  walk 144 a level, the row writer 160 (it recurses on products), the row adder 80, the continuity rows' walk 32, four
  small ones at 32; ib_at was inlined by the compiler (its 32 gone). lm_walk, lm_quot and every other frame are
  unchanged. The typed-tree walk's depth on a letter input is enter()'s 44, so its frame matters: the build gives it
  taylor_hole's shape (the point's valuing out of line) and measures it.
- **Not measured yet:** the device (the stack and the 2,000 ms mask on the deepest letter nest), the mutants.

## Cost (estimate)

S3 +3–4 KB (p1 +5.8 KB unshared). Stack: do_limit and do_taylor aren't on the gate's path (derive's STU chain, 11,360);
the walk's 44 levels at about 144 B are 6.3 KB, under the gate, and the build aims at less. Time: the walk values each
part at the point, as taylor_hole does, but only on inputs with a letter; the deepest letter nest goes on the bench.

## The oracle

- **A public letter-values judge** (scripts/letter-values.py, grown from the probe): each row is run, and judged at
  seeded K and at each value its rows exclude. The answer must be right wherever its rows allow; at every K where the
  truth fails or the answer has no value, a row must exclude it; and every excluded value must be one where the formula
  or the truth fails (no row for nothing). Controls: a row dropped, a wrong row, a row on an all-K answer.
- **h023 is abacus's.** Its judge must read rows (h018's does).
- **Mutants, at least:** each site's row not recorded (1, 2, 4–10; site 3 isn't reached); a row written unsolved, and
  solved wrong (K≠-1 for K-1); product factors not split; the > row not implying ≠; rows printed on a refusal; the
  sums' refusal applied to i^K or to a number base; lm_combine's drop keeping an abandoned attempt's rows.

## Questions

1. **UNDEFINED with a condition: its status.** (a) status UNDEFINED, with the rows in out (programs that read the status
   keep working; the CLI prints `UNDEFINED; K≠0`); or (b) status OK and the text `UNDEFINED; K≠0`. My lean is (a). The
   firmware's display is soroban's side, as for 018's rows.
2. **Domains (rule 4).** Michael's words are about K≠0. LN(K+X)'s answer also needs K>0, and K≠0 alone leaves K = -1,
   where LN(K) has no real value. Rows say where the formula fails (018 rule 4b), so I lean to including > rows. The
   alternative is refusing (UNDECIDED) when a domain holds a letter. SQRT at order 0 only needs K ≥ 0, but its
   derivative divides by SQRT(K); the prototype writes K>0 at every order, and ≥ isn't a glyph yet.
3. **canon's reading of the typed input.** limit SIN(K×X)÷SIN(K×X), (K×X)÷(K×X) and (K×SIN(X))÷(K×X) at 0 print 1 with no
   row: canon cancels the letter factor as it reads the input, as simplify prints K÷K as 1. For a limit the condition
   would be "the divisor isn't 0 for every X near the point", which is hard to find in general (SIN(K×X) is 0 everywhere
   at K = 0). I propose leaving it out, stated in docs/language.md: the input is read as canon reads it, as everywhere
   in Casimir. Taylor differs on purpose: rule 7 values the typed f, so taylor K×X÷K at 0 gets K≠0 (p1: `X; K≠0`).
4. **Rows on taylor's typed f only, or also on the derivatives?** The prototype walks the typed f. The derivatives'
   divisors are its divisors' powers, and LN's and SQRT's arguments. In the probe's 23 taylor rows no answer's formula
   fails where no part of f does; h023 is the place to look for one.

## Ruled (#9493)

1. **Q1, (a):** status UNDEFINED, with the rows in out; the CLI prints `UNDEFINED; K≠0`. Programs that read the status
   keep working.
2. **Q2, > rows in,** solved as the ≠ rows are: SQRT(K^2+X) prints K≠0, not K^2>0. K>0 is right for SQRT at order 0
   too: at K = 0, SQRT(X) has no two-sided expansion at 0. (018 rule 4b.3's ABS(X)<1 replaces the X≠1 it implies.)
3. **Q3, out of scope,** with a line in docs/language.md: canon's reading comes before any decision, the class of the
   ruled 1÷c^K sums and the factored 1÷Z.
4. **Q4, the typed f only.** h023 aims rows at derivative-only cases (an odd root, (K+X)^(1/3)); one reopens it.

Conditions for the build:
1. **Signs aren't conditions:** inputs whose answer flips with K's sign (X^K at 0+, EXP(K×X) at ∞, K^X at ∞) refuse;
   none prints one side's value with a row.
2. **A cost stop: S3 +5 KB built.** Past it, back to abacus before landing.
3. **The stack on the device:** the deepest letter nest through taylor's typed-f walk, and limit's walk with rows
   recorded, each a peak against the gate under the 2,000 ms mask.
4. **Mutants:** as listed under The oracle, plus a > row not dropping its ≠ (rule 4) and a row recorded on a letter-free
   input (rule 2).
5. **The sums (rule 6):** h022's (1+Z30)^K rows go NO_ROOM → UNDECIDED and nothing else moves.

h023 is frozen before the build; only its hash comes to casim.

## Out of scope

The two-letter-ratio shift. The follow-up list's letter-free items (#9480 (1), #9482): sum 1÷c^K to 3, and canon's
factored 1÷Z.
