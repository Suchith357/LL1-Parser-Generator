# ParseLab — Phase 1 Findings Report

Date: 24 October 2026 · Environment: Windows 10 (MSYS/Git Bash), gcc 16.1.0
(ucrt64), Node.js 24, Python 3.12, no GNU make on PATH.

## 1. How the C program is built and run

- `make` → `./ll1` (Makefile: `gcc -std=c99 -Wall -Wextra -O2 -Iinclude src/*.c`).
  **This host has no `make`**, so the documented fallback is used
  verbatim: `gcc -std=c99 -Wall -Wextra -O2 -Iinclude src/*.c -o ll1`.
  On Windows the output is `ll1.exe`. Both produce identical bit-for-bit
  `main.c` behavior.
- Run modes (from `main.c`):
  - `./ll1` — menu mode (options 1–9).
  - `./ll1 grammar.txt` — automatic pipeline, then parses/translates each
    stdin line until EOF/`quit` (empty line = epsilon input).
  - `./ll1 -` — grammar on stdin, then the parse phase is skipped.
- Binary location: repo root (`ll1.exe` / `ll1`).

## 2. Input format (verified from grammar.c + docs/input_format.md + runs)

- One production group per line: `LHS -> ALT1 | ALT2`; `->` is the arrow,
  `|` separates alternatives; symbols separated by whitespace
  (`E->+ T Ep|#` also works); `#` is epsilon and must be its own
  alternative; first LHS is the start symbol; `$` is reserved (rejected —
  it is not a legal symbol character).
- Symbol names: 1–31 chars from alphanumerics, `_` and `+ - * / ( ) =`.
- Input strings at parse time are whitespace-separated token lists; the
  engine appends `$` itself. Input tokens need not be declared terminals:
  unknown tokens are reported at parse time ("not a terminal of the
  grammar").

## 3. What the engine prints (plain text, fixed sections)

`GRAMMAR` / `Non-Terminals: ... Terminals: ... Start Symbol:` /
`FIRST SETS` (`FIRST(E) = { (, id }`, `#` printed for epsilon) /
`FOLLOW SETS` (`$` included) / `PREDICTIVE PARSING TABLE` (`A | t1 | ... |
 $ |` grid, entries `LHS -> RHS`) / `LL(1) ANALYSIS` ("Grammar is LL(1)"
 or "Grammar is NOT LL(1): N conflict(s)" + per-conflict blocks
 `Conflict detected at M[S, a]` + the two productions) / trace table
 (`Step Stack Input Action`, stack bottom→top, `$` bottom marker) /
`Result: INPUT ACCEPTED` or `SYNTAX ERROR` with positioned diagnostics
("Unexpected token 'x' (position n)", "No parsing-table entry for [T, *]",
"Expected: 'y'") / `SYNTAX-DIRECTED TRANSLATION` — `Postfix: ...` and
`  t1 = id * id` TAC lines.

## 4. Feature status (evidence: 26/26 integration tests pass,
`sh testcases/run_tests.sh` → `Tests passed: 26 failed: 0`)

| Feature | Status | Evidence |
|---|---|---|
| Grammar validation (missing/double arrow, empty alt, `$`, invalid symbols, undefined NT) | works | T06a–T06f |
| Symbol classification + start symbol | works | T01 output |
| FIRST (epsilon propagation, fixed point) | works | T01, T02, T07 |
| FOLLOW (fixed point, `$`) | works | T01, T07 |
| Table + conflict detection (FIRST/FIRST, FIRST/FOLLOW, left recursion as conflict) | works | T04, T13, direct runs |
| Predictive parse + positioned error diagnostics | works | T08, T09 |
| Empty/epsilon input | works | T03b, T12b |
| SDT postfix + TAC (structural shape rules) | works | T10, T10b; `id id * id +` / `t1 = id * id` |
| Limits | bounded: 50 productions, 50 symbols, 20k trace steps, 256 tokens |
| Limitation: engine refuses to parse non-LL(1) grammars (by design) — the UI must reflect that |
| Limitation: SDT is not a general scheme — shape rules 1–6 (identity, paren-drop, postfix-op, concat, echo) work for many CFGs but TAC is only emitted for `+ - * /` |
| Edge scope currently covered: no unreachable/non-productive/duplicate-alternative diagnostics (engine accepts these silently) — UI adds presentational warnings |

## 5. How translation rules are defined (src/translation.c)

Not hard-coded to the demo grammar: each completed production is
classified by RHS *shape* at reduce time, on the semantic stack:

1. `#` → empty value; 2. single symbol → identity; 3. `( X… )` → drop
parens, concatenate; 4. leading terminal operator with non-terminals
after → postfix (X1…Xk op); 5. non-terminals only → concatenate;
6. anything else → concatenate (echo fallback). TAC is then generated
from the postfix string with a stack evaluator for single-char
`+ - * /` (temporaries t1, t2, …). So expression-like CFGs translate
automatically; grammars without that shape still parse but produce only
concatenated "postfix" and no TAC.

## 6. Chosen integration architecture

**Choice: thin Python-stdlib adapter + additive JSON mode in C.**

- All algorithmic modules already expose exactly the data the web UI
  needs (`FirstCollection`, `FollowCollection`, `ParsingTable.cells` +
  `conflicts`, `ParserState`, `TranslationResult`), but `main.c` only
  prints prose. Text-scraping prose is brittle (misjudged column
  alignment, locale changes), so per option (b) we additionally compiled
  a **separate additive entry point** (`tools/json_main.c` +
  `src/json_export.c`, new Makefile target `ll1json`) that reuses every
  existing function unchanged (`readGrammarFromStream`,
  `computeFirstSets`, `computeFollowSets`, `constructParsingTable`,
  `parserRun*`, `translateInput`) and emits one JSON document.
- To expose per-step trace data (stack, remaining input, action *kind*)
  without duplicating the parse loop, `parser.c` gained an additive,
  default-off step-recording hook (`parserRunRecordSteps`); every
  existing function/signature/behavior is unchanged (see git diff —
  three call-through wrappers).
- The adapter is a dependency-less Python 3 HTTP server
  (`web/server.py`, stdlib `http.server` + `subprocess`): it runs
  `ll1json` on a random temp file with argv-array invocation (never a
  shell string), 10 s timeout, 64 KB grammar limit, localhost-only by
  default, and it serves the static frontend from the same process so
  **one command starts everything** (`python web/server.py` / `make web`).
- Frontend: dependency-free HTML + CSS + ES-module JavaScript
  (no build step, no npm); two bundled fonts (IBM Plex Sans, JetBrains
  Mono) served locally from `web/fonts/`. Justification: zero new
  packages, works offline, trivially auditable by a viva examiner, and
  the app spends ~200 lines of state code where a framework would pull
  ~30 MB of node_modules.
