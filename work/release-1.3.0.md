Casimir 1.3.0: the known wrong answer found after 1.2.0 is fixed, and a sum's line no longer prints a letter's row that another row already says. Summed from 0 to ∞, `(Z÷Y+1)^K` printed `-Y÷Z; Z≠0; ABS(Z÷Y+1)<1` in every release from 1.0.0 to 1.2.0. That reads 0 at Y = 0, where every term from K = 1 has no value, so the sum has none, and no row excluded the point. It now prints `-Y÷Z; Y≠0; Z≠0; ABS(Z÷Y+1)<1`. 1.3.0's code is CAS 032's and CAS 033's, accepted at fde2bc3 and 63b9215, with the version text changed. src/casim.h's interface is 1.0's, but answers' bytes change, which is why this is 1.3.0 and not 1.2.1, as for 1.1.0 and 1.2.0: a moved answer has the same value wherever both lines have one, and the rows it gains exclude only points where the sum has no value.

## What's new since 1.2.0

- **A divisor inside the ratio, to ∞** (CAS 033, the fix). Where the ratio holds a letter under a division inside a sum, every term from K = 1 has no value where the divisor is 0, and the closed form can cancel the divisor:
  - `(Z÷Y+1)^K` and `(Z÷Y^2+1)^K` from 0 to ∞ get `Y≠0`, the letter's row, where the ratio itself has no value at the letter's 0;
  - `(Z÷(Y-1)+1)^K` gets `Y-1≠0`, and `(Z÷SIN(Y)+1)^K` gets `SIN(Y)≠0`: the divisor as the summand reads it, before the closed form;
  - where the answer itself has no value with the divisor at 0, it shows the hole, and no row is added: `K×(Z÷(Y-1)+1)^K` from 1 to ∞ prints its 188 B line as 1.2.0 did;
  - a divisor that is never 0 (EXP, COSH, 2^Y) gets none. To N nothing moves: each answer keeps the divisor, so it has no value where the sum has none.
- **A letter's row that another row says, left out** (CAS 032). A letter's row `L≠0` goes where a kept row has a value at L = 0 and is false there, whatever the other letters are:
  - `COS(Z)^K` from 1 to N is `-COS(Z)÷(COS(Z)-1)+COS(Z)^(N+1)÷(COS(Z)-1); COS(Z)≠1`, since COS(0) is 1. 1.2.0 printed `Z≠0` beside it;
  - `X^(-K)` from 0 to ∞ is `-1÷(1÷X-1); ABS(X)>1`, `(Y÷SIN(Z))^K` from 1 to N prints `SIN(Z)≠0; Y÷SIN(Z)≠1`, and `(K+1)×((Z-1)×Y)^K` from -1 to ∞ prints `-Y+Y×Z≠0; ABS(-Y+Y×Z)<1`, each without 1.2.0's letter row;
  - `EXP(X)^K` from 0 to N prints `EXP(X)≠1` alone, though `X≠0` names the same point more plainly. That reverses a choice made in 1.0's design, the plainer row first, and is the change's named cost.
- **How:** a row says `L≠0` only where every other letter in it is in a sum, a product or a whole power not below 0, so that it is false at L = 0 whatever they are: `Z÷Y+1≠1` has no value at Y = 0 and says nothing, and `Z×ABS(Y)+1≠1` keeps `Z≠0` (below). A row that gives way at the cap says it only on a line that keeps that row; where it gives way, the letter's row comes back, and the line is 1.2.0's.
- **The bytes it frees make room at the cap:** `K×SIN(Z)^(K-1)` from -1 to N prints `…; SIN(Z)≠0; SIN(Z)≠1` in 189 B, where 1.2.0's `SIN(Z)≠0` gave way (1.2.0's Not yet), and `K×COSH(Z)^(K-1)` from 1 to N, TOO_LONG in 1.2.0, fits in 187 B.
- **What moves:** CAS 032 moves 66 of the 1,280 calls of the 640 function-factor probe sums at two arenas, and 290 of a family's 5,280; 18 of the documented examples; 3 lines of the golden corpus, all `EXP(X)^K` from 0 to N. CAS 033 moves only its own class on those sets (12 calls), one example, and nothing in the golden corpus. Of the earlier units' hidden sets, CAS 032 moves 298 runs and CAS 033 20, all in CAS 032's set, each by its rule; the rest are 1.2.0's byte for byte.
- **Measured:** on the ESP32-S3 at 16,384 B, the ten new bench rows take at most 5,840 B of arena and 2,672 B of stack.

**The guarantee is unchanged:** an exact answer, or a status that says why not. Never an approximation presented as an answer.

