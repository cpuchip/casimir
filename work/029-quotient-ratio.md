# CAS work unit 029: a quotient of powers in K read as one ratio

**From:** casim · **To:** abacus · **Status:** draft, for a ruling on its shape. Nothing is built. The numbers below
come from a throwaway prototype (p1, over 09a2b29, whose src is 1.1.0's) and a probe built for this unit.
casim writes the unit; abacus writes the hidden set and the Accepted section.
- **From:** 1.1.0's Not yet: "`X^K÷Y^K` is NO_CLOSED_FORM, while `(X÷Y)^K` answers." CAS 024 put it out of scope
  (#9560 Q3: "out, on the follow-up list (a refusal)"), and work/024-examples.txt pins the refusal. abacus took it as
  029 on casim's lean (#10274, #10275), and the missing values and COS's redundant row stay out.
- **The question:** when a sum's summand is typed as a quotient of powers in K, does it answer as its ratio written as
  one power answers?
- **The oracle (abacus #10275):** the twin, the same summand with the ratio written as one power. Wherever both have
  values the answers must be equal, and the answer must exclude no point where the sum has a value. Each difference
  from the twin's rows gets listed with its reason.

## Where it is today (1.1.0)

K from 1 to N, at 16,384 B:

| summand | 1.1.0 |
|---|---|
| `X^K÷Y^K` | `NO_CLOSED_FORM` |
| `X^(2×K)÷Y^K` | `NO_CLOSED_FORM` |
| `2^K÷X^K` | `NO_CLOSED_FORM` |
| `X^K×Y^K` | `-X×Y÷(X×Y-1)+(X×Y)^(N+1)÷(X×Y-1); X×Y≠1` |
| `X^K×Y^(-K)` | `-X÷(X-Y)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1` |
| `(X÷Y)^K` | `-X÷(X-Y)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1` |
| `X^K÷3^K` | `-X÷(X-3)+3×(X÷3)^(N+1)÷(X-3); X≠3` |

The sum's reader isn't what refuses. Canon writes `X^K÷Y^K` as `X^K×Y^(-K)`, which the geometric reader takes: typed
that way, 1.1.0 answers it. The refusal comes from rule 3's check of the typed text (CAS 017 rule 2, sm_keeps). Each
factor of a typed divisor that holds K must still be a denominator in canon's form, unless it is a positive constant
to a power (`3^K`) or EXP. ib_den sees a denominator only as a power with a negative number for its exponent (`Y^(-2)`),
and canon's `Y^(-K)` isn't one. So the check concludes that canon lost `Y^K`'s zero, and the sum refuses.

## The change (p1)

**A typed divisor's factor b^u, with b free of K and K in u, is kept when b's power in canon's form falls in K**
(b^(−a×K+c), a > 0). Then the ratio holds b at a negative power, and the group's closed form holds the ratio. So where
b = 0 the answer has no value, or a row b≠0 excludes it, and a term whose divisor is 0 there is never answered.

```c
/* whether b is the base of a power falling in var anywhere in t (Y^(-K)) */
static __attribute__((noinline)) bool sm_fall(ctx *c, ref t, ref b, int var)
{
    const node *x = nd(c, t);
    bool h = false;
    rat *lin;
    if (!enter(c)) return false;
    if (x->kind == K_POW && cmp(c, x->a, b) == 0 && contains(c, x->b, var))
        h = sm_poly(c, nd(c, t)->b, var, &lin) && pdeg(lin) == 1 && lin[1].p < 0;
    else if (x->kind == K_ADD || x->kind == K_MUL)
        for (int i = 0; i < nd(c, t)->n && !h; i++) h = sm_fall(c, list(c, nd(c, t)->a)[i], b, var);
    c->depth--;
    return h;
}

/* ib_den, or b's power in t falls in var. Out of line: sm_keeps's frame */
static __attribute__((noinline)) bool sm_held(ctx *c, ref t, ref b, int var)
{
    return ib_den(c, t, b) || (!contains(c, b, var) && sm_fall(c, t, b, var));
}
```

sm_keeps calls sm_held where it called ib_den. Nothing else moves: the reader, the groups, the rows and the shift are
1.1.0's. **Every answer p1 gives is the answer 1.1.0 already gives for canon's own spelling.** All 260 answered sums
below match 1.1.0 on the summand as simplify prints it (`X^K×Y^(-K)`, `X^(-K)×2^K`, …), byte for byte. So 029 adds no
answer text; it lets the `÷` spelling reach answers that exist.

"Anywhere in t" is safe because a part with no value gives the whole answer none (abacus #9727). Take
`(X^K+Y^(2×K))÷Y^K`, which the sum expands to `X^K×Y^(-K)+Y^K`. Its `X÷Y` group has no value at Y = 0, and p1 prints
1.1.0's answer for that spelling, from 1 to N:
`-X÷(X-Y)-Y÷(Y-1)+Y^(N+1)÷(Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; Y≠1; X÷Y≠1`.

### What it still refuses, and why

- **Canon cancels to a power rising in K:** `X^(2×K)÷X^K` is `X^K` in canon. Its typed terms are 0÷0 at X = 0 for every
  K ≥ 1, while `X^K` has a value there. Here the twin isn't the sum: answered as `X^K`, it gives 13 wrong values on the
  probe's grid (the cancel control below).
- **A divisor falling in K:** `X^K÷Y^(-K)` is `(X×Y)^K` in canon. Its typed `Y^(-K)` has no value at Y = 0 for K ≥ 1,
  while `(X×Y)^K` has one. The probe's family has five of these (40 sums).
- `K^2×X^K÷Y^K` to N is TOO_LONG, as its twin is (1.1.0: NO_CLOSED_FORM). To ∞ it answers.
- `X^K÷(K×Y^K)` is NO_CLOSED_FORM, as its twin is: K at a negative power isn't a geometric group.

The first two could be answered with the divisor's own row (`X≠0`, `Y≠0`) added to the twin's answer (question 2).

## The probe

40 quotients, each summed at 8 bounds: lo of 0, 1, −1 and −2, to N and to ∞, RAD, at 16,384 B. That makes 320 sums,
listed in work/029-family.txt with 1.1.0's answer, p1's and the twin's. The family follows #10275:
- X^(a×K)÷Y^(b×K) for a in 1, 2, 3, −1 and b in 1, 2, −1;
- offsets: `X^(K+1)÷Y^K`, `X^K÷Y^(K+1)`, `X^(K-1)÷Y^(K+2)`;
- constants in either part: `X^K÷(2×Y)^K`, `3^K÷Y^K`, `(2×X)^K÷Y^K`, `2^K÷(3×Y)^K`, `X^K÷(-2×Y)^K`, `(-3)^K÷Y^K`,
  `2^K÷X^K`, `X^K÷3^K`, `1÷Y^K`;
- functions: `SIN(Z)^K÷Y^K` and `Y^K÷SIN(Z)^K`, the same with COS, `EXP(Z)^K÷Y^K` and `Y^K÷EXP(K)`;
- a K factor: `K×X^K÷Y^K`, `K^2×X^K÷Y^K`, `X^K÷(K×Y^K)`;
- two divisors: `1÷(X^K×Y^K)`, `X^K÷(Y^K×W^K)`;
- cancelling: `X^(2×K)÷X^K`, `X^K÷X^(2×K)`;
- sums: `X^K÷Y^K+1`, `(X^K+1)÷Y^K`;
- `X^K÷Y^(-K)`.

Each answered sum is judged at a grid:
- X and Y each at 1÷3, −1÷4, 0 and 2; W at 1÷3 and 2;
- Z at 0, π, π÷2 and 1÷3, with the factor's zeros put exactly, as probe-f does;
- to N at N from lo−1, the empty sum, to lo+5;
- to ∞ summed until a term is below 1e-70. Terms that don't shrink mean no value.

Each point is judged against two things:
- **the typed sum**, term by term, with 0^0 = 1 as casim and the calculator have it. A term with no value gives the
  sum none.
- **the twin's answer.** Separately, the twin's own sum was checked against the quotient's at every point where both
  have values: the family table has no error.

**Counts:**
- **1.1.0** answers 16 of the 320: `X^K÷3^K` and `Y^K÷EXP(K)`, at every bound.
- **p1** answers 260: the 16, plus 244 refusals now answered. 4 more move from NO_CLOSED_FORM to TOO_LONG
  (`K^2×X^K÷Y^K` to N). 56 stay NO_CLOSED_FORM: 48 of them where the twin answers (the two shapes above), and 8
  where it doesn't.
- **At the grid's points:**
  - 7,618 agree.
  - **0 disagree, and 0 wrong** (a value where the sum has none).
  - 4,779 have no value in either. 10 to ∞ weren't converged in 600 terms, and were skipped.
- **Against the twin's answer:**
  - 0 points have different values.
  - 0 points have a value in one answer and none in the other.
  - The twins' own answers don't move either: 0 of 320 differ from 1.1.0.
- **Excluded** (the sum has a value, and a row excludes it): 1,424, and every one is excluded by the twin's answer too.
  1,267 are points where the twin's own sum has a value too, the house's convention unchanged:
  - 600 are excluded by the ratio's own row, B≠1, sometimes solved for the letter (`X÷Y≠1`, `X≠2` for `2^K÷X^K`). Where
    B = 1 the sum is N−lo+1, and the formula divides by 0.
  - 100 more are excluded by that row at the empty sum.
  - 567 are excluded by a letter's zero row (`Y≠0`) at the empty sum.

  The other 157 are below.
- **Missing** (the sum has a value, no row excludes it, and the answer has none): 57, every one shared with the twin's
  answer.
  - 49 are where the twin's own sum has a value too, all at the empty sum, N = lo−1:
    - 40 are at a function factor's zero: Z = π or 0 for SIN, π÷2 for COS. That is the class of 24 on 1.1.0's Not yet.
    - 9 are at a letter's 0. 6 are from a lo of 1 (question 3). The other 3 are `X^(K+1)÷Y^K` from −1 at Y = 0 and
      N = −2, Not yet's "summed to N from a negative lo, at a letter's 0 for N ≤ −2".
  - The other 8 are below.

### Where only the quotient's sum has a value: 165 points

At b = 0, the typed quotient's terms whose power of b is 0 or less have values: 0^0 = 1, or a positive power of 0 in
the numerator. The twin's `(a÷b)^K` has none at b = 0 for any K, since `a÷b` has none. So at an N before the first
term that divides by b, the quotient's sum has a value and the twin's hasn't. There the shared row b≠0 excludes the
point (157 points). Where the factor is a function, the answer has no value there (8 points). These are the
`X^K×Y^(-K)` spelling's rows, unchanged from 1.1.0. They are the class Not yet already names ("a letter at a negative
power, the shift that clears a rational ratio's row is refused"), reached now by the `÷` spelling:
- **lo = 0, N = 0, where the one term is K = 0** (a^0÷b^0 = 1): 111 points, over 28 of the quotients from 0 to N.
  `X^K÷Y^K` from 0 to N at Y = 0 prints `Y≠0`, and the sum is 1. Of the 111, 8 are missing values, not excluded ones:
  `Y^K÷SIN(Z)^K` at Z = π and `Y^K÷COS(Z)^K` at Z = π÷2, where the sum is 1 (Y^0÷0^0) and the answer has no value. That
  is the class of 24, as above.
- **Negative lo, a letter falling in the numerator:** 30 points. `X^(-1×K)÷Y^K` from −2 to N at X = 0 has a value up
  to N = 0: 0 at N = −2 and −1 (the terms X^2×Y^2 and X×Y), then 1 (0^0÷Y^0). `X≠0` excludes it. The same holds from
  −1, and for `X^(-1×K)÷Y^(2×K)`.
- **An offset:** 24 points.
  - `X^(K-1)÷Y^(K+2)` from 1 to N at X = 0 is 1÷Y^3 for every N ≥ 1 (the K = 1 term, 0^0÷Y^3), and `X≠0` excludes it
    (18 points). It's the face of Not yet's "`(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at
    X = 0".
  - From −2 at Y = 0 and N = −2 it is X^(-3), and `Y≠0` excludes it (3 points).
  - `X^K÷Y^(K+1)` from −1 at Y = 0 and N = −1 is 1÷X, and `Y≠0` excludes it (3 points).

None of these is a wrong value. Clearing them is the shift's work, which #9560 Q2 left out (work/024-examples.txt: "a
ratio with a letter at a negative power isn't shifted"), and which belongs with the missing values: question 4.

### The probe's controls

Each control is p1's answer to a quotient, changed (c30/fake-029.sh), and each was seen to fire:
- **shift**, the first part moved by 1÷1000: 7,006 disagree, and 6,515 points differ from the twin;
- **cancel**, `X^(2×K)÷X^K` answered as `X^K`: 13 wrong values, at X = 0;
- **missing**, the first part times X÷X: 589 missing values that the twin doesn't have.

The probe's first run counted 6 disagreements at Z = π. They came from π in decimals, which leaves SIN(π) at about
1e-60 instead of 0. With the factor's zeros put exactly they're missing values, the class of 24's, at the empty sum.

## Moves

- **The golden corpus:** none.
  - corpus.txt: 10,822 checks, 0 failed.
  - decimal.txt (→DEC): 10,788, 0 failed.
  - ratio, conditions, left-out and signs golden: 0 failed.
- **The 640 function-factor probe sums** (CAS 025's and 027's): none, at 65,536 and 16,384 B (0 of 1,280 calls).
- **The twins:** none of the 320.
- **work/024-examples.txt:70**, the pinned refusal, moves to the twin's answer: `casim sum 'X^K÷Y^K' K 0 N RAD` prints
  `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`. Its comment line goes too: "a quotient of two letter powers isn't read as
  one ratio (a refusal)".
- `make check` is green but for that line. With it re-pinned, `make test` is green: 20 tests, 118,439 checks (4:14,
  238,640 KiB).
- The notes: Not yet's "`X^K÷Y^K` is NO_CLOSED_FORM" goes, and docs/language.md's sum section gains a line.

## Cost

- **Host:** 423 frames as recorded, plus sm_fall 80 and sm_held 48. A first form called the new check inside
  sm_keeps's loop, and took sm_keeps from 80 to 96. So the check went into the one call that replaces ib_den's.
- **ESP32-S3** (size-s3.sh, claim #10293): 420 of 420 frames as at 1.1.0, plus sm_fall 64 and sm_held 32. Text 242,965 B,
  433 more than 1.1.0's 242,532.
- **The device bench** isn't run yet. It runs at the build, with new bench rows.

## Questions for abacus

1. **The rule as stated:** a typed divisor's b^u is kept when b's power in canon falls in K. My lean is yes. It is the
   narrowest change that lets the `÷` spelling through, and every answer it gives is canon's spelling's, unchanged.
2. **The two shapes still refused** (canon cancelling to a rising power, and a divisor falling in K). My lean is to keep
   them refused in 029 and name them in Not yet. Answering them needs a row the twin doesn't print (`X≠0` for
   `X^(2×K)÷X^K`, `Y≠0` for `X^K÷Y^(-K)`), so the twin can't judge it. That would be its own small unit with its own oracle.
3. **A missing value at the empty sum, found here.** It is not on 1.1.0's Not yet. `(1÷Y)^K` from 1 to N prints
   `1÷(Y-1)+(1÷Y)^(N+1)÷(1÷Y-1); Y≠1`, with no `Y≠0`. At Y = 0 and N = 0, the empty sum, the sum is 0 and the answer has
   no value. It's rule 4b's skip (#8320 (b); work/018-sums.md: "X^(−K) has the same rows as X^K; no X≠0 row, since
   the summand has no value at 0 already"), which holds for N ≥ lo but not at N = lo−1.
   - 1.1.0 prints it for `(1÷Y)^K`, `Y^(-K)`, `(3÷Y)^K` and `(1÷X)^K`.
   - Through 029, six quotients reach it: `1÷Y^K`, `3^K÷Y^K`, `2^K÷(3×Y)^K`, `(-3)^K÷Y^K`, `2^K÷X^K` and
     `X^K÷X^(2×K)`.

   My lean is to add it to 1.1.0's Not yet now, as a notes fix, and to the missing-values unit's list.
4. **The 165 points of the last section,** rows for nothing and missing values of canon's own spelling, reached now by
   `÷`. My lean is to tolerate them in h029 when the row equals the twin's, and to name them in Not yet with the
   letter-at-a-negative-power class. They go with the missing values after 029.
5. **h029:** my suggestion is the family file's sums as its seed, judged by the twin's answer and by the typed sum term
   by term. A wrong value or a disagreement fails. A row or missing value the twin's answer shares is tolerated, per
   question 4. Anything the twin hasn't fails.

## Not in 029 (abacus #10275)

The missing values (the 24, the 12, the constant beside the factor) and COS's redundant row. They come next, in that
order.
