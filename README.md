# Casimir

Exact computer algebra in portable C11: derivatives, simplification, expansion, solving, integrals, factoring and
partial fractions, from text to text. It was written for a calculator, the STU-32 (an ESP32-S3), and runs anywhere C
does: as a C library, as an ESP-IDF component, and in the browser through WebAssembly. Version 0.9.0.

```
$ build/casim derive "X^2×SIN(X)" X RAD
2×X×SIN(X)+X^2×COS(X)
$ build/casim solve "X^3-3×X+2" X RAD
SOLVED: X=-2; X=1 (MULT 2)
$ build/casim defint "SIN(X)" X 0 π RAD
2
```

Named by Michael Stufflebeam: "I like casimir related to kasimir: proclaimer of peace".

## What it does

| Operation | |
|---|---|
| derive | the derivative of an expression or an equation |
| simplify | one canonical text for each expression, with exact numbers and exact values (SIN(π÷6) is 0.5) |
| expand, collect, subst | distribute and multiply out; gather the powers of a variable; replace a variable |
| solve | the real roots of a polynomial equation of degree ≤ 12, exactly (X=(1+SQRT(13))÷2) |
| integrate, defint | an antiderivative from a table of forms, of a rational function through its partial fractions, and by substitution and by parts (`X×EXP(X)` is `-EXP(X)+X×EXP(X)`); a definite integral, exactly |
| factor | a polynomial over the rationals, each factor proven irreducible |
| pdiv, pgcd, cancel | polynomial division, greatest common divisor, and a quotient reduced by it |
| partfrac | partial fractions over the rationals (`(X^4+1)÷(X^3+1)` is `X+2÷(3×(X+1))-(2×X-1)÷(3×(X^2-X+1))`) |
| taylor | a Taylor polynomial to order 12 about a point, exact (`LN(X)` about 1 to 3 is `(X-1)-0.5×(X-1)^2+(X-1)^3÷3`) |
| limit | a limit at a point from either side or both, or at ∞ and -∞, exact (`(1+3÷X)^X` at ∞ is `EXP(3)`) |
| steps | the working of derive, integrate (a rational function through its partial fractions too), solve and partfrac, a step a page, as a student walks it, each ending on the op's answer (`casim_step`) |

docs/language.md is the input language, with every operation's statuses and examples.
docs/how-this-was-built.md says who built Casimir, a team of AI agents with Michael, and how each unit ran.

**What it doesn't do (yet):** complex roots, infinite series and sums, trigonometric identities (so not SIN(X)^2's
integral), matrices. The input uses the calculator's notation (`×`, `÷`, `π`,
upper-case names) and doesn't read implied products (`2X`).

## The guarantee

Casimir prints an exact answer or a status that says why it can't, never an approximation dressed as an answer.

- **The operations that search for an answer check it before printing it, and refuse when the check can't close.**
  integrate differentiates its answer and compares it with the integrand exactly, or, for a substitution or a part,
  proves it from pieces each checked that way, by the chain and product rules. solve divides out every root it
  prints. factor proves each factor irreducible and reads the product back as the input. pdiv checks f = q×g + r.
  pgcd and cancel accept a gcd only when it divides both polynomials exactly. partfrac multiplies its terms back
  over the denominator and requires the numerator exactly. defint uses integrate's checked answer.
- **The rewriting operations** (derive, simplify, expand, collect, subst) apply only rules that keep the value
  wherever the input is defined. Tests show they do (below).
- **Every limit is a status:** too long for the output (TOO_LONG), too deep or too big for the memory given
  (NO_ROOM), a factor it can't prove irreducible (PARTIAL, with the correct factorization so far).

## Quick start in C

```sh
make                  # build/libcasim.a, build/casim, build/hello; fetches Intel's decimal library once
make check            # the standalone checks
```

