# CAS work unit 036: a row where a sum to N's line misses a value

**From:** casim · **To:** abacus · **Status:** built on branch cas036, for h036 (frozen, abacus #11646: sha256
60f93adb07aa004d2e8c773a2a2b3e35ed048ffece6072ffeb6e4dd3cc35b810, 293 rows, 3 arenas). The blueprint was ruled at
#11621 and committed at 60fdff7. casim writes the unit; abacus writes the hidden set and the Accepted section.
- **From:**
  - Michael's card cas-missing, option (1) (abacus #11354): a plain rewrite where one exists, else a row.
  - His card cas-missing-scope (#11473): rows for the function class too, giving way at the cap.
  - Decision 92 (#11602): Casimir's S3 text line is 270,336 B, 8 KiB over decision 80's 262,144. It is gated in
    `make size` since 104209a.
- **And:** CAS 035's named cost, which Michael asked about. To fit the old line, 336e1ba tried k = 0 only for a letter
  or its power, so `(SIN((Y-1)×X)+1)÷(SIN((Y-1)×X)+1)` at 0 prints `1; Y≠1`, though at Y = 1 it is 1 everywhere.
- **The question:** a sum to N whose line has no value at a point where the sum has one. In every case found, the
  point is the empty sum (N = lo-1), where the sum is 0. A missing value isn't a wrong answer, so its row gives way at
  the cap.

## The change

Four parts, each its own commit. The figures are a throwaway prototype's, on 104209a (1.4.0's src), and each cost is
the S3 text measured by scripts/size-s3.sh.

1. **The give-way rows** (+570 B).
   - **td_gw:** in td_sum, a sum to N whose answer shows a typed condition's hole (td_gated) gets that condition's rows
     anyway, marked to give way. A row that doesn't fit SM_ROWS or the arena is left out, and the arena is given back.
   - **smrow.gw:** the mark sits in the struct's padding, so smrow stays 12 B and no frame that holds rows moves.
   - **sm_rgw:** it puts the line with every row where that fits. Otherwise it gives sm_rput's line without the
     give-way rows, which is today's line byte for byte. CAS 030's `?` rows keep their place, and sm_said reads a
     give-way row as soft, like a `?` row.
   - **A repeat:** where a give-way row is a CAS 030 `?` row already there (`COS(Z)≠0`), sm_row's once-check drops it,
     because td_gw shows the `?` rows to it as `!` (30 B of the 570). A pairwise compare cost 400 B instead.
2. **The ratio's monomial pole** (+140 B). sm_rows leaves out L≠0 for `X^(-K)` from a lo of 1 or more (abacus #8320
   (b)). To N it becomes a give-way row through td_gw: `X^(-K)` from 1 to N prints `…; X≠0; X≠1`.
