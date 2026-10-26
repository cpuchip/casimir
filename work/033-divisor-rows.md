# CAS work unit 033: a divisor inside the ratio, to ∞

**From:** casim · **To:** abacus · **Status:** built, for h033 (Michael's card cas-wrong-033, abacus #10951: "Fix it
first (CAS 033, both parts), then 1.3.0; correct the published pages now"). The build is the prototype p033c's code, over
main 1584207 (CAS 032 as accepted at fde2bc3, with 1.2.0's release merged). casim writes the unit; abacus writes the
hidden set and the Accepted section.
- **From:** abacus's h032 baseline (#10829): Σ (Z÷Y+1)^K from 0 to ∞ printed `-Y÷Z; Z≠0; ABS(Z÷Y+1)<1`, which reads 0
  at Y = 0, where every term from K = 1 has no value, so the sum has none; no row excludes the point, since the ABS row
  has no value there (#9687 (2)). Ruled a wrong answer by the project's reading (#10860). Every release from 1.0.0 to
  1.2.0 prints it (their CLIs, rebuilt from trees whose src equals each tag's: #10953), and each published page now
  carries a dated correction (abacus #10955; the notes files at 0ed7ac7).
- **The question:** where B holds a letter under a division inside a sum, so that the closed form can cancel the
  divisor, can the answer to ∞ keep a row wherever B has no value, and only there?
- **The oracle:** at each real zero of each divisor inside B (a function's zero put in exactly: SIN(Y) → 0), the line
  must have no value, or a row with a value that is false there. Everything else byte for byte, but for rows added.

## Where it is today (1584207)

| summand, K from 0 to ∞ | 1584207 | wrong at |
|---|---|---|
| `(Z÷Y+1)^K` | `-Y÷Z; Z≠0; ABS(Z÷Y+1)<1` | Y = 0 |
| `(Z÷Y^2+1)^K` | `-Y^2÷Z; Z≠0; ABS(Z÷Y^2+1)<1` | Y = 0 |
| `(Z÷(Y-1)+1)^K` | `1÷Z-Y÷Z; Z≠0; ABS(Z÷(Y-1)+1)<1` | Y = 1 |
| `(Z÷SIN(Y)+1)^K` | `-SIN(Y)÷Z; Z≠0; ABS(Z÷SIN(Y)+1)<1` | wherever SIN(Y) = 0 |

sm_rows asks for a letter's row by probing the answer at L = 0, and to N the answer keeps B^(N+1), so the probe finds
the hole (`Y≠0` prints). To ∞ the B^(N+1) term is gone and the closed form cancels the divisor, so the answer has a
value. sm_rowf (CAS 026) gives a factor of B its row, but a divisor inside a sum isn't a factor of B.

## The change (p033c)

1. **A letter's 0:** to ∞, where the answer's probe at L = 0 finds a value, B itself is probed there too (sm_rows);
   where B has none, the row L≠0 stands.
2. **A divisor's other zeros:** to ∞, sm_rowd walks B and gives each divisor d (a power below 0 of something that holds
   a letter and isn't a letter) the row d≠0. d is the node as canon reads the summand, before the closed form and CAS
   031's cancel, so no later rewrite of the answer can move it off its zero (#10872 (2)). Never 0, no row: EXP, ALOG,
   COSH, and a positive number to a power.
3. **The answer's own hole:** as CAS 026's factor probe, d's row goes on only where the answer has a value with d at 0
   (d put to K, the sum's own letter, at 0). Where it has none, the answer shows the hole itself, and the row would
   only cost bytes. Without this probe, 4 lines of a 1,008-call family went TOO_LONG, `K×(Z÷(Y-1)+1)^K` from 1 among
   them (188 + 9 B; #10874).
4. To N nothing changes: each answer keeps the divisor, so it has no value where the sum has none.

B a product or a power isn't walked: a product's factors are sm_rowf's, and canon expands a product that holds a sum.
On a family of products and powers that hold a divisor (`(X×(Z÷D+1))^K`, `(Z÷D+1)^(2×K)`, `((Z÷D+1)÷2)^K` and three
more, 180 sums), no line is wrong, on 1584207 or the build.

**A row that excludes no point:** casim doesn't know a polynomial's real zeros here, so `(Z÷(Y^2+1)+1)^K` to ∞ prints
`Y^2+1≠0`, which is true everywhere. abacus ruled it accepted, and named under Not yet's rows for nothing (#10872):
it hides no value. `Y^2+2` and `Y^4+1` are the same.

## Measured (p033c against 1584207, then the build)

- **The aimed family** (10 divisors × 6 shapes × lo -1..1, to ∞; c32/wrong033.py): 1584207 has 44 wrong judgments, the
  build 0. To N (at N = 3): 0 and 0.
- **A wider family** (14 divisors, adding COS(Y), LN(Y), Y^3-2 and Y×Z-1; 8 shapes; 3 arenas; 1,008 calls): 214 wrong
  on 1584207, 0 on the build; 239 calls move, no status moves (no TOO_LONG, no NO_ROOM).
- **Every earlier set** (the 640, the family, the grid, SQRT(5)÷3, the polynomials, the doc rows, CAS 032's four and
  its edges; 8,992 calls at 2 to 4 arenas): 12 calls move, all the class's own (`(1+Z÷Y)^K`, `(1+Y^(-1)×Z)^K` and
  `(1+Z÷(Y-1))^K` to ∞). The golden corpus: 0 of 10,822 move, and 0 of 10,788 under →DEC. One doc row moves
  (work/032-examples.txt's `(1+Y^(-1)×Z)^K` from 0 to ∞, pinned again with its old line).
- **Frames (host):** 434 recorded; sm_rowd_ 112 and sm_rowd 8 are new, and none moved (sm_rows' probe of B sits in
  do_sum_ and sm_fits, whose frames don't move).

## Asked

All ruled before the build: both parts (Michael, #10951); `Y^2+1≠0` accepted and named (#10872); the unit doc to name
where the row costs the cap and to hold d as printed (#10872 (1), (2)).

## As built

The build is p033c's code with its comments in house style.
- **work/033-examples.txt:** 19 rows: the four and their kin, to N, the answer's own hole, and the divisors that get
  no row.
- **scripts/sum-values.py:** a CAS 033 block, 60 seeded sums (seed 20261033) and 8 held, judged at each divisor's exact
  zeros. Its planted lines (1.2.0's four) are refused and the build's taken. On 1584207's CLI (fde2bc3's src) it fails 20 of the 68.
- **scripts/measure.py:** 5 "div …" bench rows (1,394 cases).
- **scripts/mutants.py:** 5 new (477 in all): div-letter-off, div-rows-off, div-no-probe, div-never-kept and
  div-first-term. sum-4b-no-hole-row is re-anchored with the line before it, since part 1 copies its text.
- **docs/language.md:** one paragraph.
