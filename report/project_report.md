# Final Project Report — Draft

> **Status:** structure complete; screenshots and "Actual Output" columns
> must be filled from a real run before submission. Do not submit any
> section marked `[VERIFY]` without confirming it against the program.

---

## 1. Title

Design and Implementation of an LL(1) Parser Generator with Automatic
FIRST–FOLLOW Computation, Predictive Parsing Table Construction and
Syntax-Directed Translation

## 2. Team Members

| # | Roll Number | Module Ownership |
|---|---|---|
| 1 | 24bce0092 | Grammar processing and validation |
| 2 | 24bde0155 | FIRST set computation |
| 3 | 24bce0556 | FOLLOW set computation |
| 4 | 24bci0103 | Predictive parsing table + LL(1) conflict detection |
| 5 | 24BDS0044 | Predictive parser + syntax-directed translation |

Shared responsibilities: integration (`main.c`), testcase suite,
documentation, final demonstration.

## 3. Problem Statement

Compilers rely on efficient, deterministic syntax analysis. For a large and
practically important class of context-free grammars — the LL(1) grammars —
parsing can be done with a single-token lookahead using a table-driven
predictive parser. Building the required FIRST/FOLLOW sets and the parsing
table by hand is error-prone and does not scale. The problem addressed by
this project is to **automate the entire front-end analysis pipeline**: given
any context-free grammar in a simple text format, the tool validates it,
computes FIRST and FOLLOW sets, constructs the predictive parsing table,
determines whether the grammar is LL(1), and — for LL(1) grammars — parses
input strings and performs syntax-directed translation to postfix notation
and three-address code.

## 4. Objectives

1. Accept a general context-free grammar in a documented text format
   (multi-character symbols, alternatives, epsilon).
2. Automatically identify terminals, non-terminals, and the start symbol.
3. Compute FIRST sets by fixed-point iteration with epsilon propagation.
4. Compute FOLLOW sets by fixed-point iteration implementing all three
   standard rules.
5. Construct the LL(1) predictive parsing table automatically, storing
   production references rather than duplicated text.
6. Detect and report LL(1) conflicts without overwriting table entries.
7. Parse input strings with a table-driven predictive parser and produce a
   step-by-step trace with meaningful error diagnostics.
8. Perform syntax-directed translation during parsing to emit postfix form
   and three-address code.
9. Handle invalid grammars and invalid inputs safely, with informative
   diagnostics and no crashes or undefined behavior.

## 5. Scope

**In scope:** LL(1) grammars in the documented format; automatic
FIRST/FOLLOW/table construction; conflict detection; predictive parsing with
traces; expression-oriented syntax-directed translation (postfix and TAC);
file, stdin, and interactive-menu input modes; command-line operation.

**Out of scope (known limitations, §X):** grammars that are not LL(1) are
diagnosed but not automatically transformed; no left-recursion elimination
or left-factoring is performed automatically; symbols are space-separated
tokens; fixed-size internal tables.

## 6. Grammar / Language Specification

Format (full rules in `docs/input_format.md`):

```
E -> T Ep
Ep -> + T Ep | #
T -> F Tp
Tp -> * F Tp | #
F -> ( E ) | id
```

- One production per line; `->` separates LHS and RHS; `|` separates
  alternatives; `#` denotes epsilon; symbols are space-separated strings.
- Non-terminal = any symbol appearing on some LHS; terminal = RHS symbol
  never appearing on a LHS.
- The first production's LHS is the start symbol.
- `$` (end marker) is reserved and rejected inside grammars.
- Symbol characters: letters, digits, `+ - * / ( ) _ .`.

## 7. Theoretical Background

An LL(1) parser scans the input Left-to-right, produces a Leftmost
derivation, and uses 1 lookahead symbol. Predictive parsing is possible
exactly when no non-terminal has two alternatives whose FIRST sets
intersect, and no nullable non-terminal has an alternative whose FIRST set
intersects its FOLLOW set. FIRST(X) is the set of terminals that can begin
strings derived from X (plus `#` if X is nullable). FOLLOW(A) is the set of
terminals that can appear immediately to the right of A in some sentential
form (plus `$` for the start symbol). The parsing table M[A, a] maps a
non-terminal and lookahead to the unique applicable production; the
existence of such a conflict-free table is equivalent to the grammar being
LL(1). Syntax-directed translation attaches semantic actions to productions
so that the translation is emitted as a side effect of parsing.

## 8. System Architecture