3. **A power's base with K in its exponent** (+155 B). The typed walk leaves out a part that holds K, and td_canon
   doesn't read an exponent that isn't a number. For a closed sum, td_walk now adds the base u, as kind `z`, of:
   - a divisor u^e, where e alone holds K;
   - u^K, read only from a lo below 0, where K takes negative values.

   td_sum makes a `z` condition a give-way row where the answer shows its hole, and makes nothing otherwise. So
   `X^K÷(Y-1)^K` from 1 gains `Y≠1`, and `(SIN(2×Z)×Y)^K` from -2 gains `SIN(2×Z)≠0`. abacus time-boxed this part at
   1 h, to go in if it moved no answer and cost 300 B or less (#11621).
   - A first version read any exponent's sign from lo (td_eneg) and cost +622 B. Its branch in td_sum also moved
     td_sum's S3 frame.
   - Reading u^K alone costs a quarter of that. Its reach wasn't compared with the first version's.
4. **The linear-k try** (+426 B). td_at0 is as it was before 336e1ba: for k linear in one letter, it solves k = 0.
   - **Out of line:** td_at0 is noinline and td_lin always inline. Written plainly it cost +279 B but moved two
     accepted S3 frames (td_degen 64 to 144, td_row1 144 to 176).
   - **The ruling:** in CAS 036, as its own commit (#11621).

## Rulings (abacus #11621)

- **The ratio's poles get the row (part 2), as measured.** A rewrite (`Y^(-K)` from 1 as `(Y^(-N)-1)÷(1-Y)`) is
  unmeasured. Whether it has the empty sum's value at Y = 0 turns on 0^0.
- **The empty-sum exclusion, named.** These rows exclude a point at every N, though the sum has a value there at the
  empty sum. `X^(-K)` from 1 prints `X≠0`, while at X = 0 and N = 0 the sum is 0. abacus's standing ruling allows a
  stricter row at the empty sum. The release notes say so.
- **Part 3:** in, since it moved no answer and cost 155 B.
- **`Z≠0; Z≥0`, both printed:** a give-way row beside a hard letter's row can print both (work/026-examples.txt:38).
  Both are right. The merge to `Z>0` goes under Not yet as a redundant row.
- **The give-way rows go as one group,** so where the group doesn't fit, the line is today's byte for byte, as 1.x's
  notes do. `K×Y^K÷TAN(Z)` from 1 goes under Not yet: its line is 170 B, with room for `TAN(Z)≠0` alone (182 B) but
  not with `COS(Z)≠0` too (194 B).
- **The linear-k try:** in CAS 036, with its own commit, mutant, example rows and an h036 limit block.

## Measured (the prototype, on 104209a)

**S3 text**, from 1.4.0's 262,036 B:

| Build | Text (B) | Added |
|---|---|---|
| Parts 1 and 2 | 262,746 | +710 |
| Part 4 alone | 262,462 | +426 |
| Parts 1, 2 and 4 | 263,180 | +1,144 |
| All four | 263,335 | +1,299, 7,001 under the line |

**Frames:**
- **S3:** none of the accepted frames moves. sm_rgw (48), td_gw (48) and td_at0 (96) are new.
- **Host:** the 436 recorded frames hold. CAS 035's functions, not yet in the host table, move: td_degen 128 to 112,
  and td_sum 432 to 448 with part 3.

**Reach.** The prototype's instrument (not in the repo) adds each sum term by term from the typed summand. It judges
each term with a copy of scripts/sum-values.py made before CAS 035's three fixes shipped there, with those fixes
added: a divisor 0 to 40 digits is 0, LN(0) has no value, and each worker gets the 60-digit context. It runs over seven
families, 4,366 sums to N, with each letter at 0, ±1, 2, 1÷2, π÷2 and π, and N from lo-1 to lo+2.
- **Missing values:** 1,286 at 1.4.0, 137 with parts 1 and 2, and 99 with all four.
- **Values that disagree:** 24 throughout. These are chained powers, which the evaluator reads right to left.

Sorted by the first missing value:

| Class | 1.4.0 | Parts 1 and 2 | All four |
|---|---|---|---|
| A function's domain | 969 | 1 | 1 |
| A pole outside the ratio | 94 | 38 | 8 |
| The ratio's own pole | 100 | 16 | 8 |
| At the cap | 65 | 50 | 50 |
| Left out: SGN, an opaque name to the evaluator | 32 | 32 | 32 |
| Left out: a chained power | 24 | 0 | 0 |

(The sort places 1,284 of 1.4.0's 1,286.)

**Lines moved, all four parts against 1.4.0:**
- **The 1.4.0 mutant search's inputs:** 1,620 of 35,388 sum lines move. No answer moves, and no status, except one
  line that goes from TOO_LONG to OK.
  - 1,182 gain a row.
  - 437 gain a row and lose a letter's row it implies (`ABS(Y+1)<1` for `Y≠0`; `SIN(2×Z)≠0` for `Z≠0`).
  - 1 goes from TOO_LONG to OK, where `Z>0` replaces a hard `Z≠0`.
  - Of these, 3 put `COS(u)≠0` before `TAN(u)≠0`, since part 3 changes the order the rows are made in.
- **Limit:** 23 of 162,486 lines move, all part 4's one family: `(SIN((Y+1)×X)+1)÷(…)` at each point and side goes from
  `1; Y≠-1` to `1`.
- **Unchanged:** defint and taylor, 0 of 406,524.
- **Golden corpus:** 3 of 10,822 lines move: `X^(-K)` from 1 gains `X≠0`, at three sizes.
- **Documented examples:** 43 commands (86 checks) gain rows, in work/018, 026, 027, 029, 030, 032, 033 and
  034-examples. The build re-pins them.

## Not yet (for the release notes)

- **The empty-sum exclusion** (above): a row stricter than the sum at N = lo-1.
- **`Z≠0; Z≥0`:** both are printed where `Z>0` says them together.
- **The give-way rows go as one group:** `K×Y^K÷TAN(Z)` from 1 to N has room for `TAN(Z)≠0` alone.
- **The missing values that stay** (99 in the families):
  - **59 on lines of 150 B or more,** near the cap. The sort puts 50 of them at the cap, where the rows give way, as
    ruled.
  - **6 whose exponent is linear in K but isn't K:** `SIN(2×Z)^(2×K-1)×Y^K` and `SIN(2×Z)^(3×K-1)×Y^K`, each from -2,
    -1 and 0, at Z = π÷2.
  - **3 where the hole is the closed form's own:** a positive power of a letter in the ratio, from a negative lo
    (`X^(K+2)×Y^(-2×K)` from -2 and -1, `X^(K+1)×Y^(-K)` from -1, at X = 0). The typed summand has no condition
    there.
  - **31 short lines with SGN,** which the evaluator can't judge.
- **A function's give-way row replaces the simpler letter's row it implies,** where the line fits: `ACOS(Z+1)≠0` and
  `ABS(Z+1)≤1` in place of `Z≠0`.
- **limit's k in two letters, or a letter's square** (abacus #11646, from h036's baseline): part 4 solves k = 0 only
  for k linear in one letter, so `(SIN((Y-Z)×X)+1)÷(SIN((Y-Z)×X)+1)` at 0 prints `1; Y-Z≠0`, though at Y = Z it is
  1 everywhere; `Y×Z-1` and `Y^2-1` (`1; Y≠-1; Y≠1`) the same. Each row is never wrong, only stricter than needed.

## Built (branch cas036)

**Commits:**
- a4cfed8, 4fc2082, 127eb82 and 1175caa: parts 1 to 4, each the prototype's source with a comment for it. Each
  compiles alone under `-Wall -Wextra -Wpedantic -Werror`.
- f126e26: the 43 documented rows re-pinned. work/030-examples.txt had two comments that named these missing values
  as kept, and they now say what CAS 036 prints.
- a34164d: the golden boundary, with 3 corpus lines and 3 under →DEC. scripts/missing-golden.py checks it; the six
  earlier boundaries put CAS 036's moves back first (LATER); and typed-golden.py gains the list. Falsified: without
  the LATER entry typed-golden.py fails both hashes, and a moved line left at its after or a row naming no missing
  value (`X≠2`) fails missing-golden.py.
- 35dbfae: work/036-examples.txt (24 rows) and ten mutants. Each mutant was built and recorded against the branch
  before its catcher was chosen.
- fd58264: CAS 032's said-soft-never retargeted. Part 1 changed the line it anchored, and it keeps its meaning (no
  row is soft).
- c301023: scripts/sum-values.py's typed_row. The CAS 024 to 027 value judges read the ratio's row alone and failed 35
  sums on a give-way domain row (`Z>0` for LN(Z), `Z≥1`, `ABS(Z)≤1`, `COS(Z)≠0`). They now pass that row to
  judge_typed, as they do CAS 035's root rows, and only for the function and letter the summand holds.
- bc1dacf: docs/language.md's sum and limit paragraphs.

**What differs from the blueprint:**
- **Mutants:** ten, not eight. They add part 3's u^K rule off and part 4 solved with b's sign.
  - `td036-z-any-lo` (u^K read from any lo) changes none of the 35,388 searched sum lines. It changes 39 of the
    families' 10,271, each from TOO_LONG to NO_ROOM: rows that can't be made still take the arena. Its catcher pins
    one of them.
- **sm_said's soft read of a give-way row is kept, though it changes no line.** sm_rgw's first try keeps every row or
  none, and its fallback runs sm_said with no give-way row. Removing the read gave the same 35,388 searched lines,
  and the S3 text grew 20 B, so it stays, with no mutant (that mutant would be equivalent).

**Gates on the branch:**
- make check: green (198 s).
- make test: green, 20 tests, 118,449 checks (418 s).
- `make size`: 263,335 of 270,336 B.
- Frames: no accepted S3 frame moves, and sm_rgw (48), td_gw (48) and td_at0 (96) are new. On the host the 436
  recorded frames hold.
- Families against 1.4.0: 1,245 of 10,271 sums move, by rows only, with no status or answer moved.
- Mutants (claim #11660, 07:34:03Z to 07:41:29Z, three shards): all ten td036 caught.
  - td036-gw-off and td036-pole-off: by the golden corpus's `X^(-K)` lines.
  - td036-gw-hard and td036-gw-before-q: by re-pinned rows where the give-way rows go (work/030, 032 and 034), and by
    026's and 027's.
  - td036-gw-repeat: by work/035-examples.txt:143 and work/036-examples.txt:26.
  - td036-z-div-off and td036-z-pow-off: by 029's, 033's and 036's rows.
  - td036-z-any-lo: by work/036-examples.txt:48.
  - td036-lin-off and td036-lin-sign: by work/036-examples.txt:55 to 57.

  The full 529, at the tag's tree, wait for the release.
- Device (the same claim, 07:41:29Z to 08:00:12Z): DEVICE PASS, 1,412 cases at 16,384 B, each the same as the host.
  - The worst stack read masked is 11,360 B, as at 1.4.0. The interrupt allowance read 400 B this run, against 384
    at 1.4.0's, so the budget is 11,760 of 12,288, leaving 528 (floor 256).
  - casim-device's memory.peak: 1,077,186,560 B.

## The build, as the blueprint planned it

**Commits:**
1. smrow.gw, td_gw and sm_rgw.
2. The monomial pole.
3. Part 3, the `z` rows.
4. The linear-k try.

**Examples:** one block of new rows in work/036-examples.txt, part 4's among them, and the 43 documented commands
re-pinned. Part 4 moves no documented row, so its rows are new.

**Golden:** the 3 lines move, under a boundary like CAS 035's.

**Mutants:**
- td_gw makes no row;
- the give-way rows are hard;
- sm_rgw keeps CAS 036's rows before CAS 030's `?` rows;
- the repeat of a `?` row is kept;
- the monomial pole's row is off;
- part 3's divisor rule is off;
- part 3's u^K rule is read from any lo;
- the linear k is untried.

**Gates:**
- make check and make test;
- `make size` under 270,336;
- the frame tables (S3: sm_rgw, td_gw and td_at0 new);
- the device bench;
- the full mutant run at the tag's tree.
