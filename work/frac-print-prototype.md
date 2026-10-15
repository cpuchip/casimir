# frac-print: a prototype (abacus #8392)

**Not for main.** Michael is leaning toward printing exact numbers as fractions. This branch measures what that costs
and moves, so his card can carry the numbers. Measured over 04bed78's library; the golden corpus and the text rows are
**not** re-recorded here, so `make check` is red on this branch by design.

## What changed

One switch, `CASIM_FRAC_PRINT` (1 on this branch): `decimal_of` answers only for a whole number. Every other rational
prints as a quotient, through the path the non-terminating ones already take. `decimal_of` is marked noinline so that
no other frame moves.

**The fold: numerator-folded, k×u÷d** (-X^2÷4+X^2×LN(X)÷2, EXP(X^2)÷2, SQRT(2)÷2, 3×X^(4÷3)÷4). Why this one:
- It is the form CAS 002 already prints for 1÷3-type coefficients, so no third form appears and those rows don't move.
- It reads back as itself: every part that was a fixed point before is one after (2,098 of 2,098).
- (k÷d)×u costs 2 bytes of parentheses per coefficient. Each × and ÷ chain also reads left to right without them.

## The measures (scripts/frac-*.py; BASE = the same tree built with the switch at 0)

- **(a) Moved text, value unchanged.**
  - Golden: 2,187 of 10,463 lines move:

    | op | lines | op | lines | op | lines |
    |---|---|---|---|---|---|
    | cancel | 420 | integrate | 304 | simplify | 270 |
    | expand | 206 | subst | 195 | collect | 183 |
    | taylor | 158 | defint | 152 | pdiv | 143 |
    | derive | 96 | limit | 60 | | |

  - The read-back: each moved part, old text and new, is canonicalised by the BASE build. 2,272 of 2,272 print the same.
  - The instrument was checked with a planted ÷8→÷9 printing bug: 141 parts differed, then 0 once it was restored.
  - Example rows (units 005–017): 84 of 502 move. Docs and steps walks compared as text: 77 rows (language.md 24,
    017 examples 25, 017 steps 13, 013 steps 9, 016 examples 6).
  - Every value judge in make check stays green on the fraction build: log, step, istep, ssteps, partfrac, psteps
    and integrate. Only rows compared as text fail.
- **(b) Bytes.**
  - Over 2,183 moved OK answers: median +1 B (ratio 1.029), worst +33 B, best −50 B. 668 get shorter, 182 stay the
    same length and 1,333 get longer.
  - Newly TOO_LONG at cap 192: 0 golden lines and 0 steps walks.
  - At cap 150: 1 steps walk of 1,883.
  - Steps pages: 506 of the 3,766 walks change text; the longest page grows by at most 13 B.
  - **4 golden statuses move, all taylor at the 8 KB arena:** 2 OK → TOO_LONG, 1 OK → NO_ROOM, 1 TOO_LONG → OK. They
    come from taylor's internal text buffers (CAS 014), not from the answer's length. Not traced further here.
- **(c) Frames and size.**
  - Host: no frame moves except decimal_of 112 → 96. Text −376 B.
  - S3: no frame moves except decimal_of 96 → 80. Text 193,533 → 192,855 (−678).
  - Stack: no frame grows, so the worst stack can't grow. Not re-benched.
  - Arena: the printer uses none, except taylor's buffers above.
- **(d) What fractions alone don't fix** (counted over golden answers):
  - **A sum with non-whole coefficients in a denominator: 462 lines; 32 of them quadratics**, as in 018's
    0.5×X^(N+2)÷(X-0.5×X^2-0.5). Sums aren't on this branch, so the golden example is an expand row (X÷((X+1)×(X-3)) among nine):
    -X÷2÷(X-X^2÷2+3÷2) (a textbook's X÷(X^2-2×X-3)).
    - The fix, not built: at print time, pull the lcm of the sum's coefficient denominators, and the sign, out into
      the product's coefficient (a primitive-part print). The canon stays as it is. That needs the scale passed into
      print_term, or scaled nodes made in the arena (the printer has no arena today).
    - Reaching (X-1)^2 also needs a factor, either at the producer (018's answer) or at print time. That is CAS 007's
      factor on every denominator, which is too costly to do as a print rule.
  - **k÷d÷u, the coefficient's ÷d before another divisor: 157 lines** (-1÷2÷SQRT(X), 7÷2÷U). It is correct left to
    right, but a textbook writes 1÷(2×SQRT(X)). The fix is in print_product: one ÷(d×u) group, 2 B of parentheses.
  - **k÷d×u before a factor: 21 lines**, mostly collect's own coefficient groups (1÷4×X+…), and the steps' constant
    multiple, 7÷2×∫(1÷U)dU.
  - **Numbers kept apart: 15 lines** of (1÷200)×(1÷200)…, an existing unfolded-product form; the decimal hid it.
  - **Powers of ten: 8 lines**, where a typed 1E-12 reads back as 1÷1000000000000 (+13 B).

## The open question: a typed decimal

Today CAS 002 reads 0.5 as exactly 1÷2, so ∫ 0.5×X is X^2÷4 on this branch and 0.25×X^2 on main. Nothing in the tree
remembers how a number was typed. Keeping it would take:
- **Memory: nothing.** A K_NUM node doesn't use its `id` byte, so the flag fits there.
- **The parse:** set the flag on a literal with "." or "E".
- **Every fold:**
  - num() has 41 call sites. Each one that combines numbers must OR its operands' flags: canon's sums, products and
    powers, and every unit's coefficient arithmetic (derive, integrate, partfrac, taylor, sums).
  - small()'s shared cache can't hold a flagged number.
  - Canon's compare must ignore the flag, while a merge ORs it.
- **Paths through text** (partfrac's slots, 017's and 018's texts) keep the flag only if the printer prints a flagged
  number as a decimal, which this rule does.
- **Rules Michael has to set:** what 0.5÷3 prints (1÷6, or a rounded decimal that is no longer exact), and whether
  2×0.5 is "1" or "1.0".
- **Estimate:** 1–2 KB of S3 text and about a day, plus a golden re-record.
- **The risk is consistency.** A path that forgets to OR the flag prints a fraction where the user typed a decimal,
  in one op and not in another. Only a text check per op catches that, so every unit's examples would need a row
  with a typed decimal.
