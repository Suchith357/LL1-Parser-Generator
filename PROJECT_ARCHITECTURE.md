# PROJECT ARCHITECTURE — LL(1) Parser Generator / ParseLab

## 1. Project Overview
1. LL(1) Parser Generator: a C program that turns a context-free grammar into a predictive LL(1) parser, automatically computing FIRST and FOLLOW sets.
2. It builds the predictive parsing table M[A, a], detects LL(1) conflicts, runs a traceable predictive parser, and performs syntax-directed translation (SDT).
3. ParseLab is the web front end: grammar editing, FIRST/FOLLOW tables, cell-level parsing-table view, animated parse trace, postfix and three-address-code (TAC) output.
4. Intended users: computer-science students (compiler-design labs), faculty evaluators in a viva, and developers learning LL(1) parsing.
5. All computation happens in the compiled C engine; the browser never re-implements or fabricates results.

## 2. Technology Stack and Architecture
1. Engine: pure C99 (gcc, -Wall -Wextra), one translation unit per module under `src/`.
2. Adapter: Python 3 standard library only (`web/server.py`, http.server + subprocess) — no pip packages, binds loopback-only.
3. Frontend: dependency-free HTML/CSS/ES-modules (`web/`), no framework, no build step, no npm.
4. Flow: browser → POST /api/analyze (JSON) → adapter writes the grammar to a random temp file → runs `ll1json` via an argv array (never a shell string) → one JSON document back → UI renders.
5. The engine entry point `tools/json_main.c` outputs machine-readable JSON via `src/json_export.c`, reusing the unchanged engine modules.
6. `/api/health` really executes the engine on a built-in sample before the header reports "Engine connected".
7. Adapter limits: grammar ≤ 64 KB, input ≤ 4 KB, 10 s engine timeout, structured JSON error envelopes with stable codes.
8. The classic CLI (`ll1`) and its menu/file-input modes are untouched; the web pipeline is a separate additive target (`ll1json`) plus `make ll1json` / `make web`.

## 3. Repository Structure
- `src/grammar.c` — grammar reading (`readGrammarFromStream`), production parsing (`addProduction`), symbol classification (`identifySymbols`), validation (`grammarValidate`); `src/utils.c` — shared string helpers.
- `src/first.c`, `src/follow.c` — FIRST/FOLLOW fixed-point computation (`computeFirstSets`, `computeFollowSets`).
- `src/parsing_table.c` — table construction (`constructParsingTable`), conflict collection, table printing.
- `src/parser.c` — predictive parser with step recording (`parserRun`, `parserRunRecordSteps`, additive default-off hooks).
- `src/translation.c` — SDT (semantic stack, `sdtAction`) + TAC generation from postfix (`translateInput`).
- `src/main.c` — CLI/menu modes; `src/json_export.c` + `tools/json_main.c` — additive JSON mode (target `ll1json`, default `ll1` untouched).
- `include/` — the shared module contract (`grammar.h`, `first.h`, `follow.h`, `parsing_table.h`, `parser.h`, `translation.h`, `constants.h`, `json_export.h`).
- `web/server.py` — adapter + static file server; `web/index.html`, `web/css/styles.css`, `web/js/engineClient.js`, `web/js/main.js` — frontend.
- `Makefile` — original targets (`all`, `run`, `test`, `clean`) plus additive `ll1json`, `web`, `start`; `testcases/` — valid/, invalid/, edge/ grammars, inputs/, `run_tests.sh` (26 tests).
- Docs: `README.md`, `docs/` (design, input format, algorithms, user guide), `PROJECT_SPECIFICATION.txt`, this file.