## The known wrong answers

None. 1.0.0 to 1.2.0's, `(Z÷Y+1)^K` from 0 to ∞ reading 0 at Y = 0 with no row, prints its row now (above).

**Found after this release, on 26 Oct 2026.** canon may drop a part of the input that has no value at some points, since its domain rule lets it define a result where the input has none (u÷u is 1, u-u and 0×u are 0, u^0 is 1, EXP(LN(u)) and ALOG(LOG(u)) are u, SQRT(u)^2 is u). sum, defint and limit then print a value where the input as typed has none:
- Summed from 1 to N, `(Y÷Y)^K` prints `N`. At Y = 0 every term is 0÷0, so the sum has no value, while `N` has one. Summed from 0 to ∞, `(Z×LN(Y)÷LN(Y))^K` prints `-1÷(Z-1); ABS(Z)<1`, which has a value at Y = 1, where every term holds LN(1)÷LN(1), and at every Y ≤ 0, where LN(Y) has none.
- Summed from 0 to ∞, `(Z×EXP(LN(Y))+1)^K` prints `-1÷Y÷Z; Y≠0; Z≠0; ABS(Y×Z+1)<1`. At Y = -1 and Z = 1÷2 every term holds LN(-1), which has no real value, so the sum has none, while the answer is 2 there and each row holds. With ALOG(LOG(Y)), SQRT(Y)^2 or SQRT(Y)×SQRT(Y) in place of EXP(LN(Y)) it prints the same line.
- `defint "SQRT(X)^2" X -2 -1` prints `-3÷2`, though SQRT(X) has no real value on the interval (`SQRT(X)` over it is IMPROPER). `SQRT(X)×SQRT(X)` and `(X^(1÷4))^4` print the same, and `0×SQRT(X)` prints `0`.
- `limit "EXP(LN(X))" X -1 ''` prints `-1`, though LN(X) has no real value near -1.
- With another letter, a 0÷0 reaches limit and defint as well: `defint "X×Y÷Y" X 0 1` prints `1÷2` and `limit "X×Y÷Y" X 1 ''` prints `1`, each with a value at Y = 0, where the input has none.

limit also reads a function outside its real domain as typed, with no canon involved: `limit "SQRT(X)" X -1 ''` prints `SQRT(-1)`, and from below 0 it prints `0`, though SQRT(X) has no real value near -1 or below 0.

simplify promises no more than the domain rule (docs/canonical-form.md), and expand, collect, subst, derive and integrate answer through canon under the same rule. taylor and the polynomial ops read what was typed: `taylor "EXP(LN(X))" X -1 2` prints `UNDEFINED`, and solve, factor, cancel, partfrac and pdiv refuse these forms (NOT_POLYNOMIAL). 1.0.0 to 1.3.0 print every line above. The project's reviewer found it while testing a later unit, and a fix is being made (CAS 035).

## How it's tested (at this tag)

- `make test`: 20 programs with 118,439 checks. They read every answer back through an independent evaluator (the calculator firmware's core) and compare values.
- The golden corpus, 10,822 calls compared byte for byte: CAS 032 moves 3, each its line less `X≠0`, and CAS 033 none.
- Frame and symbol gates: no frame over 1 KiB, no `pow` linked, and every function's frame against the recorded table. No frame moved from 1.2.0, on the host or the S3. The new functions take 128 B (sm_said), 80 (sm_false0), 48 (sm_whole), 112 (sm_rowd_), 8 (sm_rowd), 80 (sm_holeb) and 80 (sm_rowfd) on the host, and 64, 64, 48, 64, 32, 48 and 48 B on the S3.
- Value checks beside the corpus judge Taylor polynomials, limits, integrals and sums against independent constructions: power series over fractions, numeric derivatives at 90 digits, and sums added term by term at 60 digits. New in 1.3.0: sums whose letter's row another row may say, predicted by construction and judged both ways (a row the rule drops but the line keeps, and a drop the rule doesn't allow); and sums holding a divisor inside the ratio, judged at each of its exact zeros, where a value with no row excluding it is a failure.
- CI on every push: `make check` under gcc and clang, `make test`, CMake under gcc and clang, WebAssembly, and the ESP-IDF component for the ESP32-S3.
- 477 planted mutants, each a fault the checks must catch: 475 are caught at this tag's tree (work/v1.3.0-mutants.txt), the 13 new on CAS 032's and CAS 033's rows among them. Two are named: cancel-rewind-keeps-cache, and ratio025-negpow-mono-row, whose fault CAS 032 made invisible. No input is known where either differs from the build.
- Hidden sets kept by the project's reviewer, independent of this repository, rerun at each unit's acceptance. At CAS 032's: its own, 1,456 runs at four arenas, judged by the rule (what it drops, what it keeps, and the give-way at the cap); at CAS 033's: its own, 484 runs at two arenas, judged at each divisor's exact zeros; and the earlier units' sets.
- On the ESP32-S3, every one of the bench's 1,394 cases is byte-identical to the host:
  - with a 16,384 B arena, the size the calculator runs it with;
  - worst stack 11,360 B read with interrupts masked, plus a 384 B interrupt allowance (two frames, or the most measured over the masked reading where that is larger), within a 12 KiB task;
  - nesting deeper than 44 levels is refused (NO_ROOM);
  - about 248 KB of Casimir's code at -O2 (248,362 B, 2,027 more than 1.2.0's).

