# Casimir

Our own computer algebra system for the STU-32 calculator: symbolic mathematics on top of the abacus
core's equation engine and number type. Derivatives (unit CAS 001) and simplification to a canonical form
(CAS 002, docs/canonical-form.md) so far; expand, substitute, exact polynomial solving and integration
later. Portable C11, small enough to run on the ESP32-S3 beside the firmware. Named by Michael ("I like
casimir related to kasimir: proclaimer of peace"); the repository and the seat that writes it are casim.
Intended MIT; PRIVATE until Michael opens it.

## Clean room

Every algorithm here is written from the mathematics and its published description (textbooks, papers,
encyclopedia articles), or from a work unit's rules. Nothing derives from any GPL or LGPL computer
algebra system or from any calculator implementation, and no such source is on the author's disk.
Permissively licensed CAS projects serve only as black-box oracles: run and compared, never read.
No big-number library beyond the standard C toolchain and the core's own is linked.

Provenance rule: every commit that adds an algorithm names its source ("from <reference>, section N")
or its work unit and case ("unit CAS NNN, case M"), and its test exists and fails before it lands.

## Building

    make test                 # export the pinned core, build it, run every test
    make size                 # S3 code size of src/ (espressif/idf:v5.4.2 container)
    scripts/mutants.py        # deliberate faults the tests must catch
    scripts/measure.py        # arena and length, host (8-byte nodes: the same on the S3)
    scripts/device.sh         # on the S3: byte-identical to the host, time, arena, stack
    build/casim simplify "<text>" RAD
    build/casim derive "<text>" X RAD

The core (cpuchip/abacus-firmware) is consumed at the commit in `CORE_PIN`, exported read-only with
`git archive` from `CORE_REPO` (default `../abacus-firmware`), so a build never touches that clone.
Moving the pin is a commit of its own. A test ends with `check_done()`; `scripts/run-tests.sh` refuses
a test that made no checks.

Work units arrive from the abacus seat; changes the core needs go to its author.
