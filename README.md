# LL1-Parser-Generator

Team 4 — Compiler Design course project.

**Title (locked):** Design and Implementation of an LL(1) Parser Generator with
Automatic FIRST-FOLLOW Computation, Predictive Parsing Table Construction and
Syntax-Directed Translation.

A generic, table-driven LL(1) parser generator written in C99. It reads any
context-free grammar in the documented text format, identifies terminals and
non-terminals, computes FIRST and FOLLOW sets, builds the predictive parsing
table, detects LL(1) conflicts, parses input strings, and performs
syntax-directed translation (postfix + three-address code).

## Build

```
make          # or: gcc -std=c99 -Wall -Wextra -Iinclude src/*.c -o ll1
./ll1 gram.txt
```

## Grammar format

```
E -> T Ep
Ep -> + T Ep | #
T -> F Tp
Tp -> * F Tp | #
F -> ( E ) | id
```

`->` separates LHS/RHS, `|` separates alternatives, `#` is epsilon, symbols
are space-separated, first LHS is the start symbol. See
`docs/input_format.md` for the full rules and diagnostics.

## Status

| Component | Owner | Status |
|---|---|---|
| Grammar processing + validation | 24bce0092 | done |
| FIRST computation | 24bde0155 | done |
| FOLLOW computation | 24bce0556 | done |
| Parsing table + LL(1) conflicts | 24bci0103 | done |
| Predictive parser + SDT | 24BDS0044 | done |
| Integration (main.c, menu, pipeline) | all | done |
| Test suite (19 checks) | all | done |

Full description: `PROJECT_SPECIFICATION.txt`. Review presentation:
`docs/presentation.md`.

## Layout

```
include/   module headers (the shared interface contract)
src/       one translation unit per module + main.c
testcases/ valid/, invalid/, edge/ grammar and input files
docs/      design.md, input_format.md, algorithms.md, user_guide.md
report/    project report
```

Documentation lives in `docs/`; module ownership and the dependency graph
are described in `docs/design.md`.

## Running the website (ParseLab)

ParseLab is the interactive web front end for this project. **Every result shown in
the browser is computed by this repository's own C engine** — nothing is
re-implemented in JavaScript and nothing is hard-coded per grammar.

### Architecture (what was added, and why)

- `tools/json_main.c` + `src/json_export.c` (new Makefile target `ll1json`) — an
  additive entry point that reuses every existing engine module unchanged and
  emits ONE machine-readable JSON document: grammar validation, FIRST, FOLLOW,
  the LL(1) table, conflicts, the full predictive-parse trace (per-step stack /
  remaining input / action / kind) and the syntax-directed translation. Chosen so
  results come from the real C code instead of scraping the prose output of `ll1`.
- `web/server.py` — the adapter. A **dependency-free Python 3 standard-lib**
  HTTP server (http.server + subprocess only; no pip installs) that serves the
  static frontend from `web/` and runs `ll1json` on a random temp file with an
  argv array (never a shell string), a 10 s timeout and a 64 KB grammar limit.
  One process = one command = whole app.
- `web/index.html`, `web/css/styles.css`, `web/js/engineClient.js`,
  `web/js/main.js` — the frontend: pure HTML/CSS/ES-modules, **no build step,
  no npm, no framework**. `engineClient.js` is the only module that talks to the
  adapter; grammar editing, trace visualization and labels are cosmetic — all
  numbers come from the engine. There is **no browser demo mode and no
  fabricated results**: if the engine is unavailable, stages are disabled and
  errors are shown.

### Prerequisites

- gcc (the same one used for `make`), Python 3.8+, any modern browser. No
  Node.js/npm needed to run it (Node ≥ 20 runs the adapter test suite only).

### Exact commands