## Not yet, after 1.3.0

These print a line without the note it could have, print a row the answer doesn't need, have no value where the sum has one, refuse, or print a constant as written. None of them is a wrong value.

- **A quotient that canon cancels, or whose divisor falls in K, is still refused.** `X^(2×K)÷X^K` is NO_CLOSED_FORM: canon reads it as `X^K`, which has a value at X = 0, where its terms are 0÷0. So is `X^K÷Y^(-K)`: its divisor has no value at Y = 0, while canon's `(X×Y)^K` has one. Answering them needs a row that `(X÷Y)^K`-style twins don't print (`X≠0`, `Y≠0`).
- **The `÷` spelling reaches the rows of the `×` one.** A quotient prints the answer its canon spelling did in 1.1.0, rows for nothing and missing values included. At a letter's 0, a quotient's terms whose power of the letter is 0 or less have values (0^0 = 1), and there the letter's row excludes the sum's value. `X^K÷Y^K` from 0 to N prints `-1÷(X÷Y-1)+(X÷Y)^(N+1)÷(X÷Y-1); Y≠0; X÷Y≠1`, though at Y = 0 and N = 0 the sum is 1, as `X^K×Y^(-K)` printed in 1.1.0. `X^(K-1)÷Y^(K+2)` from 1 to N prints `X≠0`, though at X = 0 the sum is 1÷Y^3 for every N ≥ 1. Where the factor is one of CAS 030's functions of a letter, its row excludes the point now: `Y^K÷SIN(Z)^K` from 0 to N prints `SIN(Z)≠0`, though at Z = π and N = 0 the sum is 1 (rows for nothing, below). They're the class below, where the shift that clears a rational ratio's row is refused past a letter at a negative power.

- **A note that doesn't fit.** Where the line with the note would pass solve's 191 B, solve prints the line as 1.0.1 did, with no note, never TOO_LONG for it and with no root dropped. `(7×X-2)×(3×X^2-9×X-7)×(8×X^2+3×X-2)×(8×X-3)×(X^2-3×X+8)×(8×X^2+6×X-1)×(1×X^2+9×X+3)` prints its ten real roots in 184 B as `SOLVED: …`, and `, 2 CPLX` would make it 192 B. With `--complex` the same input is TOO_LONG. In a family of degree-12 polynomials built to reach the cap, 59 of 1,416 noted answers give way at one seed and 69 of 1,403 at another; none in the golden corpus does.
- **The complex roots of a factor of degree 3 or more** aren't counted, since solve doesn't find them, with the bit or without it: `X^4+1` is `PARTIAL, DEGREE 4 LEFT`.

- **Rows for nothing.** Each is excluded where the sum has a value, while the answer has none there:
  - a function factor's row to N, at an N before the first term that divides by the factor (CAS 030, as a letter's row): `(Y÷SIN(Z))^K` from 1 to N prints `SIN(Z)≠0`, which excludes Z = π at N = 0, where the sum is 0, as its twin's `X≠0` excludes X = 0. For every N from 1 the sum has no value there, and the row is the one it needs;
  - `(X÷Y)^K÷X` from 1 to N prints `X≠0`, though the sum has a value at X = 0;
  - a row that excludes no point: casim doesn't know a polynomial's real zeros here, so to ∞ a divisor inside the ratio that has none gets its row all the same (CAS 033): `(Z÷(Y^2+1)+1)^K` from 0 to ∞ prints `Y^2+1≠0`, true everywhere;
  - past an irrational constant, to N, the shift that clears a rational ratio's row is refused, and the cancel (above) doesn't reach a power of N. `(K-1)×(SQRT(2)×X×Y)^K÷X` from 1 to N, TOO_LONG in 1.1.2, now prints `-N×(X×Y×SQRT(2))^(N+1)÷(X-X^2×Y×SQRT(2))-X×Y^2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2)+Y×(X×Y×SQRT(2))^(N+1)×SQRT(2)÷2÷(X×Y×SQRT(2)-X^2×Y^2-1÷2); X≠0; X×Y×SQRT(2)≠1`, though the sum is 0 at X = 0. To ∞ the cancel clears it.