```
CFG INPUT (file/stdin/interactive)
        |
        v
GRAMMAR PROCESSING + VALIDATION  (grammar.c)
        |
        v
SYMBOL IDENTIFICATION            (grammar.c)
        |
        v
FIRST COMPUTATION                (first.c)
        |
        v
FOLLOW COMPUTATION               (follow.c)
        |
        v
PARSING TABLE CONSTRUCTION       (parsing_table.c)
        |
        v
LL(1) CONFLICT DETECTION         (parsing_table.c)
        |                \
        v                 v
   (conflicts)       (no conflicts)
   report, stop      PREDICTIVE PARSER   (parser.c)
                          |
                          v
                  SYNTAX-DIRECTED TRANSLATION (translation.c)
                          |
                          v
                  postfix + three-address code
```

Shared data structures (`constants.h`, `utils.h`): fixed-length string
symbols (`MAX_SYMBOL_LENGTH = 32`), `StringSet`, symbol indexing by
`nonTerminals[]`/`terminals[]` arrays; all limits centralized in
`include/constants.h`.

## 9–16. Module Design and Algorithms

Summarized here; full per-module algorithm descriptions, edge cases, and
complexity are in `docs/algorithms.md`.

- **Module 1 — Grammar (24bce0092):** line-based reader; per-line validation
  before parsing; alternative splitting; LHS-based non-terminal
  identification; whole-grammar checks (undefined non-terminals, reserved
  `$`, capacity overflow) with line-numbered diagnostics.
- **Module 2 — FIRST (24bde0155):** fixed-point iteration; `firstOfSequence`
  walks RHS symbols left to right, continuing past nullable symbols;
  recursion and indirect recursion handled by iteration to convergence.
- **Module 3 — FOLLOW (24bce0556):** `$` into FOLLOW(start); Rule 2 adds
  `FIRST(β) − {#}` after each non-terminal position; Rule 3 adds FOLLOW(A)
  when β is nullable; fixed-point iteration.
- **Module 4 — Table + LL(1) (24bci0103):** for each production, FIRST(α)
  claims cells in row A; if α is nullable, FOLLOW(A) claims cells; duplicate
  claims are recorded as conflicts (both productions preserved).
- **Module 5 — Parser + SDT (24BDS0044):** stack initialized to `$` + start
  symbol; terminal match/pop/advance; table-driven expansions pushing the
  RHS in reverse; epsilon never pushed; accept on `$`/`$`; structured
  diagnostics on every error path; the same machine exposes an action
  callback used by the translator (semantic actions run when productions are
  applied, no second parse).

## 17. Implementation Details

- Language: portable C99; build `gcc -std=c99 -Wall -Wextra -pedantic`
  (warning-free).
- Eight modules (`src/*.c`) with headers under `include/`; all capacity
  limits in `include/constants.h`; shared string-set/symbol-table utilities
  in `src/utils.c`.
- Safety: bounds-checked fixed arrays, overflow flags, no recursion in
  set computation (fixed-point loops), immediate error returns on all
  invalid input paths.
- Modes: `./ll1 grammar.txt` (pipeline + parse prompt), `./ll1 - <
  grammar.txt`, `./ll1` (9-option menu). `make` / `make test` / `make clean`.

## 18–20. Test Cases, Results, Screenshots

Complete designed test suite (inputs in `testcases/`):

