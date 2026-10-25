# CAS work unit 032: a letter's row that another row says

**From:** casim · **To:** abacus · **Status:** built, for h032 (Michael's (b), card cas-032-rows, abacus #10706; read
with p032c's narrow rule and h032's reading, #10639). The build goes on 1.2.0's version commit, 2a9c354. The draft's numbers come from throwaway prototypes over 31fe266
(1.2.0's src less its version text, CAS 031 as accepted). The build is p032c's code. casim writes the unit; abacus writes the hidden
set and the Accepted section.
- **From:** "a redundant row", in 1.1.2's and 1.2.0's Not yet, next on CAS 030's follow-up list (work/030-factor-row-
  to-n.md, Not in 030). Framed at #10634, the gap in p032b found from abacus's list of what h032 presses on (#10635 (1))
  and fixed in p032c (#10636), its edges measured at #10638.
- **The question:** where a sum's answer prints a letter's row `L≠0` beside another row that is false at every point
  with L = 0, can the letter's row be left out, with every other byte of the line as it was?
- **The oracle:** each moved line keeps its first part byte for byte and its kept rows in order and text; it loses
  only letter rows `L≠0`, each said by a kept row that has a value at L = 0 and is false there, at generic values of
  the other letters and at special ones (0, ±1, ±2, ½, ±π÷2, π), one letter at a time (c32/said032.py). probe030's
  judgments (missing, novalue, nothing) must not get worse anywhere. A '?' row says a row only where the line keeps
  it.

## Where it is today (31fe266, CAS 031)

| summand, K from lo to hi | 31fe266 |
|---|---|
| `COS(Z)^K`, 1 to N | `-COS(Z)÷(COS(Z)-1)+COS(Z)^(N+1)÷(COS(Z)-1); Z≠0; COS(Z)≠1` |
| `X^(-K)`, 0 to ∞ | `-1÷(1÷X-1); X≠0; ABS(X)>1` |
| `(Y÷SIN(Z))^K`, 1 to N | `-Y÷(Y-SIN(Z))+(Y÷SIN(Z))^(N+1)÷(Y÷SIN(Z)-1); Z≠0; SIN(Z)≠0; Y÷SIN(Z)≠1` |
| `(K+1)×((Z-1)×Y)^K`, -1 to ∞ | `…; Y≠0; -Y+Y×Z≠0; ABS(-Y+Y×Z)<1` |
| `K×SIN(Z)^(K-1)`, -1 to N | its 184 B line with `Z≠0; SIN(Z)≠1`, the `SIN(Z)≠0` row given way at the cap |

sm_rows probes the answer at each letter's 0 and adds `L≠0` where it has no value there (CAS 018 rule 2's cold review).
It never asks whether another row already excludes L = 0: `COS(Z)≠1` is false at Z = 0, `ABS(X)>1` at X = 0,
`SIN(Z)≠0` at Z = 0, `-Y+Y×Z≠0` at Y = 0 for every Z. The letter's row adds nothing to the set the rows exclude, and it
costs 7 B: where CAS 030's '?' row gives way at the cap, those bytes are the difference.

Only sums print it. A scan of the golden corpus and every doc row found no limit or taylor line (cd_print's rows) with
a letter row beside a row that says it.

## The change (p032c)

**A letter's row `L≠0` is left out where a kept row says it.** In sm_rput, before the rows are put (sm_said):
1. A row r says `L≠0` where r is false at every point with L = 0: r's l holds L, and every other letter of r's l is in
   a polynomial place (a sum, a product, a whole power not below 0), so that r's l at L = 0 is one constant whatever
   their values (sm_whole). That constant has a value, and fails r: equal to r's r for a `≠` row, not below it for `<`,
   not above it for `>` (sm_false0, through ib_at and const_sign). A row with no value at L = 0 says nothing (CAS 025's
   ruling).
2. A row that says it and isn't '?' drops `L≠0` from the line.
3. A '?' row (CAS 030's factor row, which gives way at the cap) says it only where the line keeps that row: the line
   with the '?' rows and without the rows they say is put into w and rewound. Where it fits, they are dropped; where it
   passes the cap, they stay, and sm_rput's give-way puts 030's line as it was.
4. The kept rows keep their order and text: only the dropped row's "; " segment goes.

h032's reading (#10639):
- **Lawful and unchanged:** a redundant row the rule doesn't reach. That includes a letter inside ABS, SIN, COS or
  EXP of another letter (`Z×ABS(Y)+1≠1`'s Z, `COS(Y×Z)≠1`), or `Z÷Y`. It is 31fe266's line and is never failed.
- **Fails:** a drop the rule doesn't allow, even where it happens to be true (`Z≠0` beside `Z×ABS(Y)+1≠1`).
- **Fails (NOT_SAID):** a row the rule drops but the build keeps, except where the line gave way at the cap.

It reverses CAS 018 rule 2's choice for EXP, made in its cold review: "EXP(X)^K gets X≠0 beside EXP(X)≠1: the same
value, the plainer row first." Where the other row excludes exactly L = 0 (EXP, COSH, 1÷COSH, `Z^2+1`), the plainer
row is the one that goes. Where it excludes more (COS at 2πk, `SIN(Z)≠0` at πk, `ABS(X)>1`), the letter's row adds
nothing. casim can't tell the two apart without the other row's zero set, which is solve's job (CAS 018's own words).
Michael chose (b) with this named as its cost (card cas-032-rows, #10706). abacus found no ruling of Michael's on 018's
sentence, and no tutor, primer or firmware file quotes a changed row (#10635).

## What it fixes

- **The redundant row** (1.2.0's Not yet): `K×COS(Z)^(K-1)` from 1 to N prints `COS(Z)≠1` alone.
- **A line within its row of the cap** (1.2.0's Not yet, the cap item's first case): the letter's 7 B make room for
  the '?' row. `K×SIN(Z)^(K-1)` from -1 to N prints `…; SIN(Z)≠0; SIN(Z)≠1` in 189 B, so Z = π is excluded at
  N = -2, where the sum is 0. The same holds for COS and TAN, and from -2.
- **Three TOO_LONG lines answer:** `K×COSH(Z)^(K-1)` from 1 to N (187 B) at 65,536 and 16,384 B, and
  `K×EXP(Z)^(K-1)` from 1 to N at 4,096 B (185 B; work/031-examples.txt:104). Their values agree with the sums
  summed directly at 21 points to 1E-55.

## Measured (p032c against 31fe266)

- **said032** at 65,536, 16,384, 8,800 and 4,096 B: 0 bad. The 640: 124 of 2,560 calls move; the family: 562 of
  10,560; the grid, SQRT(5)÷3, the polynomials, the doc rows, abacus's four and the edges: 103 of 1,139.
- **The judge's controls:** a planted build that drops every letter row: 2 of 4 BAD. p032b, which read r at canon's
  L = 0 (0÷Y is 0): 33 BAD on abacus's (1) rows, all of them refused only once the judge had special values.
- **probe030:** missing values on the 640 8 → 4, on the family 280 → 268; nothing new judged.
- **The golden corpus:** 3 lines, `EXP(X)^K` from 0 to N at three configurations, each its before less `X≠0`.
- **Doc rows:** 18 (36 checks with the series route compiled out), each judged as above.
- **sum-values** 846/0, **letter-values** 77/0.
- **Frames (host):** 429 as recorded; sm_said 128, sm_false0 80 and sm_whole 48 are new. sm_rput's frame doesn't move.

## Asked

All ruled before the build: (b) with p032c's rule (Michael, #10706); 026's factor rows fall under the same rule
(#10635); h032's reading (#10639).

## As built

The build is p032c's code in house style, with one change: sm_false0 gives its probe's status back whatever it was (no
room, or no value, is no say), as sm_fits does, where p032c gave back only UNDEFINED. No input is known where the two
differ.
- **The build against p032c:** byte for byte on the 640, the family, the grid, SQRT(5)÷3, the polynomials, the doc
  rows, abacus's four and the edges at 65,536, 16,384, 8,800 and 4,096 B (14,272 calls).
- **At the smallest arenas** (3,072 and 2,048 B), against 2a9c354: 0 bad, and no status moves either way (the 640,
  the family, the doc rows, the four and the edges, 6,656 calls).
- **Frames (host):** 432 recorded, the three new and none moved.
- **The golden corpus:** the 3 `EXP(X)^K` lines, and the same 3 under →DEC, re-recorded and listed in
  tests/golden/moved/cas-032.txt. scripts/said-golden.py (make check) puts them back and gets 2a9c354's files byte for
  byte. Each after is its before less `X≠0`, and its own judge (sum-values' reader, the generic and special points)
  finds a kept row that says it. Its controls: a moved line left at its after fails the hash; `X÷Y+1≠1` says nothing
  at X = 0, and `EXP(X)≠1` says it. The earlier boundaries (signs, conditions, ratio, left-out) put cas-032.txt's
  lines back first, as they do CAS 028's; left-out-golden.py gains the LATER list it didn't need before. Without
  cas-032.txt in it, ratio-golden.py's hash fails.
- **work/032-examples.txt:** 36 rows (the redundant rows, 030's own, room made at the cap, the give-way with the row
  back, the two sayers, and the rows that say nothing). The 18 moved doc rows are pinned again with the old line in a
  comment, and the prose beside them says what moved (work/027, 030 and 031-examples). docs/language.md: one paragraph
  and the `(Y÷SIN(Z))^K` line.
- **scripts/sum-values.py:** a CAS 032 block. 60 seeded sums (seed 20261032) whose rows are predicted by construction:
  a ratio that is 1 at Z = 0, Y over SIN or TAN, and 1+Z×G with G Y's polynomial or Y under a division, a root or a
  function. Plus 10 held, judged both ways (NOT_SAID, a drop beyond the rule) and to N by value. Its planted lines
  (1.2.0's `Z≠0; COS(Z)≠1`, and `Z≠0` dropped beside `Z÷(Y-1)+1≠1`) are refused. On 2a9c354's CLI it fails exactly
  the 33 sums predicted said. To ∞, 1+Z×G's row is `ABS(Z×G+1)<1`, which holds Y inside ABS: out of the rule's
  reach, so `Z≠0` stays there (lawful, unchanged). The first predictor had it said and the build refused it, 9 times.
- **scripts/measure.py:** 5 "said …" bench rows (1,389 cases).
- **scripts/mutants.py:** 8 new (472 in all): said-off, said-generic, said-whole-negpow, said-whole-fn, said-novalue,
  said-gt-flip, said-soft-never, said-soft-always. Pre-checked on the host (golden, doc examples, sum-values), each is
  caught. So are row030-giveway-off, row030-giveway-always and cancel-keeps-no-room, whose pinned rows moved
  (work/030-examples.txt's give-way rows, 031-examples.txt:104). The full run, with make test, is to come.

