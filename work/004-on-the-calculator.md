# CAS work unit 004: Casimir on the calculator

**From:** abacus · **To:** casim, with soroban (the firmware side) · **Status:** ready (Michael, 2026-10-07, on the
roadmap: "Casimir on the calculator next"; abacus decision 60). It follows CAS 003, accepted at 5a1be66.

## Why

Casimir works today on the host: derive, simplify, expand, collect and subst, each judged against hidden oracles.
This unit puts those five on the device, in STU mode, on the equations a student has typed. Exact polynomial solving
and integrals stay later units.

## Who does what

- **casim** keeps src/ portable and self-contained, and delivers what the firmware needs (rules 1-3, 6).
- **soroban** brings Casimir into abacus-firmware through the owner's gate (decision 59), exactly as Babal came in:
  - a CASIM_PIN file;
  - scripts/fetch-casim.sh, which exports src/ read-only with git archive from a local clone;
  - the gate's checks, then the keys, the menu and the screen (rules 4-5).
- Split the commits so each side's change is its own.

## Rules

1. **Equations keep their meaning.** An equation's stamp (unit 031) decides its grouping.
   - STU: powers right to left, and implied × already written into the stored text.
   - 33s and 35s: powers left to right.
   - Casimir must parse a stored equation as its stamp says. Today casim's core pin (f839cb9) predates 031.
   - **Repin first** to a core with 031's stamps (the current head), and take the stamp as an input. Propose how:
     an argument, or a parse entry that reads the stamp.
   - **A test:** 2^3^2 stamped STU differentiates and simplifies as 2^(3^2), and stamped 35s as (2^3)^2.
2. **Results are printed for STU,** whose list they go into. Where STU's right-to-left powers would read a printed
   power differently from what Casimir meant, parentheses make it unambiguous. Re-parsing a result under STU's
   stamp must give back the same tree (a test over every hidden case).
3. **The device budget:**
   - **flash:** at most 64 KB in all (ruled at CAS 003);
   - **arena:** 8 KB;
   - **stack:** 12 KB at worst, through the depth guard;
   - **frames:** none larger than 1 KB, under -Wframe-larger-than=1024 for src/, as arcade's stage 2 now does.
     Any frame above it is named and justified.
   - soroban places the arena (static, or in PSRAM) and reports the internal RAM change (the nm diff, as for
     Babal).
4. **The keys, STU only** (soroban proposes the layout).
   - A CAS menu reachable from an equation in the list (EQN), with D/DX, SIMP, EXPND, COLCT and SUBST. Propose its
     key, and whether it is a soft-key page of EQN or its own menu. In 33s and 35s modes the menu doesn't exist (as
     GRAPH, decision 58's rule).
   - D/DX and COLCT prompt for the variable, as SOLVE's prompt does.
   - SUBST prompts for the variable, then takes the replacement typed in the equation editor.
   - **The result:**
     - becomes a new equation, inserted after the one it came from (the original is kept);
     - is shown, ready for TABLE, GRAPH, SOLVE or another CAS step;
     - UNDO removes it.
5. **Messages:** each casim status maps to one short message. Propose the texts.
   - TOO_LONG: the result is over 191 bytes.
   - Unsupported: for example a function derive refuses.
   - Arena or depth: the guard's refusal.
   - The equation and the stack are untouched on any refusal.
6. **Time on the S3:**
   - Measure each operation's worst case over abacus's hidden sets on the device.
   - If any exceeds 1 s, it must be interruptible by C, as SOLVE is (040b's computing mark).
   - Report the times.

## Acceptance

- **Byte-identical:** abacus's CAS 001, 002 and 003 hidden sets (seeds kept) are run as key vectors through the
  calculator (tools/refmodel/cas_to_vectors.py), on the host build and on the S3. Every result is byte-identical
  to casim's host CLI at the same pin. TOO_LONG and the refusals are identical too.
- **The stamp tests (rule 1) and the round trip (rule 2).**
- **app_test:** the menu, each prompt, the inserted equation, UNDO, a refusal leaving everything as it was, and the
  menu's absence in 33s and 35s modes.
- **Mutants:**
  - the stamp ignored (STU's 2^3^2 read left to right);
  - the result replacing the original;
  - the menu reachable in 35s mode;
  - TOO_LONG truncated instead of refused.
- **The S3:**
  - flash, arena, stack and the internal RAM diff;
  - each operation's worst time;
  - the free heap after boot, against Babal's 57,099.
- **tutor:** an STU lesson using D/DX can follow, as tutor's call.
