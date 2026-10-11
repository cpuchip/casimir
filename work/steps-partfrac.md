# Show steps, 4: partial fractions (abacus decision 71; order #6602/#6604, proposal #6610, approved #6612)

The fourth op whose working casim_step pages (work/steps-derive.md, steps-integrate.md and steps-solve.md are the
others). Partfrac is not a tree either. It cancels, divides, factors, and then finds each numerator; the steps are that
route as a student writes it: set the unknowns, clear the denominator, cover up each linear factor's root, and equate
coefficients for what is left.

## The steps

Depth 0 is the fraction as it goes, depth 1 the solving. In order:

- **COMMON FACTOR:** `(n)÷(d)=(n')÷(d'); rows`. The input, cleared and multiplied out, then the quotient with the gcd
  out, then CANCEL's rows. Left out when the gcd is 1. The rule id is solve's (the gcd out; abacus #6612).
- **POLYNOMIAL PART:** `(n)÷(d)=q+(r)÷(d)`. Left out when the fraction is proper.
- **FACTOR:** `d=lc×F1^k1×F2^k2…`, in partfrac's order (rule 5). Left out when d is one factor with lc 1.
- **FORM:** `(r)÷(d)=A÷F1+B÷F1^2+(C×X+D)÷F2…`. One unknown numerator over each factor power, of degree deg F − 1, its
  highest power's letter first. The letters are A, B, … skipping the variable.
- **CLEARED:** `r=lc×(A×F1×F2+…)`. The form times d, each numerator times its cofactor (the other factors to their
  powers, and F to what is left), written as factors. No outer brackets when there is one term.
- **COVER-UP** (depth 1), one for each linear factor, in partfrac's order. The template puts the root into the cleared
  form, `X=a: r(a)=c×U`, where only that factor's highest power's unknown U survives and c is the cofactor at a. The
  result is `U=r(a)÷c`. This is the books' cover-up (Thompson's).
- **COEFFICIENTS** (depth 1), for the unknowns cover-up leaves. One template for every power of X below deg d,
  `X^m: left=Σ c×U`: the X^m coefficients of the cleared form, the cover-up values moved across. Every power is
  printed, `X^m: 0=0` included, so the last equations check the values (abacus #6607). Then one result,
  `U=…; V=…`.
- **ANSWER:** partfrac's own text, byte for byte, its rows included.

A fraction that is already one factor to the first power (after the common factor and the polynomial part) goes
straight to ANSWER, after a FACTOR line for its lc if it has one. CASIM_STEPS_ONE_LINE leaves out the templates
(COVER-UP's and COEFFICIENTS'). Every page returns partfrac's status: PARTIAL on each page when partfrac's answer is
(abacus #6612).

**The rule ids** (abacus #6612): from 0.9.0 the numbers never move. CASIM_RULE_FUNCTION + k holds for k < 256, and
new rules start at CASIM_RULE_EXT = 256: POLYNOMIAL PART, FACTOR, FORM, CLEARED, COVER-UP and COEFFICIENTS are 256 to
261, and casim_rule_name names both ranges.

## How

- casim_step calls do_partfrac whole, with its answer in a SLOT at the arena's top (the solve-steps pattern).
  do_partfrac leaves its kept block at the top of the arena it was given and its work area first below it. The walk
  reads both in place (held, not zeroed): the factors in rule 5's order, each with its cancelled powers, the reduced
  n and d, q, r, and the numerators. It takes its own room after them.
- The kept block's n is the rows' room in pf_work, so COMMON FACTOR rebuilds the input from the reduced n and d
  times what cancelled.
- A page's text, the cover-up's values and each equation's coefficients are computed only for the page asked for.
  Every page is counted on every call.
- Each page runs partfrac again (the API holds nothing between calls). Where the slot takes the room partfrac
  needed, plain partfrac's own run decides: its refusal, or a soft NO_ROOM (solve's oracle 6).
- New helpers only (ps_*), no new callers of old ones beyond CAS 012's own pf_* printers.

## The oracle (abacus #6612)

1. scripts/psteps-values.py (make check) judges every page exactly: the sides of COMMON FACTOR, POLYNOMIAL PART,
   FACTOR, FORM and CLEARED at points (FORM and CLEARED with the letters bound to the results); COVER-UP against
   CLEARED at the root; each COEFFICIENTS template as exactly CLEARED's X^m coefficient (an affine form in the
   letters); one equation for every power; the results satisfying them; the order, the depths, the letters; the
   last page equal to partfrac's answer; the one-line pages equal to the results. Planted faults are refused.
2. abacus's hidden steps judge over the 5,019 hidden fractions (the ssteps pattern).
3. Steps off is byte-identical: the golden corpus, the S3 frame table function by function, the bench.
4. The S3: the worst stack within the guard's gate, the pages' arena and time, flash, with a v1.0 forecast (each
   remaining item's estimate × 1.5).