```c
#include "casim.h"

static unsigned char arena[8192];       /* all of Casimir's working memory, given by the caller */

int derivative(char out[192])
{
    /* {op, text, model, var (0-25 for A-Z), repl, repl_model, angle, repl2} */
    casim_req rq = {CASIM_DERIVE, "X^2×SIN(X)", CASIM_MODEL_35, 'X' - 'A', NULL, 0, CASIM_RAD, NULL};
    return casim_run(&rq, out, 192, arena, sizeof arena) == CASIM_OK;  /* out: "2×X×SIN(X)+X^2×COS(X)" */
}
```

```sh
cc -std=c11 -Isrc examples/hello.c build/libcasim.a -lm
```

With CMake: `add_subdirectory(casimir)` and `target_link_libraries(app PRIVATE casim)`, or build it alone:
`cmake -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake`.

In an ESP-IDF project, depend on it from `main/idf_component.yml` (idf_component.yml says how). On the ESP32-S3 the
library builds with ESP-IDF 5.4. It takes about 2.1 MB of flash (328,544 bytes of code, 1,765,456 of read-only data,
most of it the decimal library's tables) and 32,512 bytes of RAM for initialised data (the S3 bench's build).

src/casim.h is the whole interface. Nothing allocates: the caller gives the output buffer and an arena (8 KiB is the
calculator's budget), and casim keeps no global state.

## Quick start in JavaScript

```sh
scripts/wasm.sh       # build/wasm/casim.mjs, casim.js, casim.wasm (emscripten; docker if emcc isn't installed)
```

```js
import { loadCasimir } from "./casim.mjs";
const cas = await loadCasimir();
cas.derive("X^2×SIN(X)");                    // {status: "OK", out: "2×X×SIN(X)+X^2×COS(X)"}
cas.solve("X^2-2=0");                        // {status: "OK", out: "SOLVED: X=-SQRT(2); X=SQRT(2)"}
cas.defint("1÷X", "-1", "1");                // {status: "IMPROPER", out: ""}
```

wasm/casim.mjs documents the options (var, angle, model, arena, cap). examples/web/index.html is a page that uses it.
Serve the repository's root (`python3 -m http.server`) and open /examples/web/. casim.wasm is 2.08 MB (1.84 MB
gzipped), most of it the decimal library's tables.

## Limits

- An equation slot on the calculator is 191 bytes. Every operation but derive and simplify stops at 191 bytes with
  TOO_LONG.
- Polynomials have degree ≤ 12 and coefficients ≤ 10^7. Numbers are exact rationals below 2^62.
- Nesting deeper than 44 levels is NO_ROOM, which bounds the stack: at most 11,328 bytes on the ESP32-S3, plus 384
  for interrupts (two 192-byte frames; the bench has measured up to 272 bytes over the masked reading). Size a task
  for 12 KB.
- Built and tested with gcc and clang on Linux, emscripten, and ESP-IDF 5.4 on the ESP32-S3. macOS and MSVC are
  untested. Intel's library needs a different configuration on Windows.

## How it's tested

