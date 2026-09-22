# Shared Architecture — LL(1) Parser Generator

Team 4 · Compiler Design Project
Title (locked): *Design and Implementation of an LL(1) Parser Generator with
Automatic FIRST-FOLLOW Computation, Predictive Parsing Table Construction and
Syntax-Directed Translation*

## 1. Key architecture decision: string symbols

The first prototype stored every grammar symbol as a single `char`. That
cannot represent the demo grammar (`Ep`, `Tp`, `id` are two characters), so
the shared architecture (v2) was redesigned before Module 3 onward were
implemented:

- Every symbol is a fixed-size string: `char[MAX_SYMBOL_LENGTH]` (32 bytes).
- All modules include `include/constants.h` (shared limits and sentinels)
  and `include/utils.h` (shared helpers). Sizes are defined **once**.
- FIRST/FOLLOW sets are arrays of symbol strings; the parsing table stores
  production indices, never duplicated production text.

## 2. Module map and ownership

| Module | Files | Owner | Status |
|---|---|---|---|
| Shared core | `include/constants.h`, `include/utils.h`, `src/utils.c` | all | done |
| 1. Grammar + validation | `include/grammar.h`, `src/grammar.c` | 24bce0092 | done (v2) |
| 2. FIRST sets | `include/first.h`, `src/first.c` | 24bde0155 | done (v2) |
| 3. FOLLOW sets | `include/follow.h`, `src/follow.c` | 24bce0556 | done |
| 4. Table + conflicts | `include/parsing_table.h`, `src/parsing_table.c` | 24bci0103 | done |
| 5. Parser + SDT | `include/parser.h`, `include/translation.h`, `src/parser.c`, `src/translation.c` | 24BDS0044 | done |
| Integration | `src/main.c` | shared | full pipeline + menu done |

Complete system description: `PROJECT_SPECIFICATION.txt`. Presentation:
`docs/presentation.md`.

## 3. Data structures (the contract)

```
Production { lhs[MAX_SYMBOL_LENGTH]; rhs[MAX_ALTERNATIVES][MAX_RHS_LENGTH];
             rhsCount }                       // rhs[j] is "T Ep" or "#" or "+ T Ep"
Grammar    { productions[]; productionCount; nonTerminals[][]; nonTerminalCount;
             terminals[][]; terminalCount; startSymbol[] }
FirstSet   { symbolName[]; elements[][]; count; hasEpsilon }
FirstCollection { sets[MAX_SYMBOLS]; setCount }   // sets[i] <-> nonTerminals[i]
FollowSet  { elements[][]; count }                // terminals plus "$"
FollowCollection { sets[MAX_SYMBOLS]; setCount }
TableCell  { lhsIndex; altIndex }                 // INVALID_INDEX when empty
ParsingTable { cells[MAX_TABLE_ROWS][MAX_TABLE_COLS]; conflictCount;
             conflicts[] }                        // "$" is the last column
```

## 4. Module dependency graph

```
constants.h ──> utils.h ──> grammar.h ──> first.h ──> follow.h
                                   │                        │
                                   └──── parsing_table.h ◄──┘
                                             │
                                    parser.h ┴ translation.h ──> main.c
```

Key reuse points (do not duplicate this logic):

- `productionAltSymbolCount()` / `productionAltGetSymbol()` — RHS tokenizer
  used by FIRST, FOLLOW, the table, the parser, and validation.
- `firstOfSequence()` — FIRST of a full RHS alternative; FOLLOW Rule 2 and
  table construction must use this instead of recomputing.
- `grammarIsNonTerminal()` / `grammarIsTerminal()` — classification.
- `END_MARKER` (`$`) and `INVALID_INDEX` are the only sentinel values.

## 5. Build strategy

- `make` (C99, `-Wall -Wextra`, objects in `build/`, binary `ll1`).
- One translation unit per module; no cross-inclusion of `.c` files.
- `make test` runs every file in `testcases/{valid,invalid,edge}`.
- No global variables outside `const` tables; every module works on
  caller-provided structs so tests can construct isolated inputs.

## 6. Git branch strategy

- `main` — always compiles; integration happens via merge.
- `feature/grammar`, `feature/first`, `feature/follow`,
  `feature/parsing-table`, `feature/parser-translation` — one per owner.
- Each member commits under their own account: module commits, bug fixes,
  testcase commits, and docs for their own module.
- Interface changes require a message to the team **before** pushing.

## 7. Complexity of the implemented part (Review 1 scope)

- Grammar validation: O(total characters); symbol identification:
  O(P · R · (N + T)) with P productions, R RHS symbols, N non-terminals,
  T terminals (linear scans; fine for course-scale grammars).
- FIRST fixed point: at most N + T iterations; each sweep touches every
  production once → O((N + T) · P · R) worst case, in practice 2–3 sweeps.
