# Show steps, 2: integrate (abacus decision 71; design #6227, approved #6228)

The second op whose working casim_step pages (work/steps-derive.md is the first). Integrate's shape is not
derive's tree: on the text as typed, it splits a sum into terms, takes each term's constant factors out, and
integrates the one factor left by a table form (with a linear substitution) or a polynomial term by term, each term
checked by differentiating its answer back.

## The steps

- **SUM:** `∫(X^2+SIN(2×X))dX = ∫(X^2)dX+∫(SIN(2×X))dX`, the terms' steps a level down, then the result. A minus
  through the whole is CONSTANT MULTIPLE: `∫(-SIN(X))dX = -∫(SIN(X))dX`.
- **CONSTANT MULTIPLE:** `∫(3×COS(X))dX = 3×∫(COS(X))dX`, the factor's step, then the result.
- **A table form:** POWER ((aX+b)^n, SQRT, SQ, INV), RECIPROCAL (n = -1: LN(ABS(…))), EXP, SIN, COS, TAN, SINH, COSH,
  ALOG. When the argument is aX+b and not X, its substitution comes first, as a student writes it:
  `∫(SIN(2×X+1))dX = ∫(SIN(U))dU÷2, U=2×X+1` (V when the variable is U; ÷(-1) is a minus).
- **POLYNOMIAL:** a product multiplied out, `∫((X+1)×(X-2))dX = ∫(-X+X^2-2)dX`, then the result.
- **CONSTANT:** `∫(5)dX = 5×X`.
- **CHECK** (its own rule id, so a caller may hide it), before each term's result: `d/dX(F) = f`.

Integrands print as typed; answers in canonical form; no step carries "+C" (the firmware draws it beside the answer,
unit 048). The last step is the root's result, integrate's own answer.

## How

- Integrate itself is unchanged with steps off: casim_step runs it, and only once it has answered does a second walk
  of the typed text (irec_sum, irec_term, irec_factor) take the same decisions and record the steps. Hooks inside
  integrate, the first build, moved its frames on the S3 (+80 bytes a level of nesting); the walk keeps them as they
  were, measured function by function against 1.0.0-rc3.
- A record is the typed subtree, the rule, the line, a divisor's sign and the call that gives its result; 8 bytes at
  the arena's top. No record keeps an answer: each result is its call run again when its page prints. After the run
  and after the recording, the arena goes back to where parsing left it (the cached constants made since go too), so
  a page needs the arena integrate needs, plus 8 bytes a step.
- Compaction (each term's garbage given back after its check) never reaches the records: measured first, turning it
  off in steps mode cost up to ×5.9 the arena, and one case past 8 KB (#6227).

## The oracle (abacus #6228: as for derive)

1. Steps off is byte-identical: the golden corpus, integrate's frames on the S3, and the bench's per-case stacks.
2. Every step is right by value: a result's and a CHECK's F differentiates to the integrand; a template differentiates
   to it too, each ∫(w)dX taken as w and a substitution's ∫(H(U))dU÷a, U=L as H(L)×L'÷a. abacus's hidden judge
   (judge_steps.py) and the public one (scripts/istep-values.py, make check).
3. The steps compose: the last step is integrate's answer byte for byte; k runs 0..count−1.
4. The same page every time.
5. The stack: the bench's worst + one interrupt frame leaves at least 256 of 12,288 bytes (#6224).
6. Steps that don't fit fail softly: NO_ROOM, integrate still answers; one case is shown.
7. The page cost on the S3, measured.
