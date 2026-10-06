# casim's canonical form (unit CAS 002)

One way to write each expression, so that "is this the same as that" is a text comparison, within the
unit's rules (work/002-simplify.md). This is the contract; `tests/` holds the checks that judge it.

## The pipeline

    text --parse--> raw tree --canon--> canonical tree --print--> text
                                         \--derive--> canonical derivative --print--> text

- **parse** is iterative (an explicit operator stack, no recursion), so its stack does not grow with
  nesting. Measured on the S3 before this design: CAS 001's recursive parser took 24.6 KB of stack for
  95 nested parentheses and 30.7 KB for 191 minus signs, against a 16 KB budget. It fails where and as
  the core's check-only pass fails (the one that reports SYNTAX ERROR when an equation is entered),
  checked against the core on 20,000 strings. A fraction that only the core's evaluating pass rejects
  (1.2.3E4, 1.2.) is a literal: printed back as typed, reported by the core when it evaluates. CAS 001's
  parser used the evaluating pass's rule for these; the syntax oracle's positive control found all 328
  of its disagreements with the core in that one class.
- **canon** builds the canonical tree bottom-up. Chains of one operator (X+X+…, ----X, …) are walked
  iteratively, so recursion depth is the depth of real nesting (functions, powers, mixed operators),
  not the length of a chain.
- **derive** works on the canonical tree, and builds its result through canon's constructors, so the
  derivative comes out canonical.

## Memory

Everything lives in the caller's arena. A node is 8 bytes on the host and on the S3 alike: a kind, an id,
a count, and two 16-bit references (arena offsets in 8-byte units, so up to 512 KB). A sum or product
points to an array of references; a number points to a 16-byte rational. So arena figures measured on the
host hold on the chip. The recursive walks (canon, derive, compare, print) keep only references in their
frames; numbers are read in out-of-line leaves, and a left chain of powers prints in a loop. Before that,
a 192-byte power chain took 25 KB of stack on the S3; after, the worst measured case is 16.1 KB.

## Numbers: exact rationals

A number is p/q with q > 0, gcd(p, q) = 1 and |p|, q below 2^62, held in 64-bit integers. Every operation
is exact and checked: a product or a sum that would overflow is reported, and Euclid's algorithm reduces
(gcd, Knuth, TAOCP vol. 2, section 4.5.2). Nothing passes through binary floating point, not even to
order two numbers: a/b against c/d compares their whole parts, then the reciprocals of their remainders,
in turn (their continued fractions), so no product can overflow. A decimal literal is the rational of the
value the core reads for it (0.1 is 1/10); a fraction a.b.c is (a×c+b)/c. A number that does not fit
(more than 18 digits, or a sum or product beyond 2^62) is simply not folded: a literal is kept as typed,
and folded operands stay as they are, in canonical order, so both texts of a pair still agree.

The first version held p and q in the core's decimal128 (`abn`), exactly too, with 34 digits. On the S3 it
made CAS 001's derivatives 15 to 45 times slower (D0001: 238 µs to 10.4 ms), and folding was 80% of the
time in a profile (Euclid on software decimal: gcd 18%, fmod 10%, isZero 13%). With 64-bit integers the
same derivative takes 0.9 ms, and a 191-byte equation simplifies in at most 2.1 ms.

Printed: a rational whose decimal expansion terminates within 18 digits prints as that decimal (0.3);
any other prints as a quotient (2÷3). A number is folded into a whole power only (0.25^1.5 stays: exact
roots are not part of CAS 002).

**Numbers that do not fold together** (a step past 2^62) are handled all or nothing: when any step of
folding a sum's or a product's numbers would not fit, none is folded. They stay apart, after every other
operand, in order of value, the identities dropped (1 in a product, 0 in a sum). In a product they follow
one sign rule: their absolute values, the product's sign on the first (so X−4E18×4E18 and −4E18×4E18+X
agree). A product has a coefficient only when it holds exactly one number; numbers kept apart print whole,
as d, (−d) or (p÷q). Written parentheses around a chain of numbers survive parsing, so (p÷q)×… reads back
as that one number and not as p×(1/q)×…. Grouped this way the result is deterministic and a fixed point;
two groupings of the same mathematics may still differ past 2^62 (the folding happens in different places).
Two fresh-eyes review passes found each of these cases; each is a test in tests/simplify_test.c.

**0 to a negative power** is undefined and stays as written, in the numerator: 0^(−1), 0^(−0.5).
(Written as 1÷0, SQRT(0) or 0 under a ÷, it would read back as something else.)

## The depth guard (unit CAS 002b)

