# CAS work unit 024: the shift for a ratio in two letters

**From:** casim · **To:** abacus · **Status:** draft, for abacus's review; measured on a throwaway prototype (p1, over
305bd84, not pushed). The hidden set's hash comes before the build.
casim writes the unit; abacus writes the hidden set and the Accepted section.
- **Why now:** the order abacus set for v1.0 (#9354): complex roots, the series unit, then the two-letter-ratio shift.
  CAS 018 named it when its one-letter shift landed (7fbcefa, abacus #8349): "Y^K×X^(K−1) still shows X≠0"
  (work/018-sums.md, named, not held; casim's limit, #8486). 022 and 023 came in between, ruled by Michael (#9428).
- **The question:** when a sum's ratio is a monomial in two letters or more, does the answer carry an L≠0 row only where
  the sum itself has no value at L = 0, as 018's shift already gives a ratio in one letter?

## Where it is today

Rule 4b (018) adds L≠0 for a letter L of the ratio where the answer has no value at L = 0 (the hole probe). The shift
(sm_shift, abacus #8349) sums a letter group q(K)×B^K whose q holds a negative power of B's letter as
q(K+s)×B^s×B^K over lo−s .. hi−s, so the answer divides by no power of L and the probe finds a value. It reads B with
sm_mono, which takes k×L^m in one letter, so a ratio in two letters is never shifted:

| Command | Prints at 305bd84 |
|---|---|
| `casim sum 'Y^K×X^(K-1)' K 1 N RAD` | `-Y÷(X×Y-1)-(X×Y)^(N+1)÷(X-X^2×Y); X≠0; X×Y≠1` |
| `casim sum 'X^(K-1)×Y^(K-1)' K 1 N RAD` | `-1÷(X×Y-1)-(X×Y)^(N+1)÷(X×Y-X^2×Y^2); X≠0; Y≠0; X×Y≠1` |

At X = 0 the first sum is Y (its K = 1 term, 0^0 = 1, and every later term 0), and the second is 1. Neither row says
where an answer fails that the sum doesn't. To ∞ the row isn't there today (Y^K×X^(K−1) from 1 to ∞ is
`-Y÷(X×Y-1); ABS(X×Y)<1`).

## Rules (proposed)

1. **A ratio that is a monomial in its letters is shifted as 018's is:** B = k × L1^m1 × L2^m2 × …, a rational k and
   each letter at a whole power 1 ≤ m ≤ 12. For each letter, s_L is the least shift that clears q's negative powers of L
   (⌈−e÷m⌉ for the most negative power e), and q becomes q(K+s)×B^s over lo−s .. hi−s as today.
2. **With two letters or more, only the letters a shift of at most lo clears** (lo a whole number; a symbolic lo: no
   shift): s is the largest s_L ≤ lo. A letter whose s_L passes lo has a term that divides by it, so the sum has no value
   at L = 0 and its row stands. Shifting past lo would leave B to a negative power in the answer at N = lo−1, and B is 0
   wherever any one letter is: without this rule the prototype printed, for Y^K×X^(K−1) from 0,
   `1÷(X-X^2×Y)+Y×(X×Y)^N÷(X×Y-1); X≠0; X×Y≠1`, which has no value at Y = 0, N = −1, where the empty sum is 0.
3. **One letter: unchanged.** 018's shift stays as accepted. With one letter, B is 0 only where that letter is, and a
   shift past lo keeps the letter's row, so the same negative power is never reached (question 1).
4. **The rows are rule 4b's, unchanged.** Only the answer they probe changes. A ratio with a letter at a negative power
   (X÷Y) and every other B isn't shifted (question 2).

## Measured (prototype p1, over 305bd84; ~/scratch/casim/wt-024, sm_shift and its call)

- **A probe of 23 two-letter sums** (c24/probe.py), each answer valued exactly against the sum added term by term at X
  and Y in {0, 1÷2, −1÷3, 2, −3÷2} for N from lo−1 to 6, and each L≠0 row against the truth (a term from lo divides by
  L). A typed ÷X is judged as canon reads it ((2×X×Y)^K÷X is 2^K×X^(K−1)×Y^K). At 305bd84: 17 of the 23 carry a row for
  nothing (21 rows), no value wrong. p1 without rule 2: 5 have no value at the empty sum where the sum is 0 (the shift
  past lo). p1: 0.
- **Rows that change** (p1, from 1 to N unless said; every probe row, as printed):

  | Sum | 305bd84's rows | p1's |
  |---|---|---|
  | Y^K×X^(K−1), (X×Y)^K÷X, X^(K−1)×Y^(K+1), K×X^(K−1)×Y^K; X^(K−2)×Y^K and X^(K−1)×Y^K from 2 | X≠0; X×Y≠1 | X×Y≠1 |
  | X^(2×K−1)×Y^K; (X^2×Y)^K÷X^3 from 2 | X≠0; X^2×Y≠1 | X^2×Y≠1 |
  | (2×X×Y)^K÷X | X≠0; 2×X×Y≠1 | 2×X×Y≠1 |
  | (X×Y÷2)^K÷X | X≠0; X×Y÷2≠1 | X×Y÷2≠1 |
  | (−X×Y)^K÷Y | Y≠0; −X×Y≠1 | −X×Y≠1 |
  | X^(K−1)×Y^(K−1); X^(K−1)×Y^(K−2) from 2; X^(K−3)×Y^(K−1) from 3 | X≠0; Y≠0; X×Y≠1 | X×Y≠1 |
  | X^(K−1)×Y^(2×K−1) | X≠0; Y≠0; X×Y^2≠1 | X×Y^2≠1 |
  | X^(K−1)×Y^(K−2) | X≠0; Y≠0; X×Y≠1 | Y≠0; X×Y≠1 (Y's s_L is 2, past lo) |
  | X^(K−3)×Y^(K−1) from 2 | X≠0; Y≠0; X×Y≠1 | X≠0; X×Y≠1 (X's s_L is 3, past lo) |
  | (K+1)×X^(K−1)×Y^(K−1) | TOO_LONG (the answer past 191 bytes) | an answer; X×Y≠1 (the shifted answer is shorter) |

  Unchanged, since a term from lo divides by the letter: Y^K×X^(K−1) from 0, X^(K−2)×Y^K from 1 and (X^2×Y)^K÷X^3
  from 1 (X≠0), X^(K−1)×Y^(K−1) from 0 (X≠0; Y≠0). Y^K÷X keeps Y≠1 alone: every term divides by X, 018's no-row case.
- **The golden:** the corpus moves 3 lines, all `sum Y^K×X^(K-1)` from 1, which lose X≠0 (the answer
  `Y×(X×Y)^N÷(X×Y-1)-Y÷(X×Y-1); X×Y≠1`). →DEC, complex and the series route compiled out move the same 3; the doc
  examples move 1, work/018-examples.txt:142, the row 018 wrote to show the limit. Nothing else moves.
- **The judge:** sum-values.py's reader gave 0 to a negative power as Infinity, not as no value (1÷0 raises). It hadn't
  mattered (its letter answers divide with ÷), but a shifted answer holds B^(N−1). It raises now; sum-values is 329
  checks, 0 failed, on 305bd84 and on p1.
- **S3 text: 239,257 B, +263 over 305bd84's 238,994** (sm_shift 878 → 1,156); 22,887 B left of decision 80's 262,144.
  **Frames:** sm_shift 128 → 96 on S3, 176 → 160 on the host; nothing else moved. The sums' deepest stack can't grow, and
  the device bench isn't on this path; the build reruns it anyway.
- **Rule 2's alternative, measured:** the same cap for one letter too moves 3 more corpus lines and one 018 example
  (K×X^(K−2) from 1 changes form; its X≠0 stands either way).

## Cost (estimate)

S3 +0.3 KB built. No new frame; sm_shift's shrinks.

## The oracle

- sum-values.py gains a two-letter family: q(K) × (c×X^a×Y^b)^K × X^e × Y^f with a and b in 1..2, e and f in 0..−3, a
  polynomial q to degree 2, lo in 0..3, to N and to ∞, judged exactly at a grid with 0 for both letters, N from lo−1, and
  each L≠0 row against the truth (a term from lo divides by L): no row for nothing, none missing. Controls: a row
  dropped, a row for nothing, a value moved.
- work/024-examples.txt: the table above, the kept rows, one letter unchanged (K×X^(K−1), K×X^(K−2) from 1), to ∞.
- Mutants: the multi-letter read off (one letter only again); rule 2's cap off; the cap applied to one letter; a letter
  at a negative power read as positive.
- The S3 frames, and the device bench at the shipping mask with sum rows that shift by two letters.

## Questions

1. **Rule 2 for two letters or more only** (my lean), or for one letter too? Uniform is simpler to state; it moves 018's
   K×X^(K−2) from 1 in form (3 corpus lines and an example) and changes no value or row.
2. **A ratio with a letter at a negative power stays unshifted** (my lean: out, on the follow-up list).
   (X÷Y)^K÷X from 1 prints `…; X≠0; Y≠0; X÷Y≠1`. The sum has a value at X = 0 (1÷Y), so X≠0 is a row for nothing. A
   shift by 1 clears X but puts 1÷Y into q, where Y≠0 stands anyway. It's a different reading of B, and its own probe.
3. **X^K÷Y^K says NO_CLOSED_FORM** (at 305bd84 and fc46c8b), while (X÷Y)^K and X^K×Y^(−K) answer. A refusal, not a
   wrong value: the quotient of two letter powers isn't read as one ratio. My lean: out of scope, on the follow-up list.

## Out of scope

Questions 2 and 3 unless ruled in. The follow-up list's items (#9480 (1), #9482): sum 1÷c^K to 3, and canon's factored
1÷Z.