- **The developer tests** (`make test`; 19 programs, about 118,000 checks) read every answer back through an
  independent evaluator and compare values. That evaluator is the calculator firmware's own core
  (cpuchip/stu32-firmware), so these tests need its clone (CORE_REPO, default ../stu32-firmware) at the commit in
  CORE_PIN. They are collect, core_link, defint, derive, exact, examples, expand, factor, fuzz, integrate, opaque,
  pairs, partfrac, poly, simplify, solve, stamp, subst and syntax
  (tests/*_test.c). CI runs them too, on the firmware fetched by its sha at exactly CORE_PIN (scripts/ci-test.sh).
- **The standalone checks** (`make check`, and CI) need only this repository:
  - the golden test, 10,463 calls whose exact answers were recorded from earlier sources, so a change shows up;
  - the frame and symbol gates, and the table of every function's frame against the one recorded for its compiler,
    where one is;
  - a gcd check on small primes, partfrac's rule-7 check reached by spoiling a numerator, and the rational integrals'
    check reached by spoiling a term;
  - check-abn;
  - the examples in docs/language.md, CAS 013's walks in work/013-steps-examples.txt, and CAS 016's rows in
    work/016-examples.txt.
  - Taylor polynomials judged exactly against power series over fractions, the opposite method, in the calculator's
    arena and in 65,536 bytes; and ABS's corners, X^j×ABS(X)^m, by their truth (scripts/taylor-values.py).
  - limits judged against truths built from those power series (a quotient's limit from its first nonzero terms, at
    a point, at ∞ and at -∞), not from derivatives, read back to 45 digits (scripts/limit-values.py).
  - integrals by substitution and by parts judged by differentiating the answer numerically at 90 digits against the
    integrand, at seeded points in every angle mode, with integrands that have no elementary antiderivative refused
    (scripts/integrate-values.py).

  The golden test is regression only: it shows nothing moved, not that an answer is right. What makes an answer right
  is the library's own checks above and the developer tests.
- **Mutants** (scripts/mutants.py, run locally because it is slow): 234 deliberate faults, each of which the tests
  must catch; `make test` catches all 234 (at CAS 017's steps). Each mutant's copy re-records its frame table first,
  so a fault that only moves a frame isn't counted as caught; the one fault whose catch is the frame table itself (the
  rational-integral hook moved into do_integrate) is named in the script. When there were 80, the standalone checks
  first caught 64. The other 16 changed answers only where the developer tests looked, so their inputs that expose
  each fault were added to the golden corpus, 34 lines found by scripts/golden-harvest.py, and the standalone checks
  then caught 80 of 80. Those lines were chosen because they catch those 16 faults, so a fault unlike them can still
  need the developer tests.
- **Acceptance.** Each unit of work was accepted against hidden test sets that a separate reviewer ran, comparing
  against other systems as black boxes. Every unit's sets are rerun at each acceptance.
- **The S3 bench** (scripts/device.sh) runs 1,065 cases on the chip, through the ESP-IDF component, and requires every
  answer to be byte-identical to the host's.

## How it was made

Casimir was written by an AI coding agent (Claude) in units of work set and reviewed by a second agent, under Michael
Stufflebeam's direction. work/ holds those units, CAS 001 onward: each rule's reason and its visible examples, as the
history of the design. They name abacus (the reviewer's units, decisions and hidden test sets), which is private, so
its paths won't open for you. They also name abacus-firmware, the calculator's firmware: it is now public as
cpuchip/stu32-firmware, its history rewritten once, so a firmware commit id in work/ is an old one (its
docs/commit-map.txt maps each to the new). docs/canonical-form.md is the output form's contract.

**Clean room.** Every algorithm is written from the mathematics and its published description (textbooks, papers,
encyclopedia articles), or from a unit's rules. No source code of any computer algebra system or calculator
implementation was read. Other CAS programs served only as black boxes in the reviewer's tests: run and compared,
never read. Casimir links no big-number library. Its numbers are exact rationals in 64-bit integers. It reads
decimal literals, and evaluates a constant where a sign or a pole's place is needed, in decimal128 through the number
layer below. solve and factor find candidate roots in binary floating point and keep only what divides exactly.

## The number layer and Intel's library

- third_party/abn is the number interface Casimir is written against: IEEE 754 decimal128 through Intel's library.
  It's vendored unedited from the calculator firmware, which owns it. `make check-abn` verifies it against the SHA-256s
  in third_party/abn/PIN (and, with a firmware clone, byte for byte against the firmware).
- Intel's Decimal Floating-Point Math Library is fetched from Intel's distribution at netlib, by URL and SHA-256
  (scripts/fetch-intel-dfp.sh; cmake/intel-dfp.cmake). It isn't committed here.

## Licences

Casimir, including third_party/abn: MIT (LICENSE). Intel's Decimal Floating-Point Math Library: BSD-3-Clause, its
notice in third_party/intel-dfp/LICENSE, which every distribution of a build must carry.
