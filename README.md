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
