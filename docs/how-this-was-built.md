# How this was built

Michael Stufflebeam owns Casimir, named it, and rules on it. The code was written by a team of AI agents, each a
Claude Code session with its own charter, working with him. The commit messages, work units and comments name these
agents (the "seats") and the messages between them. They stay in the history on purpose, so anyone can see how the
work was done and try it themselves.

## The seats

- **casim** is Casimir's clean-room author: the library (`src/`), its tests, its measurements on the chip, and the
  prototypes and proposals each unit starts from.
- **abacus** writes the work units (`work/NNN-*.md`), rules on every departure from them, and accepts or returns each
  unit. It runs the hidden test sets: random inputs judged against other systems used only as black boxes, at high
  precision. Only verdicts and counts cross to casim, never another system's output or source.
- **soroban** writes the calculator firmware that pins Casimir as a component, and shares the stack-budget rule with
  casim (`docs/stack-gate.md`).
- **basecamp** coordinates the seats and relays Michael's rulings.

## How a unit runs

1. **Prototype.** casim builds the smallest throwaway thing that answers the unit's riskiest unknown, measured on the
   host and on the ESP32-S3: frames, flash, arena, time. It is deleted afterwards; only the numbers are kept.
2. **Proposal.** casim sends what it measured and lists every departure from what was asked, as questions.
3. **The unit.** abacus writes the work unit, with its rules, bounds and acceptance. When a rule doesn't match the
   measurements, casim says so before the build, not after, and abacus rules on it.
4. **Build.** Tests are written red first. Every answer is read back through an independent evaluator. Deliberate
   faults (scripts/mutants.py) must each be caught. No existing function's stack frame may move (the frame table in
   `make check`). The S3 bench must be byte-identical to the host.
5. **Report.** Every resource is reported (decision 68): flash, arena, stack, time, the mutants, and the golden lines
   that moved on purpose.
6. **Acceptance.** abacus runs the hidden sets, reruns every earlier unit's, and accepts the unit or returns it with
   the cases that failed.

## The numbers

A `#NNNN` in a commit, a unit or a comment (for example "abacus #6927") is a message in our internal message log: the
request, ruling or review that a change answers. The log itself is not public. The number shows which conversation a
line came from and in what order.

Decisions are numbered too ("decision 80"): those are Michael's rulings, recorded by abacus.

## The rules the work kept

- **Clean room:** casim never reads the source of another computer algebra system or calculator. Other systems are
  used only as black boxes, by abacus, and only their verdicts reach casim.
- **No copyleft dependencies:** Casimir is MIT. Its only third-party code is Intel's Decimal Floating-Point Math
  Library (BSD-3-Clause, its notice in third_party/intel-dfp/LICENSE), fetched by URL and checksum at build.
- **A check that has never been seen to fail is not evidence:** each gate and judge is tested against planted faults.
- **Report what was measured:** when a measurement contradicts an earlier record, the tension is reported rather than
  the record silently overwritten (`docs/stack-gate.md` is one such case).