Every structural walk (canonical form, derivative, comparison, printing) counts its nesting; past 52
the call returns TOO_LONG. So the stack is bounded by construction: on the S3 the deepest case of every
kind of nesting, swept across the limit, takes at most 11.3 KB (abacus's ruling: at most 12 KB). The
parser is a loop and a left chain of powers prints in one, so neither counts. A nest of powers or
functions costs one level per level of the text, and every 192-byte one passes (up to 51 levels); a nest
that alternates sums and products costs two (each level is a sum node and a product node), so it stops at
25 levels: 191-byte texts like X+Y×(X+Y×(… or 1÷(1+1÷(1+… past 25 levels, and -(X+-(X+… past 25,
are TOO_LONG. The constructors (sum, product, power) are not counted: they recurse one level on their
own, under a walk that is.

## The forms

| Kind | Canonical when |
|---|---|
| number | reduced p/q as above |
| variable, π | always |
| sum | flattened; at least two terms; at most one number, which is not 0; no two terms with equal monomials (like terms collected: 2×X + 3×X is 5×X); a numeric coefficient times a single sum is distributed (−(X+Y) is −X−Y) |
| product | flattened; at most one number (the coefficient), not 0 and not alone with coefficient 1; no two factors with equal bases (X×X^2 is X^3); a sum that is a factor is **primitive**: its first term's coefficient is 1, the content moved into the coefficient (2×(X+1)×Y and (2×X+2)×Y are both 2×Y×(X+1)); an integer power of a product is distributed ((X×Y)^2 is X^2×Y^2) |
| power u^e | e is not 0 or 1; a number to an integer power is folded; (u^a)^n with an integer n is u^(a×n); an integer power of a sum with content k is k^n × (primitive sum)^n |
| function | its argument is canonical; LN(EXP(u)) and EXP(LN(u)) are u, and likewise LOG and ALOG; SQ(u) is u^2, INV(u) is u^−1, SQRT(u) is u^(1/2); ABS of a number is folded |
| L=R | each side canonical |

Subtraction and division are not kinds: a−b is a + (−1)×b, a÷b is a × b^−1, −u is (−1)×u.

## The order

Sums and products sort their operands by one total order, at every depth, function arguments included.

- Kinds rank: variable < π < function < power < product < sum < number. Variables by letter (A…Z, then
  i); functions by name, then argument; powers by base, then exponent; products and sums by their
  operands, compared in order, then by count; numbers by value (ties broken on p, then q).
- A product's factors sort by **base**, then exponent (X is X^1), so like factors are adjacent; the
  coefficient is first.
- A sum's terms sort by **monomial** (the term without its coefficient), so like terms are adjacent; the
  constant is last.

Why primitive sums: distributing a number over a sum only when the sum stands alone made the result
depend on grouping. (π−Y)×2 alone is −2×Y+2×π, but inside 2×(π−Y)×(5÷Y) it was not distributed, so a
regrouped pair printed two texts (our pair test, 12 of 2921 before the rule). With every sum factor
primitive, both are −10×(Y−π)÷Y. It also keeps printing re-readable: the sign of a product is always in
its coefficient, never inside a sum factor, so −(S)÷X reads back to the same tree.

Plain numbers sort after every other factor (a power of a number, SQRT(1.5), would otherwise sort by its
base's value among them).

After sorting, adjacent like operands merge. Any merge can create something that sorts elsewhere or must
be flattened, folded or made primitive ((X+Y)^(1/2) × (X+Y)^(1/2) is X+Y, a sum inside a sum; SQRT(X^2)²
is X^2, a power of another base), so a level repeats flatten, fold, make primitive, sort and merge until
nothing merges. Skipping that repeat is the idempotence mutant the unit names; the first version repeated
only when a merge made a number or a product, and the review found the rest.

## The domain rule

A simplification may define a result where the original was undefined, never change a value where the
original is defined (the unit's rule 3). Each rule above was checked against it:

- u−u → 0, u÷u → 1, 0×u → 0, u^0 → 1, EXP(LN(u)) → u: these only extend.
- X^a × X^b → X^(a+b): where X < 0 both sides need integer exponents; where X = 0, positive ones.
- (u^a)^n → u^(a×n) for an integer n only. (X^2)^(1/2) is |X|, so it stays SQRT(X^2).
- (u×v)^n → u^n × v^n for an integer n only.
- No trig identities, no expansion of products of sums, no factoring (the unit's rule 4).

## Printing

Readable and re-readable: printing a canonical tree and parsing it back gives the same canonical tree
(idempotence).

- A sum prints its terms in order; a term with a negative coefficient prints with − (X−2×Y). A leading
  negative term prints as −2×Y+X.
- A product prints its coefficient's numerator and the factors with positive exponents, then ÷ and each
  factor with a negative exponent (with its exponent negated) and the coefficient's denominator:
  2×X÷(3×Y) prints as 2×X÷3÷Y. An exponent of 1/2 prints as SQRT(u); u^2 prints as u^2.
- Parentheses are only those the core's precedence needs, as in CAS 001.

## What judges it

- Pairs: each of the 12 visible pairs (work/002-examples.txt) and our own pair generator (commute,
  regroup, write a−b as −b+a) must give identical text; both texts and the result must evaluate through
  the core's eqn_eval to the same value at 1E-24.
- Idempotence over every case: simplify(simplify(t)) = simplify(t).
- Exact folding: 0.1+0.2 prints 0.3; 1÷3+1÷3 prints 2÷3.
- Each rule alone, and the domain rule's cases.
- CAS 001's tests unchanged: every derivative still right.
- Mutants (scripts/mutants.py): sorting skipped inside function arguments; a collection that drops a
  coefficient; folding through a double; sorting before collecting without the repeat.
- The S3 bench at 192 characters: stack ≤ 16 KB, arena ≤ 8 KB, ≤ 20 ms, casim ≤ 40 KB of flash.
