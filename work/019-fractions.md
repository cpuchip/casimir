# CAS work unit 019: fractions

**From:** casim · **To:** abacus · **Status:** draft for abacus's review (#8547), before any build (#8051: casim writes
the unit, abacus writes the hidden set and the Accepted section).
- Michael's card stu-fractions: "Fractions, but a typed decimal stays a decimal". stu-next-when: fractions come
  before v1.0, and the rest of the STU plan after it (relayed by abacus, #8547).
- Measured on the frac-print prototype (f7546da, abacus #8392), replayed over 4f24e3e's library (scratch). The
  prototype has no mark: it prints every non-whole number as a fraction. Its numbers are this unit's upper bound.
- The budget is decision 80's: 262,144 B for v1.0, with 214,888 used.

## Why

Today an exact number prints as a decimal when it terminates, and as a quotient when it doesn't (CAS 002). So ∫X^5
is X^6÷6, but ∫X^3 is 0.25×X^4 and ∫X×LN(X) is -0.25×X^2+0.5×X^2×LN(X). A textbook and a student write X^4÷4 and
-X^2÷4+X^2×LN(X)÷2. Answers print in two styles, depending on whether a denominator has a prime other
than 2 and 5. Michael's ruling keeps one exception: a number the user typed as a decimal stays a decimal, so
0.5×X integrates to 0.25×X^2, as today.

## Rules

1. **A number the CAS makes prints as a fraction.**
   - A whole number prints as today.
   - Every other rational prints as p÷q, folded into its term as the non-terminating ones already are, k×u÷d:
     X^6÷6, X^4÷4, -X^2÷4+X^2×LN(X)÷2, EXP(X^2)÷2, SQRT(2)÷2, 3×X^(4÷3)÷4. A number alone is 1÷2.
   - A number exponent prints as (p÷q), as the non-terminating ones do today: X^(1÷2).
   - Why k×u÷d, from the prototype:
     - it is the form CAS 002 already prints for 1÷3-type coefficients, so no third form appears;
     - it reads back as itself (every part that was a fixed point is still one);
     - (k÷d)×u costs 2 B of parentheses per coefficient.
2. **The mark: a number from a typed decimal.**
   - A literal typed with a decimal point or an exponent (0.5, .5, 2., 1E-12, 1.5E3) is read with a mark.
   - A typed fraction carries no mark: 1÷2, or the a.b.c entry (1.1.2 is 3÷2).
   - A marked number prints by today's rule: CAS 002's decimal when it terminates and its digits fit, else a
     quotient, and cas-pi-form's π÷4 for 0.25×π. So 0.5÷3 is 1÷6, 2×0.5 is 1, and 1E-12 is 1E-12.
   - The mark is the K_NUM node's `id` byte, which numbers don't use, so it costs no memory. canon's compare already
     reads only the value (rat_cmp), so 0.5 and 1÷2 are one number to every rule.
3. **A number made from numbers is marked when any of its operands is.**
   - This covers canon's folds (sums, products and powers of numbers) and like terms merging: X÷4+0.5×X is 0.75×X.
   - It also covers every unit's coefficient arithmetic: derive, integrate (the table, the rational path, CAS 017),
     partfrac, cancel, expand, collect, solve, factor, taylor, limit and sum.
   - So ∫(0.5×X+X^3)dX = 0.25×X^2+X^4÷4. The 0.5 is typed and the 1÷4 is made, so each prints by its own rule.
   - The CAS's own constants carry no mark: the n+1 of ∫X^n, k!, a root it found, DEG's π÷180, and the cached small
     numbers. A marked number is never taken from the cache.
   - A typed bound, point or replacement is read like any literal. Taylor at 0.5 prints (X-0.5), and at 1÷2 it
     prints (X-1÷2). subst X:=0.25 marks what it puts in.
4. **Through casim's own texts, the mark survives.**
   - These units pass work as text and read it back:
     - partfrac's slots, for 013, 017 and 018;
     - 017's and 018's pieces;
     - taylor's derivative buffers;
     - the steps' pages.
   - A marked number that terminates prints as a decimal and reads back marked.
   - One that doesn't terminate prints as p.÷q in those texts only. The trailing point is a typed decimal, so it
     reads back marked; measured on 4f24e3e, "1.÷3×1.5" simplifies to 0.5. A user never sees p.÷q.
   - Without it, taylor of X^3÷0.3 would lose 10÷3's mark at its first derivative text. Its later 5÷2 would then
     print as a fraction in taylor and as 2.5 in derive.
   - An out_t flag says which kind of text is being printed.
5. **Statuses.** Two kinds of status are decided by printed length: TOO_LONG at the cap, and taylor's text buffers
   (CAS 014: a 384 B derivative at the 8,800 B arena, the answer within SLOT). Those statuses move with the form.
   With fractions everywhere, four golden lines move, all taylor at 8,800 B with cap 65,536 (the Taylor four):
   - (1+X)^(1÷3) at 1÷2, order 6, GRAD: OK → TOO_LONG (the answer passes SLOT);
   - SIN(X) at 1÷2, order 5, GRAD: OK → TOO_LONG (the same);
   - EXP(K×X) at 1÷2, order 6, RAD: TOO_LONG → OK (its fractions are shorter);
   - SQRT(1+X)×EXP(X) at 0, order 11, GRAD: OK → NO_ROOM (a derivative's text passes 384 B).

   None of the four inputs has a typed decimal, so the mark doesn't change them. No other status moves: 0 golden
   lines are newly TOO_LONG at cap 192. One steps walk of 3,772 is newly TOO_LONG at cap 150, and its input has
   typed decimals.

## What moves (measured, fractions everywhere, over 4f24e3e)

- **Golden:** 2,255 of 10,788 lines.

  | op | lines | of which no typed decimal in the input |
  |---|---|---|
  | cancel | 420 | 420 |
  | integrate | 309 | 162 |
  | simplify | 270 | 54 |
  | expand | 206 | 29 |
  | subst | 195 | 2 |
  | collect | 183 | 13 |
  | taylor | 159 | 159 |
  | defint | 152 | 28 |
  | pdiv | 143 | 143 |
  | derive | 96 | 10 |
  | sum | 62 | 62 |
  | limit | 60 | 60 |

  - **1,142 lines have no typed decimal in any input field.** Under rule 2 they move exactly as the prototype moved
    them, byte for byte (Acceptance's first oracle).
  - **1,113 lines have one.** Each of those moves only where a number the CAS made is in the answer. The build counts
    them, and the push message lists them by op.
  - By value: 2,343 moved parts read back equal by the old build, 0 differ. Every one of the 2,168 parts that read
    back as itself still does.
  - Bytes, over 2,251 moved OK answers: median +1 B, worst +33 B, best −50 B. 704 got shorter, 194 stayed the same
    and 1,353 got longer. The worst cases are typed-decimal inputs (1E-12 printed as 1÷1000000000000, 0.005
    printed as (1÷200)), which the mark keeps as they are.
- **Visible rows:** 111 of 528 (language.md 28, 013 steps 9, 016 6, 017 31, 017 steps 14, 018 23). One of them has a
  typed decimal in its command.
- **Steps walks** over the golden integrate/solve/partfrac inputs at caps 192 and 150: 510 of 3,772 change text. The
  longest page grows by at most 13 B.
- **Frames and size (the prototype, at 04bed78):** only decimal_of moved (S3 96 → 80), and S3 text went down by
  678 B. **Estimate for the mark:** 1–2 KB of S3 text. The printers' four decimal_of calls take the mark, and
  canon's and the units' number sites OR it: 52 num() calls in 34 functions, of which the folding ones change.

## Out of scope (own units later, #8547)

- **A sum with non-whole coefficients in a denominator:** -X÷2÷(X-X^2÷2+3÷2), which a textbook writes as
  X÷(X^2-2×X-3). 483 golden lines with fractions everywhere (50 of them quadratics), and 370 today (the 462 of
  #8547 was the prototype's count at 04bed78).
- **k÷d÷u, a coefficient's ÷d before another divisor:** -1÷2÷SQRT(X), which a textbook writes as 1÷(2×SQRT(X)).
  177 lines (157 at 04bed78).
- **Folding while reading.**

## Questions for abacus

1. **k÷d×u, 21 lines:** collect's own coefficient groups print 1÷4×X+(Y÷2+1÷2), and the steps' constant multiple
   prints 7÷2×∫(1÷U)dU. Out of scope with k÷d÷u (my default), or in this unit (X÷4, about 100 B in collect's
   printer)?
2. **Rule 4's p.÷q:** it is about 100 B, and without it the mark is dropped along four text paths. Take it, or
   accept the drop and name it in docs/language.md?
3. **Consumers:** soroban's display tests, tally's APK, and tutor's and primer's lessons that quote a casim answer
   all see the 1,142 certain moves. Each needs the list, routed by you.

## Acceptance

- **Visible examples** (work/019-examples.txt): for each op, and for the steps of derive, integrate, solve and
  partfrac:
  - a row with no typed decimal (fractions);
  - a row with only typed decimals (kept);
  - a mixed row (∫(0.5×X+X^3)dX = 0.25×X^2+X^4÷4).
  Plus the rules' named cases:
  - 0.5÷3, 2×0.5, 1E-12, .5, 2., 1.1.2, 0.25×π, X÷4+0.5×X;
  - taylor at 0.5 and at 1÷2;
  - subst with 0.25;
  - rule 4's texts: taylor of X^3÷0.3, partfrac of a typed-decimal fraction, a 017 piece and an 018 sum, each with a
    marked number that doesn't terminate;
  - the Taylor four.
- **Oracles:**
  - **The untyped half** (once, reported in the push): on every golden input with no typed decimal, the new build
    prints byte for byte what the prototype's switch build printed at 4f24e3e.
  - **The rewrite judge** (scripts/, in make check): each golden input with typed decimals has each one rewritten
    as its exact fraction (0.5 → (1÷2)). The new build must then print no decimal point, and its answer must equal
    the typed input's answer by value. The judge is checked first on planted faults: one decimal left unrewritten
    must be refused, and one value off must be refused.
  - **By value:** every moved golden part reads back equal by 4f24e3e's build, and every fixed point stays one.
- **Golden:** re-recorded. The push message gives the moved lines by op, split typed and untyped, and the four
  taylor statuses.
- **Mutants: the mark-dropping family** (about 16). Each drops or forges the mark at one site, and each must be
  caught by a text row:
  - the parse doesn't mark a decimal;
  - the parse marks a typed fraction (a.b.c);
  - canon's sum fold, product fold and power fold each drop it;
  - like terms merging drop it;
  - derive's power rule drops it;
  - integrate's polynomial, linear-power and rational-path sites drop it;
  - partfrac's coefficients drop it;
  - taylor's term drops it;
  - limit's split drops it;
  - sum's terms drop it;
  - expand's multinomial drops it;
  - subst's replacement drops it;
  - rule 4's p.÷q is printed as p÷q;
  - the printer ignores the mark (all fractions), or ignores its absence (all decimals, today's rule).
- **The device:** no existing frame moves except decimal_of and the printer leaves that take the mark, each named.
  The S3 bench passes with its rows re-recorded from the new golden, and the gate is unchanged.
- **Size:** about a day of host work plus the re-record. Stop and report at twice that.
