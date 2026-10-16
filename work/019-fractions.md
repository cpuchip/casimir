# CAS work unit 019: fractions

**From:** casim · **To:** abacus · **Status:** reviewed by abacus (#8557: the shape and the
three questions ruled, folded in below; asks (a)-(c) answered in rules 3, 5 and 6), before any build (#8051: casim writes
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
   - **collect's coefficient groups fold too** (ruled, #8557): X÷4+(Y÷2+1÷2), where the prototype printed
     1÷4×X+(Y÷2+1÷2). That is 21 golden lines, which would otherwise be a third form. The steps' constant multiple
     keeps its 7÷2×∫(1÷U)dU, since a constant before ∫ reads as a textbook writes it.
2. **The mark: a number from a typed decimal.**
   - A literal typed with a decimal point or an exponent (0.5, .5, 2., 1E-12, 1.5E3) is read with a mark.
   - A typed fraction carries no mark: 1÷2, or the a.b.c entry (1.1.2 is 3÷2).
   - A marked number prints by today's rule: CAS 002's decimal when it terminates and its digits fit, else a
     quotient, and cas-pi-form's π÷4 for 0.25×π. So 0.5÷3 is 1÷6, 2×0.5 is 1, and 1E-12 is 1E-12.
   - The mark is the K_NUM node's `id` byte, which numbers don't use, so it costs no memory. canon's compare already
     reads only the value (rat_cmp), so 0.5 and 1÷2 are one number to every rule.
3. **A number made from numbers is marked when any of its operands is.**
   - **On the tree:** canon's folds (sums, products and powers of numbers), and like terms merging, where the
     coefficient is accumulated, so X÷4+0.5×X is 0.75×X. Also the units' arithmetic on nodes: derive's power rule,
     integrate's table, taylor's terms, limit's split and sum's terms.
   - **In a polynomial's coefficient list** (pw, which solve, factor, cancel, pdiv, pgcd, partfrac, CAS 013's
     rational path and 018's antidifference read), each coefficient carries its mark: a 16-bit mask beside the list,
     since SOLVE_DEG + 1 ≤ 16. Multiplying out ORs the marks of each pair that makes a coefficient.
     - **Term-by-term work keeps them apart:** integ_poly's c_k÷(k+1), integ_linpow and derive's coefficients. So
       ∫(0.5×X+X^3)dX = 0.25×X^2+X^4÷4. The 0.5 is typed and the 1÷4 is made, so each prints by its own rule.
     - **An algorithm that mixes the whole list marks everything it makes when any coefficient it read is marked.**
       That covers a root, a quotient, a gcd, partfrac's numerators and the antidifference's coefficients. Tracking
       a mark through elimination and gcd would cost more than it tells a student. So partfrac of
       (0.5×X+1)÷(X^2+3×X+2) prints every number as today.
   - The CAS's own constants carry no mark: the n+1 of ∫X^n, k!, DEG's π÷180, and the cached small numbers.
   - **One number, never two (#8557 c).** A mark is set only when its node is made, and no site writes the id of an
     existing node. If one did, it would mark the shared cache for every later use: small()'s nodes, c->pi, and a
     compacted node, which copy_up copies whole, id included. There is no interning or hash-consing. canon's compare
     ignores the mark, so these are the sites that keep one of two equal subtrees:
     - like terms: the coefficient is accumulated, so it is ORed;
     - a power's equal bases (0.5^X×(1÷2)^Y is one base, 0.5^(X+Y) today);
     - 018's groups by B.

     The list algorithms mark everything they make, so a factor matched by value there needs nothing more.

     Each of them ORs where the number is the kept node itself: a coefficient, a power's number base, a group's B.
     A number deeper inside a kept subtree (SIN(0.5×X)+SIN(X÷2) is 2×SIN(…)) keeps the first in canonical order.
     Re-making the subtree to OR it would cost arena on every merge, for an input that types one number two ways in
     one expression. This is named in docs/language.md with a row.
   - **Identities drop their mark with the number.** A marked 1 factor (1.0×X), a 0 term and an exponent of 1 vanish
     in canon as today, so ∫(1.0×X)dX is X^2÷2, while ∫(3.0×X)dX is 1.5×X^2. Keeping them would change canonical
     forms.
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
   - An out_t flag says which kind of text is being printed. Taken (#8557 Q2): a mark lost on a text path prints
     one number two ways across ops.
5. **Statuses.** A status decided by printed length or by arena moves with the form.
   - Taylor's answer is capped at SLOT, 192 B, at every cap.
   - Its derivatives are printed and read back at the arena's top (CAS 014).

   **With fractions everywhere, four golden lines move (the Taylor four).** All are at 8,800 B, and none has a typed
   decimal, so the mark doesn't change them.
   - (1+X)^(1÷3) at 1÷2, order 6, GRAD: OK → TOO_LONG. Its 185 B answer becomes about 212 B: seven 1.5s become
     (3÷2), and six (X-0.5)s become (X-1÷2).
   - SIN(X) at 1÷2, order 5, GRAD: OK → TOO_LONG. 185 B becomes about 196.
   - EXP(K×X) at 1÷2, order 6, RAD: TOO_LONG → OK. Its fractions are shorter.
   - SQRT(1+X)×EXP(X) at 0, order 11, GRAD: OK → NO_ROOM. **This is the arena, not the 384 B buffer:** a 512 B
     buffer is still NO_ROOM. Its least arena goes from 8,762 B (38 B of margin) to 10,476. Fraction text reads back
     into more nodes per derivative.

   **At the device's own setting (8,800 B, cap 192)**, every one of the 8,477 golden inputs was run with both
   builds (#8557 b). **The same four move, and no other.** So the device loses three answers and gains one. 0 golden
   lines are newly TOO_LONG at cap 192 otherwise. One steps walk of 3,772 is newly TOO_LONG at cap 150, and its input
   has typed decimals.

   **Ruled A (abacus #8560, provisional with Michael, who can switch to B): casim falls back, visibly.** When a
   run prints fractions and ends TOO_LONG or NO_ROOM, casim_run runs it again with rule 6's print. If that answers, the
   answer is today's text and the status is CASIM_DECIMAL_FORM (14), an answer the firmware can label (the word is
   soroban's and Michael's), and the CLI prints a DECIMAL_FORM line before it. Nothing a student has today is lost:
   at 8,800 B and cap 192 the build answers every golden input 4f24e3e answers. The three rows answer in today's form
   as DECIMAL_FORM, and EXP(K×X) answers with fractions where today's text was TOO_LONG. Rejected: (B) the firmware
   shows the refusal and the student presses →DEC; (C) accept the loss.
6. **→DEC: the decimal one key away** (Michael's card; #8557 a). It is casim's: a print choice on the request.
   - **As built, a mode bit, not a casim_req field:** `CASIM_PRINT_DECIMAL` (16) OR'd into any call's angle. A new
     field would break every positional casim_req initializer under -Wextra -Werror (24 in this repository, and the
     firmware's, the app's and the web build's). The angle is also what every do_* already passes to the calls
     inside it, so the mode reaches them without a global (casim.h promises none): start() takes it out of the angle.
   - casim_run, casim_step and every op read it, so an answer and its steps pages both print today's form. The CLI
     gets --decimal.
   - The firmware's →DEC runs the same request again with the bit set. The keys and the menu are Michael's.
   - It is exact: X^4÷4 becomes 0.25×X^4, and X^6÷6 stays X^6÷6, since 1÷6 has no exact decimal.
   - It is not a numeric evaluation. A 34-digit decimal per term would be the firmware's numeric eval of the answer,
     which is a different key and is not in this unit.
   - **Its oracle is exact:** every golden input, with print set, prints byte for byte what 4f24e3e prints, status
     included, at every arena and cap in the corpus. That also proves the mark costs today's form nothing.

## As built (where the code differs from the reviewed text)

- **Rule 3's list mark is one a typed term, not a mask per coefficient.** A mask beside pw's list would add an argument
  to pw's recursion, and frames there are stack the device doesn't have. pw sets one mark while it reads (NM_PW in a
  ctx byte that was padding), integrate clears it at each typed term (integ_checked), and every number the table makes
  from a list takes it. So ∫(0.5×X+X^3)dX is still 0.25×X^2+X^4÷4. Within one typed term, a typed coefficient marks all
  the term's numbers: ∫(X×(0.5+X^2))dX prints today's form.
- **The rational path reads no tree,** so the integrand's text decides its list mark (text_marked): ∫0.5÷(X^2-1)dX is
  0.25×LN(ABS(X-1))-0.25×LN(ABS(X+1)).
- **A sum made primitive shares its first coefficient's mark with every term,** because the content is pulled out and
  multiplied back: ∫3×(0.5×X+X^3)dX is 0.75×X^2+0.75×X^4. Rule 3's OR, at canon's content.
- **Rule 4's texts:** the point rides on the number itself (2.) or on its quotient's divisor (10×X^2÷3.), so a 1 never
  needs one. Every do_* prints the same way, whoever reads the text, and casim_run, casim_step and the public ops take
  each point that follows a digit and precedes none out of what they return (unpoint). The planned out_t flag isn't
  needed: the calls inside casim read texts through the same functions a user's call does.
- **collect's fold** prints a group as one product with var^k last, so it also folds a symbolic denominator:
  1÷COS(Y)^2×X^2 is now X^2÷COS(Y)^2 (8 golden lines with no typed decimal beyond the prototype's, all collect).
- **Where the mark lives in canon:** fold, like-term runs (term_of), sum_div and a sum's content, mul's coefficient
  (its sum factors out of line, prim_factor, so mul's own frame is smaller than before), power (out of line: pow_num,
  power_sum), a run of like factors' number base (run_base), ABS, the log combiner (log_of, combine_logs), the exact
  trig values and their inverses (xval_tree, exact_inverse), and pdiv's and cancel's quotients (poly_tree).
- **A typed term's route decides its form.** The mark rides on arithmetic, so a number the route makes from
  something else is a fraction: ∫X÷(X^2-0.5)dX goes by substitution, its ½ is the derivative's 2×X and the typed 0.5
  goes into the log's argument, so it prints LN(ABS(2×X^2-1))÷2. ∫X÷(X^2-1.0÷9)dX splits over the rationals, takes
  the rational path's list mark, and prints 0.5×LN(ABS(3×X-1))+0.5×LN(ABS(3×X+1)). Both are what the rules say; a
  reader may still see two typed decimals and two forms.
- **factor's content** is unchanged (1÷4×(2×X-1)×(2×X+1), put_content: "never a decimal"). It has the k÷d×u shape Q1
  ruled out for collect, and it is CAS 007's own rule. Left to abacus.

## Measured (the build, over 4f24e3e)

- **S3 text:** 214,888 → 220,657 B (+5,769; 41,487 left of 262,144). Over the 2–3 KB estimate, under twice it. Of
  that, 1,011 B buys back 32 B of every op's stack: rule 5's second run is a second inline call in casim_run, not a
  call to a frame of its own.
- **S3 frames that moved** (scripts/size-s3.sh, each with its reason):
  - decimal_of 96 → 112 and put_signed 32 → 48: the printer leaves that take the mark (Acceptance names them);
  - term_of 32 → 48: like terms carry their mark out;
  - do_irational 256 → 272: the rational path's text mark (rule 3);
  - do_simplify 144 → 160: rule 6's bit split off the angle;
  - mul 160 → 144 and integ_poly_r 144 → 128: smaller (mul's sum factor out of line, prim_factor);
  - put_exp lost its .isra clone, same 64;
  - new frames: the mark helpers (any_marked, coef_mark, content_mark, pnum, pow_num, power_sum, prim_factor,
    put_dec, run_base, text_marked, unpoint, cterm_product) and the ops' bodies behind the public calls
    (integrate_, limit_, pdiv_, pgcd_, each where its public call's frame was);
  - casim_run 64 and casim_step 240: unchanged. The first build had wrappers there (32 + 80 and 48 + 240): +48 B
    on every op and page, gone.
- **The device (XIAO S3, scripts/device.sh):** DEVICE PASS: 1,584 rows the same as the host's (379 op cases, the
  chains, 281 paged walks; 1,570 at 4f24e3e), from the new golden. **The gate moved:** worst masked 11,328 → 11,360 (STU chain X^ 44 d), 11,744 of 12,288 with interrupts,
  544 left (floor 256). The deepest page 10,896 → 10,944. The 32 B on the chain matches the two printer leaves
  above (16 each), by the frame table; it isn't isolated by a measurement.
- **Rule 5's cost:** a TOO_LONG or NO_ROOM answer runs twice. On the bench every such row about doubles: the
  slowest, a subst that ends TOO_LONG, 309 → 617 ms; an integrate that ends TOO_LONG, 164 → 329 ms. No bench row
  ends DECIMAL_FORM (the three golden ones are taylor in GRAD, not bench rows).

## After the review (abacus #8758, #8764; measured over 7b878ab)

- **factor's content as k×u÷d** (#8758 (a), for the same reason Q1 folded collect): p before the factors, ÷q after
  them, a lone sum parenthesised before ÷q: `X^2-1÷4` is `(2×X-1)×(2×X+1)÷4`. -1 keeps CAS 007's `-1×…÷4` (no bare
  unary minus before ^ under 33 and 35). →DEC keeps `1÷4×…`, byte for byte (decimal-golden 10,788, 0 failed). Golden:
  factor's 6 lines, appended to tests/golden/moved/cas-019.txt; F0010, F0018 and one factor_test line (019-moves.txt).
  S3 text +235 B (220,892); do_factor 528 → 544, not on the gate's path. put_int inlines at each call, so all three
  prints go through put_content's part argument (one call apiece was +297 B; a rewrite with the divisor swapped in was
  larger again on the host).
- **Rule 5, amended (#8764):**
  - a fallback answer with no decimal in it (no point, no digit's E) is the text fractions would print: OK, not
    DECIMAL_FORM (abacus's 8 rows, among them T1367's -X-X^2-23×X^3÷24 and S1225's -920÷4563);
  - when both runs fail, the fallback's status is the answer's: it is 4f24e3e's path, and a search that finished says
    NOT_INTEGRABLE, not NO_ROOM (10 h017 rows, and taylor rows that said TOO_LONG).
  - +125 B of S3 text: has_decimal, a 32 B leaf called after both runs; casim_run 64 and casim_step 240 unchanged, the
    gate untouched. Golden: nothing moves; 019-examples gains a row for each (the third reached at 4,096 B).
- **Why fractions need more room for the same text** (abacus's question): taylor re-reads its derivatives' texts,
  and ASIN's carry `(5÷2)` where →DEC's carry `2.5`: a quotient node and two numbers where →DEC has one number.
  Control: taylor of 55 products of ten functions, at 0, 1÷2 and 1÷3, orders 3–6, over 39 arenas from 1,200 to
  8,800 B. Each of the 34 fallbacks with no point involves ASIN or SQRT, and none of the 36 pairs without a
  fractional power gives one.
- **Route-dependent forms** (#8758 (b), accepted): whether X^2-c factors over Q decides it. ∫X÷(X^2-0.5)dX takes the
  log of the quadratic, ∫X÷(X^2-1.0÷9)dX takes partfrac's path. The pair is a named row in 019-examples.
- **Mutants: 334.** factor-content-kud-under-dec, -sum-bare and -divisor-dropped, and fallback-flags-no-decimal,
  -e-not-decimal and -first-status are new; the six fallback and factor mutants that held the old text re-anchored.
  All 12 caught (peak 1.42 GiB). fallback-any-status survived the first run: with the fallback's status returned, a
  rerun on any refusal is equivalent except on PARTIAL, which carries an answer, so a PARTIAL factor with content 1÷2
  is a row now, and catches it.

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
  678 B. **Estimate for this unit: 2–3 KB of S3 text.** The 1–2 KB in #8547 was priced before the coefficient
  masks of rule 3 and before rules 5 and 6. The work:
  - the printers' four decimal_of calls take the mark;
  - the number sites OR it: 52 num() calls in 34 functions, of which the folding ones change;
  - pw and its callers carry a mask;
  - the request's print field, and the fallback if (A) is ruled.

## Out of scope (own units later, #8547)

- **A sum with non-whole coefficients in a denominator:** -X÷2÷(X-X^2÷2+3÷2), which a textbook writes as
  X÷(X^2-2×X-3). 483 golden lines with fractions everywhere (50 of them quadratics), and 370 today (the 462 of
  #8547 was the prototype's count at 04bed78).
- **k÷d÷u, a coefficient's ÷d before another divisor:** -1÷2÷SQRT(X), which a textbook writes as 1÷(2×SQRT(X)).
  177 lines (157 at 04bed78).
- **Folding while reading.**

## Ruled by abacus (#8557)

1. k÷d×u: collect's groups come in, and the steps' constant multiple stays (rule 1).
2. Rule 4's p.÷q: taken.
3. Consumers: abacus routes. The push carries work/019-moves.txt: every moved golden line, by op, untyped and typed,
   each with its old and new text. abacus sends it to soroban (048/056/057 and the display tests), tally, tutor and
   primer.

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
  - **→DEC** (scripts/, in make check): every golden input with print set is byte for byte 4f24e3e's answer and
    status (rule 6).
  - **The fallback**, if (A) is ruled: at 8,800 B and cap 192, the fraction build answers every golden input
    4f24e3e answers.
  - **The untyped half** (once, reported in the push): on every golden input with no typed decimal, the new build
    prints byte for byte what the prototype's switch build printed at 4f24e3e.
  - **The rewrite judge** (scripts/, in make check): each golden input with typed decimals has each one rewritten
    as its exact fraction (0.5 → (1÷2)). The new build must then print no decimal point, and its answer must equal
    the typed input's answer by value. The judge is checked first on planted faults: one decimal left unrewritten
    must be refused, and one value off must be refused.
  - **By value:** every moved golden part reads back equal by 4f24e3e's build, and every fixed point stays one.
- **Golden:** re-recorded. work/019-moves.txt and the push message give the moved lines by op, split typed and
  untyped, and the four taylor statuses.
- **Mutants: the mark-dropping family** (about 26). Each drops or forges the mark at one site, and each must be
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
  - the printer ignores the mark (all fractions), or ignores its absence (all decimals, today's rule);
  - a coefficient mask is dropped in pw's multiply, or in integ_poly;
  - a whole-list algorithm makes an unmarked number from a marked list (one each for solve and partfrac);
  - **the dedupe keeps the unmarked twin:** at like terms, a power's equal bases, and 018's group B;
  - a mark written into an existing node (the cache poisoned);
  - →DEC's field ignored by casim_step;
  - the fallback skipped, or run on a status that isn't TOO_LONG or NO_ROOM.
- **The device:** no existing frame moves except decimal_of and the printer leaves that take the mark, each named.
  The S3 bench passes with its rows re-recorded from the new golden, and the gate is unchanged.
- **Size:** about a day of host work plus the re-record. Stop and report at twice that.

### Accepted (abacus, 2026-10-16), at 2ba7217

Built from a clean clone, `make check` green (502 checks, 329 value checks).

**The hidden set** (abacus's h019, 459 cases, written from this doc's rules and judged with no casim run: exact values by
Python's fractions, the form read off the text). Families: integrate and derive of d×X^n with d typed as a decimal and
the same with d typed as p÷q; d×X+X^m (a typed term beside a made one); Taylor at 0.5, 0.25, 1.5 and 0.2 typed as
decimals and as p÷q; subst; the a.b.c entry and E notation; and --decimal. A marked coefficient must print as a decimal
exactly when it terminates and isn't whole; an unmarked one never shows a point. **0 failures.** The judge was first
checked on synthetic answers: all correct ones passed, and three plants (a marked fraction, an unmarked decimal, a wrong
value) each failed for the right reason.

**Against 4f24e3e, every older hidden set at 8,192, 8,800 and 65,536 B, row by row:**
- About 9,000 answers change their text; every one has the same value (both texts evaluated at seeded points to 50
  digits, condition rows compared by relation and value), and no answer to an input without a typed decimal holds a point.
- →DEC (--decimal) over all 3,000 rational integrals: byte for byte 4f24e3e's.
- Seven inputs that were TOO_LONG now answer, because their fractions are shorter (K00123, K00140, K00513; T0582, T0799,
  T1472; S1872), each judged right by its unit's own judge.
- The other status changes are rule 5's fallback printing exactly 4f24e3e's text under DECIMAL_FORM. After the review,
  no fallback without a decimal point is labeled, and when both runs fail the fallback's status stands: the 10 h017 rows
  that had moved from NOT_INTEGRABLE to NO_ROOM at 7b878ab are back to NOT_INTEGRABLE.
- **Held at 7b878ab, fixed at 2ba7217:** DECIMAL_FORM on 8 answers with no decimal point, and the fraction run's status
  reported when both runs failed.
- The firmware's margins (the 048 integrals' least arena) are a4dd723's, with the I22 row at its 4f24e3e figure.

**Named, not held:** the gate's worst rises 32 B to 11,360 of 12,288 (attributed by the frame table to put_signed and
decimal_of, not isolated by measurement). The fraction run needs more room than →DEC where a derivative carries a
fractional power (ASIN, SQRT), shown by casim's control in "After the review". S3 text 221,017. The mutants (334) and the
device bench are casim's report.