- **A redundant row the rule doesn't reach** (CAS 032 leaves out a letter's row only where its rule says so). Where the row beside it holds another letter inside a function or ABS, or under a division, the letter's row stays though it may exclude nothing more: `(1+ABS(Y)×Z)^K` from 0 to N keeps `Z≠0` beside `Z×ABS(Y)+1≠1`, and to ∞ a ratio in two letters keeps its letters' rows beside `ABS(…)<1`, which holds them inside ABS: `(1+Z×Y)^K` from 0 to ∞ prints `Y≠0; Z≠0; ABS(Y×Z+1)<1`.
- **A missing value.** Each has no value at a point where the sum has one, and no row excludes that point:
  - at the empty sum, at a letter's 0, from a lo of 1 or more (found after 1.1.0's release, by CAS 029's probe). `(1÷Y)^K` from 1 to N prints `1÷(Y-1)+(1÷Y)^(N+1)÷(1÷Y-1); Y≠1`. At Y = 0 and N = 0 the sum is 0, the empty sum, while the answer has no value. A ratio holding the letter at a negative power gets no row for its 0 from a lo of 1 (abacus #8320 (b)), since every term from 1 has no value there; the empty sum still has one. The same holds for `Y^(-K)`, `(3÷Y)^K` and `(1÷X)^K`;
  - at the empty sum, where a factor outside the ratio has no value (found after 1.1.1's version, by CAS 030's probe; as at 1.1.0). `1÷X` from 1 to N prints `N÷X`, which has no value at X = 0 and N = 0, where the sum is 0. Every term has no value there, and no row excludes the empty sum. The same holds for `Y^K÷X`, and for `K×Y^K÷SIN(Z)` at Z = π;
  - summed to N from a negative lo, at a letter's 0 for N ≤ −2. `X^(K+2)×Y^(-2×K)` from -2 to N prints `X^2×(X÷Y^2)^(N+1)÷(X÷Y^2-1)-Y^4÷(X÷Y^2-1); Y≠0; X÷Y^2≠1`. At X = 0 and N = -2 the sum is Y^4, while X^2×(X÷Y^2)^(-1) has no value;
  - at a zero of a function factor other than its letter's 0, where the same sum with the factor read as a letter, its rational twin, prints that letter's row for nothing. To N, at an N before the first term that divides by the factor, CAS 030 gives SIN, COS, TAN, LN, LOG, ACOS and ACOSH of a letter their row (above); of the 640 probe sums, 2 of 1.1.1's 24 are left, each at the cap. What stays:
    - a line within its row of the cap: where the line with the row would pass 191 B, the row gives way. `(K-1)×(COS(Z)×Y)^K÷COS(Z)` from 0 to N prints its 181 B line as 1.2.0 did, with no value at Z = π÷2 for N = -1, where the sum is 0, and the row would take it to 193 B. Its SIN twin is the same at Z = π. CAS 032 made room for the row in `K×SIN(Z)^(K-1)` from -1 and its kin (above);
    - a polynomial factor, whose real zeros casim doesn't know here (Z^2+1 has none). `X^K÷(Y-1)^K` from 1 to N prints `X÷(Y-X×Y÷(Y-1)+X÷(Y-1)-1)+(X÷(Y-1))^(N+1)÷(X÷(Y-1)-1); X÷(Y-1)≠1`, with no value at Y = 1 for N = 0, where the sum is 0;
    - a function of anything but a letter. `(Y÷SIN(2×Z))^K` from 1 to N prints `-Y÷(Y-SIN(2×Z))+(Y÷SIN(2×Z))^(N+1)÷(Y÷SIN(2×Z)-1); Z≠0; Y÷SIN(2×Z)≠1`, with no value at Z = π÷2 for N = 0, where the sum is 0;
- A letter-free constant that is exactly 0 but isn't seen to be is printed as written:
  - in a finite sum's terms, Σ 1÷c^K for K = 1 to 3;
  - in a factor 1÷c pulled out of a limit, SIN(X)÷(c×X) at 0.

  Here c = SIN(1)^2+COS(1)^2-1. simplify prints 1÷c the same way.
- The least arena a case needs isn't monotone in the arena it's given: at rc6, two bench cases needed less at 16,384 B than at 12,288 B, with the same answer. Not yet traced.

Also not on 1.0's list: trigonometric identities, matrices, letters as coefficients in solve, and implied products (`2X`).
