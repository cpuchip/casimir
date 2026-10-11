# The stack gate

One definition of how much stack a call is allowed, for two benches on the ESP32-S3: casim's (scripts/device.sh,
device/main/bench_main.c) and the calculator firmware's (calc_task). Each bench reads this file; neither keeps its
own copy of the rule. casim's half by casim, the firmware's by soroban; abacus ruled the definition (#6850, #6854).

## Why the watermark alone is not the figure

A FreeRTOS task stack starts filled with a known byte, and the high-water mark is the deepest byte that is no
longer that fill. That counts the deepest **written** byte, not the deepest stack pointer. A function's `entry a1, N`
reserves N bytes, and whatever part of them the call never writes stays fill. With interrupts masked, nothing else
writes below the stack pointer, so the masked reading can sit above the real stack pointer.

An interrupt does write there. It saves one frame, 192 bytes (IDF v5.4.2's XT_STK_FRMSZ), just below the stack
pointer it interrupted. So an unmasked reading can exceed the masked one by 192 plus however far the stack pointer
was below the masked watermark.

Measured on casim's bench (2026-10-11), on I0030's steps page over 1,500 delays:
- 33 unmasked readings went past the masked one;
- each held exactly one exception frame at its lowest written word, and all 33 implied the same stack top;
- each excess equalled 192 + (the interrupted stack pointer's depth, from the frame's saved A1, − the masked depth);
- the largest, 336, is 192 + 144: the interrupt came inside libbid's bid128_ext_fma, whose 512-byte frame the call
  writes only partly.

Levels 1 to 3 cannot stack a second frame on the task stack: EXCM is held until the handler has moved to the
interrupt stack, and a nested interrupt after that lands there. The handler's window spill, before the move, writes
inside the frame: XT_STK_FRMSZ includes the interrupted function's base save area. So it stays one frame. The level-4
IPC handler uses a static save area (soroban #6236). Levels 5 to 7 (debug, NMI) were not checked; they run in masked
readings too, so they don't count in R. (IDF v5.4.2: xtensa_vectors.S, portasm.S, xtensa_context.h.)

## The figure

For each row (one call, or one page of a call's steps):

- **M**, the masked depth: bytes from the stack's top to the deepest written byte, with interrupts masked (levels 1
  to 3, portSET_INTERRUPT_MASK_FROM_ISR) around the call, in a fresh task.
- **S**, the deepest stack pointer: the largest depth the stack pointer reaches during the call (below).
- **R**, the unwritten room: S − M when S is deeper than M, else 0.
- **I**, one interrupt frame: 192.

The row needs **M + R + I** bytes. The gate takes the worst row:

    worst over rows of (M + R + I) <= the budget - the floor

For casim the budget is 12,288 bytes (CAS 002b) and the floor 256 (abacus #6224). For calc_task the budget is its
stack, 32,768 bytes (device/reva/main/reva_main.c), and the floor is not ruled yet: until abacus rules one, the
firmware reports budget − worst (M + R + I) per unit (decision 68) and fails on nothing but the sweep check below.

When the worst row leaves less than the floor, the depth guard drops until it fits. The budget and the floor don't
move.

## Measuring S

**On a bench-only build,** compiled with -finstrument-functions over every object the call reaches (casim, abn and
libbid, or the core):
- __cyg_profile_func_enter records the smallest A1 it sees and its caller's PC;
- on Xtensa the stack pointer is fixed after `entry` (no alloca or movsp on the path), so the smallest A1 seen at
  entries is the call's deepest stack pointer;
- one masked run per row gives S, and the owner of the deepest frame, for every row.

**Its control:** instrumenting may change frames.
- Every function's `entry a1, N` in the min-sp image is compared with the product image's.
- A row's S counts only when every function on its path has the same N in both images, and none uses movsp.
- The rows that don't qualify are listed.

**Its positive control:** I0030's steps page must report bid128_ext_fma, 144 bytes below the masked watermark.

**What it can't see:** a callee built without the flag enters no hook. That covers ROM functions (the S3's ROM
memcpy and memset, parts of newlib), prebuilt IDF objects, and anything else not instrumented. If such a callee is
the deepest frame on a row, its frame lies below the smallest A1 seen, and S under-reports. For each row's deepest
instrumented function, list the uninstrumented callees it can reach, from the ELF's call graph or a disassembly of
its calls. A row whose deepest function reaches one gets the watchpoint, or adds that callee's entry N to S.

**Measured on casim's image (2026-10-11): the control fails broadly.** Of 345 functions in casim, abn and libbid, 97
change their `entry a1, N` under -finstrument-functions, in both directions (add 224 to 144, abn_div 192 to 176,
casim_integrate 48 to 64): the hooks move gcc's inlining. One function, libbid's short_sqrt128, uses movsp. So for
casim, S from this build doesn't stand for the product on most rows. casim's S is measured with the level-5 sampler
instead (below).

## The checks on the figure

1. **The sweep, on the product image.**
   - Each row in the sweep's list is run unmasked at a spread of delays.
   - The deepest interrupt frame seen (the lowest written word: EXIT, PC, PS, A0, A1) gives A1's depth + 192.
   - That must not exceed the row's M + R + I. If it does, R missed something (two unwritten frames, say) and the gate
     fails.
2. **The watchpoint, on the product image, on the worst few rows.**
   - A data watchpoint (DBREAK) on the masked watermark's address, then the row run again.
   - The first write there stops with the writer's PC and A1.
   - Owner and depth must agree with the min-sp build. Where they do, the min-sp build is trusted for the other rows.

## Who runs what

- casim (scripts/device.sh, device/main/bench_main.c):
  - **Today's figure (interim):** the worst masked row + max(384, the sweep's largest excess), against 12,288 with the
    256 floor. It is a bound only if no row's unwritten room exceeds 192. On I0030's page it is 144 (measured).
  - **S, measured 2026-10-11 with the level-5 sampler** (soroban's, abacus-firmware fcfafdf; on a scratch copy of
    casim's bench until it is cleared for this repository). The timer and the worker are pinned to one core. Each
    worker sets the window to its own fresh stack, so M and S come from one masked run on one image. Each row runs at
    three periods (337, 480 and 613 cycles).
    - Controls: FULL R = 32; ROOM R = 5,872 (predicted at least 5,900: the part written at the top was larger than
      allowed for); I0030's page R = 144 in bid128_ext_fma, as the raw capture found.
    - Over 1,058 rows (861 cases and their last pages): the worst M + R + I is 11,520, at STU chain X^ 44 d with R = 32,
      leaving 768 of the 12,288 (floor 256).
    - R exceeds 192 on 99 rows, up to 400, all shallow (M about 1,936). The owner there is casim's fn, whose whole
      frame is 128: several unwritten frames sit below the watermark at once. So the interim "masked + 384" is not a
      bound row by row; it holds for the gate only because those rows are far from the worst.
    - S is still a sample, three runs per row.
  - **The sweep:** ten rows and the worst, at 27 delays, as now.
  - **The watchpoint:** on the worst rows, next.
- The firmware (soroban, scripts/resources.py board, Rev A head-less):
  - **Rows:** two shapes × three states. The shapes are the 153-unit mix at unit 047's deepest site, and 19 levels of
    SIN( in RAD. The states are factory, one equation, and a full image (66 equations, A–Z, statistics). The keys are
    typed through the console; the reading is the console's X line (uxTaskGetStackHighWaterMark, bytes never
    written), so M = 32,768 − that reading.
  - **M:** on the AB_MASK bench image (MASK ON masks levels 1 to 3 on the calc task's core around each key and run;
    package.sh refuses the image).
  - **S:** an AB_MINSP bench image, built the same way: -finstrument-functions over core/, firmware/, Casimir and
    libbid, its console reporting the smallest A1 and its owner's PC. It has the entry-N control against the product
    ELF, and package.sh refuses its banner as it refuses AB_MASK's. (Being built, after decision 84's rewrite.)
  - **The sweep:** each row unmasked on the product image. Today each exact script gives one timing, so a spread of
    delays before the keys is added for the sweep rows.
  - **The watchpoint:** on the deepest row.
  - **Report:** budget − worst (M + R + I) per unit, with M, R and the owner per row. Until S lands, the figure is
    the masked one less 384, footnoted "a bound only if unwritten room ≤ 192" (abacus #6853).