```sh
# 1. build the classic CLI engine (Makefile target, unchanged behaviour)
gcc -std=c99 -Wall -Wextra -O2 -Iinclude src/*.c -o ll1        # or: make

# 2. build the JSON engine the adapter drives (new additive target)
gcc -std=c99 -Wall -Wextra -O2 -Iinclude src/grammar.c src/first.c src/follow.c \
    src/parsing_table.c src/parser.c src/translation.c src/utils.c \
    src/json_export.c tools/json_main.c -o ll1json              # or: make ll1json

# 3. start adapter + frontend together ( binds 127.0.0.1:8787 )
python web/server.py                                            # or: make web / make start

# 4. open the site
#    http://127.0.0.1:8787/
```

Environment variables (all optional): `PARSELAB_HOST` (default `127.0.0.1`),
`PARSELAB_PORT` (default `8787`), `PARSELAB_ENGINE` (default `./ll1json(.exe)`).

### Adapter API

| Route | Method | Meaning |
|---|---|---|
| `/api/health` | GET | Runs the engine on a built-in sample. 200 `{ok, adapter:true, engine:true, engineBinary}` means the C engine really works; 503 + `error.code` (`ENGINE_NOT_BUILT`, `ENGINE_UNHEALTHY`) means it does not. |
| `/api/analyze` | POST | `{ "grammar": "<text>", "input": "<string>" }` → the full engine JSON document (grammar, first, follow, table, conflicts, parse, translation). Grammar errors return HTTP 200 with `{ok:false, stage:"grammar", grammarError:"… line-level message"}` so the UI can show the engine's own diagnostics. |

Failure envelope on the adapter side: HTTP 4xx/5xx with
`{"ok":false,"error":{"code":"…","message":"…"}}` — codes include
`BAD_REQUEST`, `GRAMMAR_TOO_LARGE`, `INPUT_TOO_LARGE`, `ENGINE_NOT_BUILT`,
`ENGINE_TIMEOUT`, `ENGINE_EMPTY_OUTPUT`, `ENGINE_BAD_OUTPUT`, `NOT_FOUND`.

### Engine limitations the UI reports honestly

- A non-LL(1) grammar is not parsed or translated — the engine refuses by
  design; the UI shows the conflicts and marks Parse/Translate
  "not supported by the engine for this grammar".
- TAC is generated for the binary operators `+ - * /` only.
- Limits: ≤ 50 productions / 50 symbols, ≤ 255 tokens / 4095 chars of input.

### Troubleshooting

- **"Engine unavailable" in the header** — the adapter is not running, or
  `ll1json` is missing at the expected path. Start `python web/server.py` (from
  the repo root) or set `PARSELAB_ENGINE` to the correct binary path. The
  header pill has a click-to-Retry, and every failed panel has a Retry button.
- **Binary not built / built for a different OS** — `curl`ing
  `/api/health` returns `ENGINE_NOT_BUILT` with the exact build command; run
  the gcc command above (add `.exe` on Windows).
- **Port already in use** — change it with
  `PARSELAB_PORT=8899 python web/server.py` and reload the page at the new
  port (the default 8787 is also where the frontend is served, so both move
  together).
- **Compiler or make not found** — this host needs no `make`; use the plain
  gcc commands above. Check `gcc --version`.
- **CORS or proxy errors** — the adapter talks only to its own pages on
  `127.0.0.1`/`localhost` and binds loopback-only. Don't proxy it through
  anything else; open `http://127.0.0.1:8787/` directly.
- **Engine timeouts** — grammar ≤ 64 KB and input ≤ 4 KB are enforced
  client-side; the engine call itself gets 10 s (`ENGINE_TIMEOUT` → HTTP 504).
- **Malformed engine output** — the adapter verifies that engine stdout parses
  as one JSON object (`ENGINE_BAD_OUTPUT` → HTTP 500) instead of scraping
  semi-structured prose.
- **Node/package-manager problems** — the app doesn't need them. Only the
  optional adapter tests do: `node --test web/test/`.
- **Permission errors on temp files** — engine runs use the system temp dir
  (`tempfile.mkdtemp`) and clean up in a `finally`; if a paranoid antivirus
  blocks it, set `TMPDIR`/`TEMP` to a writable folder.