| # | Category | Input | Expected | Actual | Status |
|---|---|---|---|---|---|
| T1 | Valid LL(1) grammar | `testcases/valid/expression_valid.txt` | FIRST/FOLLOW per theory, table, "Grammar is LL(1)" | verified: all sets match theory (FOLLOW elements in stable sorted order); table + LL(1) verdict printed | PASS |
| T2 | Epsilon propagation | `testcases/valid/epsilon_propagation.txt` | FIRST(S) = { a, b } | verified: FIRST(A)={a,#}, FIRST(B)={b}, FIRST(S)={a,b} | PASS |
| T3 | Pure epsilon | `testcases/edge/pure_epsilon.txt` | FIRST(S) = { # }, no crash | verified: empty input ACCEPTED, `a` rejected, no crash | PASS |
| T4 | Non-LL(1) conflict | `testcases/valid/non_ll1_conflict.txt` | Conflict at M[S,a], "NOT LL(1)" | verified: both productions reported at M[S,a]; parsing skipped | PASS |
| T5 | Simple grammar | `testcases/valid/simple.txt` | FIRST(S) = { a }, FOLLOW(S) = { $ } | verified: `a` ACCEPTED, `b` rejected with clear diagnostic | PASS |
| T6 | Undefined non-terminal | `testcases/invalid/undefined_nt.txt` | Rejected with line-numbered diagnostic | verified: rejected, exit code 1, pipeline halted before table | PASS |
| T7 | Indirect recursion | `testcases/valid/indirect_recursion.txt` | FIRST/FOLLOW converge; conflict correctly reported (grammar is NOT LL(1): A -> S b vs A -> # at [A,c]) | verified: FIRST(S)={c}, FIRST(A)={#,c}, NOT LL(1) reported | PASS |
| T8 | Valid input parse | `id + id * id` | Trace, INPUT ACCEPTED | verified: 17-step trace, epsilon never pushed, accept on $/$ | PASS |
| T9 | Invalid input parse | `id + * id` | Syntax error with expected-token list | verified: "Unexpected token '*' ... No parsing-table entry for [T, *] Expected one of: (, id" | PASS |
| T10 | Left recursion detected | `testcases/valid/left_recursive.txt` | Reported NOT LL(1) | verified: conflict detected, NOT LL(1) | PASS |
| T11 | Malformed lines | `testcases/invalid/*.txt` | Clear rejection diagnostics | verified: missing/double arrow, bad symbol, empty LHS/RHS/alternative, `$` in grammar — all rejected with line numbers, exit 1 | PASS |
| T12 | Translation | `a + b * c` | Postfix `a b c * +`; TAC `t1 = b * c`, `t2 = a + t1` | verified exactly; also `a * b + c` and `( a + b ) * ( a + c )` (t3 = t1 * t2) — precedence and parentheses correct | PASS |
| T13 | Edge grammars | `testcases/edge/*.txt` | Handled without crash | verified: no-space arrow, pure epsilon, optional symbol, single token, deep expression, empty input — all safe | PASS |

Run command for all of the above:
`bash testcases/run_tests.sh` (or `make test`), plus the demo command in
`docs/user_guide.md` §5.

`[SCREENSHOT: ...]` placeholders — see `report/README.md` checklist.

## 21. Error Handling

Diagnostics implemented (full catalogue in `PROJECT_SPECIFICATION.txt`
§S): missing/double arrow, empty LHS/RHS, invalid symbol characters,
reserved `$`/`#` misuse, undefined non-terminal (with line number),
capacity overflow, LL(1) conflict (with both productions), parser terminal
mismatch (with expected-token list), missing table entry, stack/input
overflow. Invalid input never crashes, hangs, or corrupts memory.

## 22. Individual Contributions

| Member | Technical contribution |
|---|---|
| 24bce0092 | Grammar reader, validator, symbol classification (`grammar.c/h`), module docs |
| 24bde0155 | FIRST fixed-point algorithm, `firstOfSequence` shared helper (`first.c/h`) |
| 24bce0556 | FOLLOW fixed-point algorithm, three FOLLOW rules (`follow.c/h`) |
| 24bci0103 | Table construction, conflict recording/reporting (`parsing_table.c/h`) |
| 24BDS0044 | Predictive parser, trace, action-callback interface, SDT postfix/TAC (`parser.c/h`, `translation.c/h`) |
| All | Testcase suite, integration testing, module documentation, demo preparation |

Each member commits their own module (Git branches `feature/*`) and
explains it in the viva.

## 23. Conclusion

The project delivers a generic, modular LL(1) parser generator in C that
automates the complete analysis pipeline of the assigned title — grammar
validation, FIRST, FOLLOW, table construction, LL(1) determination,
predictive parsing, and syntax-directed translation — with safe input
handling and demonstrable traces suitable for teaching and viva
explanation.

## 24. Future Enhancements

- Automatic left-recursion elimination and left factoring (would make many
  non-LL(1) grammars usable).
- Larger symbol tables or dynamic allocation beyond the fixed limits.
- Additional translation schemes (e.g., expression trees, constant folding).
- A regression-test mode comparing full outputs against stored expected
  files.

## 25. References

1. Aho, Lam, Sethi, Ullman — *Compilers: Principles, Techniques, and Tools*
   (2nd ed.), chapters 4.4 (bottom-up context not needed here) and 2.4 /
   4.4.4 for LL(1) parsing and syntax-directed translation.
2. Course lecture notes — Compiler Design: FIRST, FOLLOW, predictive
   parsing table construction, LL(1) conditions.
3. Project documentation: `PROJECT_SPECIFICATION.txt`,
   `docs/algorithms.md`, `docs/input_format.md`, `docs/design.md`.
