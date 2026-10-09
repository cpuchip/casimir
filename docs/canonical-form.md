# casim's canonical form (units CAS 002 and 003)

One way to write each expression, so that "is this the same as that" is a text comparison, within the
units' rules (work/002-simplify.md, work/003-expand-collect-subst.md). This is the contract; `tests/` holds
the checks that judge it. CAS 003's operations (expand, collect, subst) are at the end; each prints in this
form.

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
a 192-byte power chain took 25 KB of stack on the S3; after, the worst measured case is 16.1 KB (and with
the depth guard, below, 12.1 KB).

Expansion makes garbage: each distributed product and partial sum is built and then merged away. So an
expansion step that leaves more than 512 bytes of it compacts: the step's result is copied up the arena,
every node it passed is left a forwarding reference, the constants' caches follow, and the copy slides back
down to where the step began. Each round of expand_fully (below) is compacted the same way. Checked by the
CAS 003 review against compaction switched off: 31,000 cases byte-identical, clean under ASan. The worst
191-byte expansion now takes 6.4 KB of arena on the S3 (it took more than 8 KB before).

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
folding a sum's or a product's numbers would not fit, none is folded. A sum's numbers, and the coefficients
of a run of like terms, are added one denominator at a time, the smallest first, and each denominator's
terms in order of value, from the low end while their sum is above 0 and from the high end otherwise. So a
partial sum of one denominator's terms never lies further from 0 than its largest term or its total, and
terms of one denominator meet each other before any other (2E18÷7−2E18÷7+3E18 is 3E18). The denominators'
totals have no such bound when they are added together, so when that does not fit, every term is walked
again in order of value, from both ends (2E18+1E18−4500000000000000001÷2 is 1499999999999999999÷2). Both
orders come from the values alone, so whether a sum fits depends on the values, not on the order they were
written in (3E18×X+3E18×X−3E18×X and −3E18×X+3E18×X+3E18×X are both 3000000000000000000×X; the third
review and its delta passes). Whole numbers are one denominator, so for them the bound is exact. A run that
still does not fit stays apart, in order of value; when its numbers stay apart in the merged term, that
term sorts again and can merge with its new neighbour. They stay apart, after every other
operand, in order of value, the identities dropped (1 in a product, 0 in a sum). In a product they follow
one sign rule: their absolute values, the product's sign on the first (so X−4E18×4E18 and −4E18×4E18+X
agree). A product has a coefficient only when it holds exactly one number; numbers kept apart print whole,
as d, (−d) or (p÷q). Written parentheses around a chain of numbers survive parsing, so (p÷q)×… reads back
as that one number and not as p×(1/q)×…. Grouped this way the result is deterministic and a fixed point;
two groupings of the same mathematics may still differ past 2^62 (the folding happens in different places).
One example: a sum whose like terms were left apart stays a sum on its own, but as a factor it is made
primitive, and the divided terms may then merge into one product that cannot be distributed back; so
S and 2×(S)÷2 can print differently, each a fixed point (the CAS 003 review's third pass).
Two fresh-eyes review passes found each of these cases; each is a test in tests/simplify_test.c.

**0 to a negative power** is undefined and stays as written, in the numerator: 0^(−1), 0^(−0.5).
(Written as 1÷0, SQRT(0) or 0 under a ÷, it would read back as something else.)

## The depth guard (units CAS 002b, 003 and 004)

Every structural walk (canonical form, derivative, expansion, comparison, printing, compaction) counts its
nesting; past 47 the call returns NO_ROOM (TOO_LONG before CAS 004, which split the two: TOO_LONG is now only
a result that does not fit). So the stack is bounded by construction (abacus's ruling: at most 12 KB). The
limit was 52 in CAS 002b. CAS 003's walks made deeper frames, and it was calibrated again on the S3 at the
trip point: 49 measured 12,464 bytes worst and 48 measured 12,064. In CAS 004 the bench's idle baseline was
found to move with the build (520 or 720 bytes; stable within a run), which put 48's worst at 12,272 against
the 12,288 ceiling: no margin, so abacus moved the guard to 47 (#4797). The parser is a loop and a left chain
of powers prints in one, so neither counts. The constructors (sum, product, power) are not counted: they
recurse one level on their own, under a walk that is.

What 47 refuses, measured on the host with the CLI (the count is the same on the chip):

| Nest | Answers to | NO_ROOM from |
|---|---|---|
| powers X^(X^(… , simplify and expand | 46 levels | 47 (189 bytes) |
| powers X^(X^(… , derive | 44 levels | 45 (181 bytes) |
| functions SIN(SIN(… , simplify | every 191-byte text (38 levels) | — |
| sums and products alternating: X+Y×(X+Y×(… , X×(Y+X×(… , 1÷(1+1÷(1+… | 23 levels | 24 (169 bytes) |
| −(X+−(X+… | 23 levels | 24 (121 bytes) |
| STU's X^X^…^X (a right chain) , simplify and expand | 47 levels | 48 (95 bytes) |
| STU's X^X^…^X , derive | 45 levels | 46 |

tests/derive_test.c pins two edges (SIN( simplify 46 and 47; X^( derive 44 and 45), so a move of the guard
shows there first. A nest that alternates sums and products costs two levels for each written one (each is a
sum node and a product node). At 52 such nests passed to 25 levels; at 48 and 47, some texts that simplified
in CAS 002 are refused (X×(Y+X×(… at 25 levels, 176 bytes, simplified at 43bb21d), and at 47 a 189-byte power
nest X^(X^(… of 47 levels is too. A refusal, not a wrong answer.

## The forms

| Kind | Canonical when |
|---|---|
| number | reduced p/q as above |
| variable, π | always |
| sum | flattened; at least two terms; at most one number, which is not 0; no two terms with equal monomials (like terms collected: 2×X + 3×X is 5×X); a numeric coefficient times a single sum is distributed (−(X+Y) is −X−Y) |
| product | flattened; at most one number (the coefficient), not 0 and not alone with coefficient 1; no two factors with equal bases (X×X^2 is X^3); a sum that is a factor is **primitive**: its first term's coefficient is 1, the content moved into the coefficient (2×(X+1)×Y and (2×X+2)×Y are both 2×Y×(X+1)), and the divided sum is a sum again, its runs merged where they now fit (so it may stop being a sum); a sum holding a term with numbers kept apart (past 2^62) has only its sign taken out (dividing that term by more and multiplying it back would not meet again; by −1 it meets exactly), the sign of the largest coefficient among its first plain terms, which flips exactly when the sum is negated, or 1 when no term is plain; an integer power of a product is distributed ((X×Y)^2 is X^2×Y^2) |
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
- A product holding a draw (RAND, SEED; see below) prints its coefficient whole and first, p÷q as one item,
  then its factors in their order, each joined by × or ÷ as it comes, so the draws happen in the order they
  were written: RAND÷3 prints as 1÷3×RAND, 1÷RAND×RAND stays (it is not RAND÷RAND).
- Parentheses are only those the core's precedence needs, as in CAS 001, with one exception: a chain of
  powers always has them, (a^b)^c or a^(b^c), never a bare a^b^c (abacus's decision 52, #4293: the STU
  mode groups powers right to left, the 33s and 35s modes left to right, and the text must read the same
  in each). A bare chain typed in is still read left to right, as the core reads it now, until unit 031.

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

## CAS 003: expand, collect, substitute

Each prints its result in the canonical form above, and returns TOO_LONG past 191 bytes (plus the NUL)
whatever the caller's cap (the unit's rule 8, accepted in #4264 point 4). With L=R, each side is treated.

**Expand** distributes × over + and raises a sum to a non-negative integer power (multinomial), at every
depth, function arguments included. A sum in a denominator, or to a negative or fractional power, stays,
with its inside expanded. Denominators are joined: a product's factors with negative exponents are
multiplied into one, so (X+1)÷X÷Y and (X+1)÷(X×Y) both expand to 1÷Y+1÷X÷Y. An expansion that would
have more than 64 distinct terms cannot print in 191 bytes, and stops with TOO_LONG before it is built.
Expanding can make a product that merges into a new sum or denominator (SQRT(X+1)×SQRT(X+1) is X+1), so
expand_fully repeats until a round changes nothing, at most 8 rounds. The review found 7 to 19 of 5,000
results that were not fixed points with one round; with the repeat it found 0 in 30,000. Eight rounds is
a bound, not a proof: a result that had not settled by then would be returned as it stood.

**Collect in a variable** expands only where the variable is, then prints Σ c_k × var^k: each k once, in
descending k, negative k as c×X^(−k). c_k is simplified, not expanded (X×(Y+1)^2 collects to (Y+1)^2×X),
and free of var; c_k = 1 prints X^k alone, −1 as −X^k, a sum c_k in parentheses, and a non-sum c_k's sign
folds into the joining −. A sum c_k is printed as it reads back as a factor, made primitive and
distributed again (a run its sum left apart can merge there), so collecting again changes nothing. Then the remainder: the terms where var is not a whole power (SIN(X), X^0.5),
simplified, after the lowest power term, so after c_0 when there are negative powers (#4264 point 5). A
term holding a draw always goes to the remainder, unmerged, in its order.

**Substitute** replaces every occurrence of var with the replacement as a whole, as if in parentheses
(X^2 with X = −Y is Y^2), then simplifies. The replacement may hold var (X → X+1). It is a second text,
parsed on its own, and may not be an equation. A SYNTAX offset in it counts as if the call were
eqn + "|" + repl (#4264 point 6). A replacement holding a draw is refused (NOT_DIFFERENTIABLE, naming the
draw) when it would be copied, since that would make one draw two; replacing a single occurrence is fine.

**Opaque names** (#4264 point 3). Every name derive refuses, the postfix !, and the indirect (i) and (j)
are opaque functions in simplify, expand, collect and subst: their arguments are canonical, and like terms
and factors collect (IP(X)+IP(X) is 2×IP(X)). derive keeps refusing them, and based numbers, complex
literals and vectors stay refused everywhere.

**Draws** (#4264 point 2). RAND and SEED change the value at each evaluation, so a subtree holding either
is impure: never merged, cancelled, folded, dropped or duplicated, never distributed over or expanded, and
impure operands keep their written order, after the pure ones (RAND+RAND stays; (X+1)×RAND; 0×RAND+X is
X+0×RAND). A power leaves an impure base as written.

What judges CAS 003: tests/expand_test.c, collect_test.c (it parses the printed shape itself and checks
each c_k is free of var by its value at two points), subst_test.c and opaque_test.c, with the visible
cases of work/003-examples.txt through the core's evaluator at 1E-24, idempotence over every case, the
arena starved at every size from 0 to 8,192 bytes, and the mutants in scripts/mutants.py.

## CAS 004: each text read as its stamp

On the calculator an equation carries the model it was typed in (the core's unit 031: 33, 35 or STU), and
its meaning is that model's. `casim_run` (casim.h) takes the operation, the text and its model, and for
subst the replacement and its own model. The five older calls read 35, as before.

- **STU** groups ^ right to left: 2^3^2 is 2^(3^2) = 512, and X^Y^Z is X^(Y^Z). A minus that starts an
  exponent ranks as any unary minus, below ^, so it covers the chain after it: 2^-3^2 is 2^-(3^2), and
  2^-3×4 is (2^-3)×4. Measured against the core's own eqn_eval at each model before it was built.
- **33 and 35** group left to right, and an exponent's minus binds to the next operand: 2^-3^2 is
  (2^-3)^2. On 20,000 strings the core's syntax check and casim's agree under both.
- **Implied products** (STU's 2A) are not read. A stored STU equation has its × written in (unit 031 rule
  7), and the firmware writes it into subst's replacement before the call (#4745).
- **The output is one text under every model.** Each chain of powers has its parentheses (decision 52)
  and each product its ×, so a result reads alike under STU, 33 and 35. tests/stamp_test.c checks the
  round trip under all three models over every visible example and seeded power chains, and each result's
  value against the core reading the input in its own model.
- **Depth.** A right chain nests through its exponents, so an STU chain X^X^…^X is held by the depth
  guard (above: 47 levels for simplify and expand, 45 for derive). A 191-byte X^X^… typed in STU is NO_ROOM,
  where in 35 it is a left chain, printed in a loop, and passes.
- **Statuses, and the calculator's messages** (rule 5, the texts approved in #4797): TOO_LONG only when
  the result does not fit, RESULT TOO LONG; NO_ROOM (new, value 5) when the arena runs out or the depth
  guard stops a walk, TOO COMPLEX; NOT_DIFFERENTIABLE, UNSUPPORTED with the refused name (out) on a second
  line where the screen has one, or from subst copying a draw, CAN'T COPY RAND (or SEED); SYNTAX, the core's
  SYNTAX ERROR; BAD_ARG, INVALID DATA. The firmware maps them; casim returns the status and out.
- **Frames.** No function in src/ has a frame over 1 KB: `make test` and scripts/size-s3.sh compile src/
  under -Wframe-larger-than=1024 -Werror, and each first proves the flag refuses a planted 2 KB frame
  (tests/frame_control.c). The largest today: literal(), 336 bytes on the host and 320 on the S3.

## CAS 005: solve

`casim_solve` (and casim_run's CASIM_SOLVE; the CLI's `solve "<t>" X RAD`) gives the real roots of an
equation, L=R as L−R=0 and an expression as expr=0, read as its model, in var.

**The equation as typed** (rule 2, amended b1a8a66 on the CAS 005 review). The coefficients come from the
parsed tree, not from the canonical form: each node's polynomial in var as 13 exact rationals in the arena,
sums and products multiplied out, left chains of + − × ÷ ^ walked in a loop. So the refusals apply to what
was typed, and no root is printed where the typed equation is undefined (canonicalizing first had X^2÷X
solve to X=0, LN(X)×0+X to X=0, SQRT(X)^2+4 to X=−4):

- NOT_POLYNOMIAL: var in a denominator, inside a function, or to a power that is not a whole number ≥ 0,
  even where it would cancel (X^2÷X, X^-1×X^2, (X^2−1)÷(X−1), 2^X).
- NOT_NUMERIC: a coefficient that is not an exact rational (another letter, π, SQRT(2), 2^0.5, a draw), or a
  division by a constant that is exactly 0.
- NO_ROOM: a degree past 12 at any step, even if it cancels later (X^13−X^13+X), or a number past 2^62.
- Division by a nonzero constant is fine (X÷2), and an exponent may be any constant expression with a whole
  value (X^(4÷2), X^2.0); a constant to an integer power is exact (2^(−1) is 1/2).
- Walking the typed tree also costs far less arena than expanding: (X−1)…(X−9) needed 10.8 KB expanded and
  now solves in the 8 KB device arena. Each level of nesting that alternates × and + holds one polynomial
  (208 bytes) while its inside is walked, so such a nest fits the 8 KB arena to about 16 levels
  (1×(0+1×(0+…X…)): 15 levels take 7,632 bytes) and is NO_ROOM past it; the depth guard stops it at 23.
  STU's right chains of ^ recurse through their exponents the same way: X^1^1^…^1 fits to 32 levels
  (8,096 bytes) and is NO_ROOM by 35. Left chains (sums, products, 35's powers) are loops and cost nothing
  a level.

**The polynomial.** The coefficients are cleared to integers (decimals are exact rationals: 0.25 is 1/4),
made primitive, the leading one positive. Degree ≤ 12 and every coefficient ≤ 10^7, or NO_ROOM, up front.

**The factors over Q.** Linear and quadratic factors, each with its multiplicity:

- Candidates are numeric: every complex root by Aberth's method in double (at most 150 rounds), and for a
  cluster of k approximations (a k-fold root) its centroid polished by Newton on the (k−1)th derivative,
  where the root is simple. A real candidate x and each q dividing the leading coefficient give p =
  round(q·x); a pair of candidates (two real, or a conjugate pair) and each a dividing the leading
  coefficient give b = −round(a·sum) and c = round(a·product).
- The only verification is exact division by the candidate, repeated for the multiplicity. A candidate the
  numbers miss can only leave a factor undivided, so the answer can only weaken from SOLVED to PARTIAL; it
  never prints a root that is not one.
- What is left after the search: degree 1 or 2 is a factor itself; degree 3 or more is reported.
- The prototype (throwaway, measured before the design): 270,000 polynomials built from known factors
  (rational roots to 20/20, real and complex irreducible quadratics, cubics x³−k; multiplicities to 3),
  degree ≤ 12, coefficients ≤ 10^7: no factor missed. tests/solve_test.c keeps a seeded set of that kind.

**int64 is enough** (abacus #4843–#4845). Mignotte's bound: a factor g of degree m of f has |g_j| ≤
C(m−1, j)·‖f‖₂ + C(m−1, j−1)·|lc f|. At these bounds ‖f‖₂ ≤ √13·10^7 ≈ 3.61×10^7 and |lc f| ≤ 10^7, so a
factor of degree ≤ 11 has coefficients ≤ 252·4.61×10^7 ≈ 1.16×10^10, and one of degree ≤ 10 ≤ 5.8×10^9.
Every polynomial divided is a factor of f. A linear factor qx−p has |p|, |q| ≤ 10^7 (p | a₀, q | lc), and
each intermediate of its division is q times a quotient coefficient: ≤ 1.2×10^17. A quadratic factor has
|a|, |c| ≤ 10^7 and |b| ≤ ‖f‖₂ + |lc| ≈ 4.61×10^7; its quotient's coefficients are ≤ 5.8×10^9, so each
product is ≤ 2.7×10^17 and each running remainder < 3.3×10^17. All under 2^63 ≈ 9.2×10^18. The arithmetic
stays checked: an overflow means "not a factor", which a true factor never causes.

**The answer** (approved in #4841): one line, its status first.

    SOLVED: X=2; X=3
    SOLVED: X=-2; X=1 (MULT 2)
    SOLVED: X=(1-SQRT(13))÷2; X=(1+SQRT(13))÷2
    PARTIAL, DEGREE 3 LEFT: X=1
    NO REAL ROOTS        NO SOLUTION        EVERY X

- **SOLVED:** every irreducible factor over Q has degree ≤ 2 (a quadratic without real roots adds none).
  **NO REAL ROOTS:** the same, with no real root. **PARTIAL, DEGREE n LEFT:** a factor of degree ≥ 3 is
  left, n the degree left in all (an irreducible quartic is PARTIAL even without real roots: it is not
  solved). NO SOLUTION for a nonzero constant, EVERY X for 0.
- Each distinct root once, ascending by value (compared in decimal128, 34 digits), "; " between, its
  multiplicity above 1 as " (MULT k)". A rational root as an integer or a÷b in lowest terms; an irrational
  one as (a±b×SQRT(r))÷d in lowest terms, r square-free, ÷d left out when d is 1 and b× when b is 1. This
  is the textbook's form, not 002's canonical one (which would print 2.5−SQRT(13)÷2): abacus amended rule 5
  for it. Every root's text evaluates on the core to the root.
- Past 191 bytes, TOO_LONG.

## CAS 006: integrate

`casim_integrate` (casim_run's CASIM_INTEGRATE; the CLI's `integrate "<t>" X RAD|DEG|GRAD`) gives an
antiderivative of an expression in var, read as its model, in 002's canonical form, **without "+C"**: C is a
register on the calculator, so "+C" in the text would be read as the stored C. The firmware shows "+C" beside the
answer. An equation is BAD_ARG.

**The table's forms, recognised as typed** (as CAS 005's rule 2):

- a sum term by term, a minus through; in a product, every factor free of var is one constant (it may hold other
  letters: ∫A×X^2 is A×X^3÷3), and what holds var must be one form below, or polynomials multiplied out (a
  polynomial times a polynomial);
- x^n and (aX+b)^n for any rational n ≠ −1, as (aX+b)^(n+1)÷((n+1)a), SQRT, SQ and INV included; n = −1 (1÷X,
  1÷(aX+b), INV) as LN(ABS(aX+b))÷a;
- EXP, SIN, COS, TAN, ALOG, SINH, COSH of a linear argument aX+b, by the linear substitution (÷a): −COS, SIN,
  −LN(ABS(COS)), ALOG÷LN(10), COSH, SINH;
- a and b come from CAS 005's walk of the typed tree, so they are rational numbers (SIN(A×X) is refused);
- SIN, COS and TAN carry the angle mode's factor (CAS 001 rule 3): 180÷π in DEG, 200÷π in GRAD; SINH, COSH, EXP
  and ALOG carry none;
- anything else is NOT_INTEGRABLE: by parts (X×LN(X), X×EXP(X)), a substitution that is not linear
  (X×EXP(X^2), SIN(X^2)), partial fractions (1÷(X^2−1)), SIN(X)^2, ABS(X), 2^X, and a quotient that would only
  become a form by simplifying (X^2÷X: as typed).

**Every answer is checked exactly before it prints:** F is differentiated by casim's derive in the angle mode,
and F′ − f, canonical and expanded, must be exactly 0. Two rewrites live in the check only (abacus #5124), never
in an answer or in 002's form: SGN(u)×ABS(u)^(−1) is u^(−1) (the derivative of LN(ABS(u)); both undefined at
u = 0), and TAN(u) is SIN(u)×COS(u)^(−1) (undefined exactly where COS is 0). An answer the check cannot close is
NOT_INTEGRABLE, never printed. The check cannot see a dropped ABS on x > 0; tests/integrate_test.c (and abacus's
judge) compare F(b) − F(a) with a quadrature of f on intervals on both sides of 0 with no singularity.

TOO_LONG past 191 bytes, whatever cap is.

## CAS 007: factor

`casim_factor` (casim_run's CASIM_FACTOR; the CLI's `factor "<t>" X RAD`) factors a polynomial in var over Q,
read as typed by CAS 005's walk with the same refusals (NOT_POLYNOMIAL, NOT_NUMERIC, NO_ROOM past degree 12 or
past |coefficient| 10^7 once cleared). An equation is BAD_ARG.

**The text:** the rational content first when it is not 1 (`2×…`, `1÷2×…`, never a decimal; `-1×…` for a
negative leading coefficient), then each irreducible factor once, primitive, with integers and a positive
leading coefficient, in 002's canonical form, `^k` for a multiplicity above 1. Factors run in ascending degree,
ties in 002's order of the factor (the order 002's own product would give them). A sum is parenthesised when it
stands among other items or carries ^k; a polynomial that does not factor prints as itself, cleared, with its
content; a constant prints as itself. TOO_LONG past 191 bytes.

**How the factors are found:**

1. The roots at 0 come out first (X^k).
2. Repeated factors: an m-fold root's approximations (Aberth) ring it, so solve's clusters, at its three radii,
   tightest first, give centroids, each polished by Newton on the (m−1)th derivative. Every subset of centroids
   of one multiplicity, times every d dividing the leading coefficient, gives a candidate with rounded
   coefficients, taken out as often as it divides exactly. Each factor it takes out gets the finder again,
   multiplicities multiplied, because a factor found this way may itself be a power: a 9-fold root's ring can
   split into two clusters whose pair gives (X+2)^2, four times (abacus #5203). This is a finder, not a proof.
3. What is left, and each factor the finder took out, goes to the complete search (rule 5a, abacus #5192):
   - the roots, Newton-polished, are enclosed in disks of radius n|W_i| (Weierstrass corrections, |h(z_i)|
     inflated by Horner's running-error bound, the radius by 1.01). The inclusion theorem is Braess and
     Hadeler, Numer. Math. 21 (1973) 161–165; abacus checked the record, and nobody here has read the
     paper's text, so the constant n|W_i| rests on memory and the papers citing it. Disjoint disks hold one
     root each.
   - a root is real when no other disk meets its disk's mirror; otherwise its conjugate is the one disk that
     does.
   - every conjugation-closed subset of size 1 to n/2, in ascending size, and every d dividing the leading
     coefficient that passes the constant-term window: d·e_k is a nonzero integer that divides a_0, so
     1 ≤ d·|e_k| ≤ |a_0|. |e_k| lies between the product of the |z| − ρ and the product of the |z| + ρ, so only the
     divisors in that window can give a factor (10^-9 of slack for the products' rounding). Then each d·e_j lies within d·E_j of its computed value, E_j = e_j(|z|+ρ) − e_j(|z|) +
     10^-13·e_j(|z|+ρ). No integer there rejects the subset. A half-width under 0.25 holds one integer at most,
     which gives a candidate, kept only if it divides exactly. The first factor found is irreducible, because
     nothing smaller divides.
   - after a split, the quotient keeps its roots (no second Aberth), with fresh disks.
   - overlapping disks, a conjugate that isn't unique, an interval 0.5 or wider, or 80,000 divisor checks in
     one call: **PARTIAL**, never a guess. The budget is a count, so the host and the S3 agree, and a divisor
     outside the window is neither checked nor counted. Rule 5a accepted 150,000, but on the S3 a check costs
     about 4.3 µs, not the prototype's 3, and 150,000 checks ran 1.13 s; 80,000 keeps the worst case under 1 s.
     A NaN anywhere (a radius, a bound) counts as unproven.

**Every answer is checked exactly before it prints:** the text is parsed back and walked to its coefficients,
and they must equal the input's exactly. Otherwise nothing is printed (NO_ROOM).

**CASIM_PARTIAL** prints the factorization with every proven factor in it, and the unproven ones multiplied
back into one factor left whole. The firmware shows a note beside it, such as MAY FACTOR FURTHER. It falls
where roots crowd too closely for the disks to separate (work/007-examples.txt F0021), or with many real
roots, a divisor-rich leading coefficient and a large |a_0|, which together spend the budget. Before the window,
4 of 10,000 random products of Eisenstein factors came back PARTIAL; with it, none of about 9,800. A fresh-eyes
review (89,000 inputs against SymPy as a black box, no wrong answer) found two more places it falls:

- **a repeated factor beside a nearly equal simple root**, about 1 in 10,000 random products: the cluster takes in
  the neighbour, the finder fails, and the overlapping disks leave the whole polynomial unsplit, linear factors
  included (`(11×X^3+6)^3×(3×X^2-2)`, roots −0.817 and −0.816). A square-free decomposition (Yun's, in casim's
  512-bit integers) would decide these; it is the next step if they matter.
- **Mignotte-type polynomials**, X^n − p(aX−1)^2 with a in the tens to hundreds: two roots within about 1/a^2, too
  close for the disks.

## CAS 008: exact values

002's form never evaluated a function at an exact point, so SIN(π÷6) stayed SIN(π÷6). From CAS 008, the two canonical
constructors evaluate the exact constants a student expects, so every operation shares it:

- **fn():** SIN, COS and TAN at an exact angle; ASIN, ACOS and ATAN at the values that table produces; LN(1) = 0,
  EXP(0) = 1; EXP(k×LN(a)) = a^k for a numeric a > 0 (exact when power() makes it so; EXP(X×LN(2)) stays).
- **power():** a perfect rational power, (p/q)^(m/n) with p, q > 0 exact n-th powers: 4^(3÷2) = 8, 0.25^1.5 = 0.125.
  SQRT(8) stays SQRT(8) (no square factors pulled out), and 12^(1÷3) stays.

**One table, three angle modes.** A constant argument is first turned into a rational multiple r of π: in RAD it
must be 0, π or q×π; in DEG a number d gives r = d/180, in GRAD r = d/200 (π inside a DEG or GRAD argument is just a
number of degrees). When 12r is an integer divisible by 2 or 3 (a multiple of π/6 or π/4), the value is
s × p/q × SQRT(k) from sin(nπ/12) (COS is n + 6) or tan(nπ/12), printed in 002's form: 0.5×SQRT(2), SQRT(3)÷3, -1.
Odd multiples of π/12, TAN at its poles, and any argument that isn't constant stay as written.

**The inverses** return the principal angle in the mode: ASIN and ATAN in [-π/2, π/2], ACOS in [0, π]. ATAN(1) is
0.25×π in RAD, 45 in DEG, 50 in GRAD; ACOS(-0.5) is 2×π÷3. Only the values the table prints are read (0, ±1/2,
±SQRT(2)÷2, ±SQRT(3)÷2, ±1, and ±SQRT(3)÷3, ±SQRT(3) for ATAN): ASIN(2) and ACOS(0.3) stay.

**CASIM_UNDEFINED:** a numeric 0 under a numeric negative power, whether the layer made the 0 (1÷COS(π÷2)) or it was
typed (1÷0, X÷0, (X-X)^(-2)): the expression has no value, and out is "". Before CAS 008 these printed as
0^(-1); the rule is uniform (abacus #5462). A symbolic exponent stays: 0^(-X) is 0^(-X). No internal path builds a
numeric power at a candidate point (solve's and factor's coefficient walks are rational arithmetic; integrate's
1÷a and 1÷(n+1) have a ≠ 0 and n ≠ -1 by construction), so the status is only ever about the user's expression.

**The read-back, rule 6:** each value evaluates on the core to the input's value, relative 1E-24, except an exact 0,
which passes against a residue within 1E-30 (the core's π has 34 digits: its SIN(π) is about -1.16E-34). The
suites' shared judge (tests/coreeval.h, ce_close) uses this rule, as abacus's does.

**The integer root** (int_root) returns at once for q > 62, since no root >= 2 of an n below 2^62 exists there, and
tries candidates from 2: an STU exponent chain can make q astronomically large, and the prototype's loop hung
stamp_test until this guard.

## CAS 009: definite integrals

`casim_defint(eqn, var, lo, hi, model, angle, …)` (casim_run's CASIM_DEFINT, with the upper bound in the request's
new last field, repl2; the CLI's `defint "<t>" X LO HI RAD`) gives ∫ from lo to hi exactly: F(hi) − F(lo), F being
integrate's verified antiderivative, substituted by integrate's own rebuilding walk (check_rewrite, which substitutes
when given a value and rewrites nothing then), so CAS 008's exact values apply: ∫SIN on [0, π] is 2, ∫1÷X on [1, 3]
is LN(3), ∫SQRT(X) on [0, 4] is 16÷3.

**The integrand** is integrate's, as typed; integrate's refusals pass through with their status. That includes a
letter inside a linear argument (1÷(A×X+1) is NOT_INTEGRABLE, as in CAS 006), so the interval check never sees one.

**The bounds** are parsed as subst parses its replacement, both in the integrand's model; their SYNTAX offsets count
as if the call were eqn|lo|hi. A bound with any letter is BAD_ARG; a bound with no value (LN(0), 1÷0) is UNDEFINED; a
bound that isn't real (SQRT(-1), ASIN(2)) is BAD_ARG. lo > hi gives the sign reversed; lo = hi gives 0.

**The interval check** walks the canonical integrand (not F: F's LN(ABS(COS(X))) would refuse TAN on [0, π÷4]) over
the closed interval, for every u linear in var (read from the canonical tree directly):
- under a negative power, u must not be 0 anywhere on it, nor within 1E-20 × max(1, |u|) of 0 (an endpoint pole is
  a pole);
- under a non-integer power, u must not be negative anywhere on it. The core's y^x has no real value for a negative
  y with any non-integer x, odd denominators included ((-8)^(1÷3) is a domain error, measured), so casim follows it;
  u = 0 is allowed under a positive power (0^(1÷3) = 0);
- TAN(u)'s poles, half + k × period in u's units (RAD π/2 + kπ, DEG 90 + 180k, GRAD 100 + 200k): k from abn_floor of
  (umin − half)/period, and both p_k and p_(k+1) tested against the interval widened by the tolerance.
- At a rational bound, u is computed exactly in rationals, so an edge exactly at a bound (SQRT(3×X−1) from 1÷3) is
  exactly 0, not a rounding below it; at other bounds u is computed in abn, and a fractional power's u is refused
  if it comes out below 0 at all (a refusal near the edge is allowed; a value across it is not).
- Any failure is the new status CASIM_IMPROPER.

**ABS and SGN of a constant fold** (in fn(), so in every operation): when the constant evaluator (abn, in the angle
mode: COS(2) is negative in RAD and positive in DEG) puts the value beyond 1E-20 of 0, ABS(u) is u or −u and SGN(u)
is 1 or −1. So F at a bound prints LN(3), not LN(ABS(3)). Text changes only where ABS takes a constant: ABS(π−4)
is −π+4, X×ABS(COS(2)) is −X×COS(2) in RAD. (SGN typed is refused by the parser; it occurs only inside integrate's
check.)

**Not reduced, by the unit:** LN(6)−LN(2) stays, as does LN(0.5×SQRT(2)), and SQRT(8)'s square factor stays
(2×8^1.5÷9): all exact, and whether to reduce them is a casimir-v1 question.

## CAS 010: polynomial division, GCD and cancelling

Three operations on polynomials in var with rational coefficients, read as typed by CAS 005's walk (degree ≤ 12,
coefficients ≤ 10^7 once cleared; another letter is NOT_NUMERIC and var in a function or a denominator
NOT_POLYNOMIAL, as solve and factor say it). The second polynomial is in repl, read in repl_model as subst reads its
replacement. An equation is BAD_ARG.

- **PDIV** (`pdiv "<f>" X "<g>"`): `q; r` with f = q×g + r and deg r < deg g, over Q in casim's int64 rationals
  (rat_add, rat_mul). The identity is checked exactly before printing. g = 0 is UNDEFINED; an overflow is NO_ROOM.
  Measured at the 8 KB arena, f of degree 2–12 over g of degree 1 to deg f: coefficients up to 10 answer 96% (4%
  TOO_LONG), up to 10^3 56% (22% NO_ROOM), up to 10^5 33% (57% NO_ROOM), up to 10^7 14% (78% NO_ROOM): the
  quotient's denominators grow like lc(g)^k.
- **PGCD** (`pgcd "<f>" X "<g>"`): the primitive integer polynomial with a positive leading coefficient (a gcd over Q
  is defined up to a constant; this is FACTOR's normalization). gcd(f, 0) is f's primitive part, gcd(0, 0) is 0, a
  constant gcd is 1. Found modulo 31-bit primes (each dividing neither leading coefficient), the smallest modular
  degree kept, the gcd mod p scaled to lc gcd(lc f, lc g) and combined by Garner into int64 (overflow-checked); a
  candidate is accepted only if it divides both exactly, which proves it is the gcd (the argument is at the test in
  src/casim.c). Eight primes without a passing candidate is NO_ROOM, never an unproven gcd. One prime almost always
  suffices: the candidate is gcd(lc f, lc g) times the monic gcd, and that factor carries the true gcd's own lc.
  The paths the real primes never take (an unlucky prime between lucky ones, a failing candidate, a reconstruction
  that wraps to a negative leading coefficient) are reached by tests/gcdprimes_check.c, which builds src/casim.c with
  primes 11 to 29 (CASIM_GCD_PRIMES) and constructs each case. Garner's overflow checks are not reachable within the
  bounds (two lucky 31-bit primes reconstruct any candidate) and stay as the defence.
- **CANCEL** (`cancel "<n÷d>" X`): the top division's two sides, reduced by their gcd: the reduced quotient, then a
  row for each value the cancelling removed: `X=a` for each rational root of the gcd (ascending, once; found by
  solve's exact search), then what is left of the gcd as `p=0` when it has no rational root. The removed values are
  where the typed quotient is undefined and the reduced one isn't; limits will read them. Reduced numerator × gcd =
  numerator and the same for the denominator are checked exactly. A gcd of 1 prints the quotient as 002 writes it.
  A denominator of 0 is UNDEFINED.