## 4. Module-by-Module
1. Grammar handling: `grammar.c` parses `LHS -> ALT1 | ALT2` lines; symbols are 1–31 chars of alphanumerics, `_`, and `+ - * / ( ) =`.
2. Validation: `grammarValidate` rejects a missing/double arrow, empty alternatives, `$` in the grammar, invalid symbols, and used-but-undefined nonterminals — each with a line-level message.
3. FIRST: `first.c` `computeFirstSets` — fixed-point iteration handling epsilon propagation through RHS symbol sequences (`firstOfSequence`).
4. FOLLOW: `follow.c` `computeFollowSets` — Rule 1 ($ in FOLLOW(start)), Rule 2 (FIRST(β)\{#}), Rule 3 (FOLLOW(A) into FOLLOW(B)), iterated to a fixed point.
5. Table: `parsing_table.c` `constructParsingTable` fills M[A, a] = `A -> α` for each a ∈ FIRST(α), plus FOLLOW(A) columns when α ⇒* ε.
6. Conflicts: two entries per cell become a conflict record (FIRST/FIRST or FIRST/FOLLOW, e.g. left recursion); collected in `conflicts[]`, summarised by `isLL1`.
7. Parsing: `parser.c` `parserRunRecordSteps` — stack (bottom marker `$`), lookahead, expand/match/accept/error steps; unknown input tokens are rejected before expansion.
8. Trace: each step stores step number, full stack (top first), remaining input, action text, and a `kind` (expand/match/accept/error); capped at MAX_PARSE_STEPS = 20 000.
9. SDT: `translation.c` evaluates shape-based semantic rules on each reduce (identity, paren-drop, postfix operand+operator, concatenation), producing postfix; a stack evaluator then emits `t1 = id * id` style TAC for `+ - * /` only.
10. Engine communication: `web/js/engineClient.js` is the only module calling `fetch` (GET /api/health, POST /api/analyze), normalising failures into typed `EngineError`s with a retryable flag.
11. UI rendering & state: `web/js/main.js` renders the four stage panels from the engine document (`S.doc`), tracks staleness and playback state, and rebinds Retry handlers after error panels re-render; index.html provides the editor, stepper, stages, status bar and modals.

## 5. Grammar Input Rules and Restrictions
1. One production group per line: `LHS -> ALT1 | ALT2`; the arrow is exactly `->` and `|` separates alternatives; whitespace between symbols optional (`E->+ T Ep|#` parses); blank lines skipped.
2. Symbols: 1–31 chars of alphanumerics + `_` (multi-character likes `id` supported) and single characters `+ - * / ( ) =`; no quoted terminals exist.
3. Epsilon is `#` (a whole alternative, `A -> a | #`); `$` is reserved end-of-input and rejected as a grammar symbol.
4. Start symbol = the LHS of the first production line (no override).
5. Max 50 productions, 50 symbols, 20 RHS symbols per alternative (constants.h); rejected with line-level messages: missing/double arrow, empty alternative, `$` as symbol, undefined nonterminals, invalid characters.
6. Left recursion is *accepted by the validator* but makes the grammar non-LL(1): the table reports a FIRST/FOLLOW conflict and the parser refuses to run.
7. Example — the arithmetic grammar used throughout:
```text
E  -> T Ep
Ep -> + T Ep | #
T  -> F Tp
Tp -> * F Tp | #
F  -> ( E ) | id
```
8. Input strings (Parse stage) are whitespace-separated token lists (`id + id * id`); the engine appends the end marker `$` itself, and an unknown token like `@` becomes a syntax error at parse time.

## 6. End-to-End Workflow
1. Enter/load a grammar (editor or example dropdown) → Validate/Analyze.
2. Adapter validates size limits and POSTs; the engine runs validation → FIRST → FOLLOW → table → conflict scan in one pass.
3. Success shows start symbol, symbol counts, productions, FIRST/FOLLOW tables and the LL(1) verdict; conflicts appear with explanations.
4. "Parsing Table" stage renders the M[A, a] grid: filled cells, empty (error-move) cells, and conflict-marked cells linked to the conflict report.
5. "Parse" stage: enter an input string, run the engine → per-step trace (stack top-first with `$` at the bottom, remaining input, action) with exact failing step and reason on rejection.
6. Playback controls (Play, Pause, Step, Step Back, Run to End, speed, Export CSV) animate the stored engine trace.
7. "Translate" stage: postfix form and TAC for the last accepted sentence; unavailable for rejected inputs or non-LL(1) grammars, with the engine's message.
8. Errors: grammar rejections show line-level diagnostics; adapter/engine failures show typed error cards with a working Retry button; the header pill confirms the live engine state.
9. Stale handling: any grammar edit shows a banner and disables later stages until re-analysis; non-LL(1) grammars keep Parse/Translate disabled with "not supported by the engine for this grammar".

## 7. Frontend and User Interface
1. Grammar editor with line-number gutter, live syntax highlighting (arrow, alternatives, terminals, epsilon) and a `#`-insert button.
2. Example selector ships six grammars: arithmetic demo, statement grammar, optional symbol, left recursion, FIRST/FIRST and FIRST/FOLLOW conflicts — each with a suggested input.
3. Stage stepper (Analysis → Table → Parse → Translate) gates unreachable stages until the engine allows them.
4. Panels render FIRST/FOLLOW set tables, the production list, the cell-level table, the trace table and postfix/TAC blocks — every number verbatim from the engine JSON.
5. Controls: Save/Load (localStorage), Export/Import session JSON, theme toggle (graphite dark / paper light), Help dialog with stage explainers, Start-over confirmation, toasts and aria-live announcements.
6. Responsive: panes stack below 1100 px, no horizontal overflow at 360 px, keyboard-accessible (Ctrl+Enter = analyze); both themes are full token sets in `web/css/styles.css`.

## 8. Limitations and Known Issues
1. LL(1) only: grammars needing backtracking (FIRST/FIRST, FIRST/FOLLOW clashes, left recursion) are not parsed — the engine deliberately refuses; verified in tests.
2. SDT is shape-rule based (identity/paren/postfix/concat), not a general attributed-grammar mechanism; TAC covers only `+ - * /`.
3. Engine limits: 50 productions/symbols, 255 input tokens, 4 095 chars, 20 000 trace steps; larger inputs are refused with clear errors.
4. Error recovery stops at the first syntax error — no resynchronisation (verified in the parser's behaviour).
5. Persistence is browser-local only (localStorage + session JSON export); no server-side accounts or storage.
6. Interval-playback timing cannot be verified inside the embedded test browser (its timers are suspended there); stepping and play controls themselves were verified working.
7. Postfix/TAC semantics assume expression-like inputs — grammars without that shape still parse but only translate structurally (concatenated postfix, no TAC).
8. Not verified: multi-tab concurrent use (the adapter is stateless per request, so contention is expected to be safe, but it was not tested).

## 9. Build, Run, and Test
```sh
# 1) classic CLI engine (unchanged)   2) JSON engine for the adapter
gcc -std=c99 -Wall -Wextra -O2 -Iinclude src/*.c -o ll1
gcc -std=c99 -Wall -Wextra -O2 -Iinclude src/grammar.c src/first.c src/follow.c src/parsing_table.c \
    src/parser.c src/translation.c src/utils.c src/json_export.c tools/json_main.c -o ll1json
python web/server.py   # 3) adapter + website at http://127.0.0.1:8787/
```
1. Health check: `curl http://127.0.0.1:8787/api/health` must return `"engine": true` (it really runs the engine); Makefile equivalents: `make`, `make ll1json`, `make web` (= start), `make test`, `make clean`.
2. Tests: `sh testcases/run_tests.sh` (26 integration checks, last run: 26/26 passed) and optional `node --test web/test/engine-client.test.mjs` (7 adapter-client checks, last run: 7/7 passed); CLI demo: `./ll1 testcases/valid/expression_valid.txt < testcases/inputs/demo_inputs.txt`.

## 10. Summary
1. One pipeline: grammar text → validation → FIRST/FOLLOW → table/conflicts → traced predictive parse → postfix → TAC.
2. Data flow is single-source: every number on screen is the engine's JSON output, relayed by a small stdlib adapter.
3. Strengths: zero runtime dependencies, honest failure reporting, visual trace playback, spec-exact arithmetic results, untouched legacy CLI.
4. Scope: an educational LL(1) workbench for the classroom and the viva — not a production parser framework.
