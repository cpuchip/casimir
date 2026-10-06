# casim

Our own computer algebra system for the STU-32 calculator: symbolic mathematics (derivatives first; then
expand, collect, simplify and substitute; exact polynomial solving and integration later) on top of the
abacus core's equation engine and decimal128 number type. Portable C11, small enough to run on the
ESP32-S3 beside the firmware. Working name casim. Intended MIT; PRIVATE until Michael opens it.

## Clean room

Every algorithm here is written from the mathematics and its published description (textbooks, papers,
encyclopedia articles), or from a work unit's rules. Nothing derives from any GPL or LGPL computer
algebra system or from any calculator implementation, and no such source is on the author's disk.
Permissively licensed CAS projects serve only as black-box oracles: run and compared, never read.
No big-number library beyond the standard C toolchain and the core's own is linked.

Provenance rule: every commit that adds an algorithm names its source ("from <reference>, section N")
or its work unit and case ("unit CAS NNN, case M"), and its test exists and fails before it lands.

## Building

    make test        # export the pinned core, build it, run every test
    make size        # S3 code size of src/ (espressif/idf:v5.4.2 container)

The core (cpuchip/abacus-firmware) is consumed at the commit in `CORE_PIN`, exported read-only with
`git archive` from `CORE_REPO` (default `../abacus-firmware`), so a build never touches that clone.
Moving the pin is a commit of its own. A test ends with `check_done()`; `scripts/run-tests.sh` refuses
a test that made no checks.

Work units arrive from the abacus seat; changes the core needs go to its author.
