# CAS work unit 011: Casimir as a portable library, made public

**From:** abacus · **To:** casim · **Status:** ready.
- It's Michael's decision 70 (2026-10-09, abacus docs/decisions.md): *"Let's spend a unit working on making casimir a
  portable library, as part of v1.0 and making it public"*.
- It follows CAS 010 (accepted at casim 56bc6fb with the guard at dbb6d4c, abacus
  docs/evidence/2026-10-09-cas-010-review.md).

## Why

Casimir already runs on three platforms: the S3, a Linux CLI, and the learning site's WebAssembly. Its source needs
only C11's <stdbool.h>, <stdint.h> and <string.h>, plus abn.h. It has no heap and no libm. What it lacks is the
ability to stand on its own:
- its numbers come from the firmware repo at a pin;
- its API and input language are documented for us, not for an outsider;
- it has no packaging.
This unit closes those gaps without touching the maths, then makes the repo public.

## Rules

1. **Nothing in the maths changes.** Every output of every op is byte-identical before and after this unit, on the
   host and the S3. abacus checks this with every hidden set (001–010).
2. **A standalone build.**
   - **The library is src/casim.c and src/casim.h plus the number layer:** abn.h and its Intel implementation
     (abn_intel.c), vendored into casim's tree at the firmware's pinned commit.
     - The vendored files carry the pin.
     - `make check-abn` fails unless they're byte-identical to the firmware's at CORE_PIN.
     - The firmware stays the owner of abn, and casim never edits its copy.
   - **Intel's decimal library is fetched by URL and SHA-256,** as the firmware's fetch script does, with its BSD-3
     notice.
   - **A consumer builds Casimir with no abacus-firmware checkout.** Prove it: a clean container with only casim's
     repo builds the library, the CLI and the examples.
   - **The developer tests may keep the firmware core** (coreeval's read-back is an oracle, not a dependency of the
     library). Say which tests need it.
3. **The API.**
   - casim.h is the public interface: every function, status, model and angle mode documented in the header.
   - **CASIM_VERSION** (semver) and a casim_version() call. This unit's release is 1.0.0-rc1, unless Michael names
     another.
   - Fixed-size buffers stay the contract: the caller gives the arena and the output, and nothing allocates.
4. **The input language, documented for outsiders** (docs/language.md):
   - the syntax as the calculator types it (×, ÷, π, the function names, implied products in STU);
   - the 35 and STU models (power chains);
   - the angle modes;
   - each op's statuses with an example;
   - the bounds (191 bytes on the device, degree ≤ 12 and coefficients ≤ 10^7 for polynomial ops).
   It's written from docs/canonical-form.md and the units; nothing new is decided in it.
5. **Builds:**
   - the Makefile (as today, made standalone);
   - a CMakeLists.txt that builds the library for any host;
   - an ESP-IDF component manifest, so another S3 project can depend on it;
   - **WebAssembly:** an emscripten build of the library, with a small JavaScript wrapper (`simplify(text, opts)` and
     the rest, returning `{status, out}`) and one browser example. Report its size.
   - A Python wrapper is out of this unit; propose it after if wanted.
6. **CI, public:** a workflow that runs gcc and clang at -Werror, every test, the symbol gate and `make check-abn`.
   The mutants stay a local target, since they're slow. No secrets are needed.
7. **Documentation for outsiders:**
   - **README:** what Casimir is, what it does and doesn't do, the guarantee that every answer is checked before it's
     printed and a check it can't close is a refusal, a quick start in C and in JS, and the limits.
   - **The clean-room statement:** built from documented behaviour; no CAS source code read.
   - **Licences:** MIT for Casimir, BSD-3 for Intel's library, with both notices shipped.
   - **A trademark line** wherever HP is named.
   - work/ stays as the unit history; a sentence in the README says what it is.
8. **Before the repo goes public (the scrub), a report to abacus:**
   - **The tree and the full history,** searched for:
     - secrets and tokens;
     - private hostnames, IPs and paths (a user's home path, the workspace's layout, mesh names);
     - personal data;
     - anything naming a child;
     - links to private repos.
   - Report the search commands and their results, not a summary.
   - **The hidden seeds:** confirm no hidden seed of abacus's appears anywhere in the history (work/ and the review
     links name only visible examples).
   - **The name:** search GitHub, npm and PyPI for "casimir" as a project or package name. Report what exists. If the
     name collides in a way that matters, propose alternatives; the name is Michael's call.
   - **What to do about findings:** rewrite history only if a secret or a person's data is found. Seat names and
     message numbers in commit messages are process, not secrets, and stay.
9. **The flip:** abacus reads the scrub report and confirms the moment with Michael, with the findings. Then casim runs
   `gh repo edit --visibility public` and reports the result. Nothing is published to npm or PyPI in this unit; that's
   a later call.
10. **Bounds, decision 68:** the firmware's build doesn't change. It keeps consuming casim at CASIM_PIN, and its flash
    and stack figures are re-measured by soroban at the next pin.

## Acceptance

- **Byte-identity:** abacus's hidden sets 001–010 rerun at the unit's commit, and every output is identical to dbb6d4c.
- **The standalone proof:** a clean container (casim's repo only, plus the network for Intel's tarball) builds the
  library, the CLI, the C example and the wasm wrapper. abacus repeats it.
- `make check-abn` fails on a planted one-byte change to the vendored abn.c.
- CI passes on a pushed branch.
- The scrub report as rule 8.
- The flip as rule 9.
