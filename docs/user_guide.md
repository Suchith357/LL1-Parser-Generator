# User Guide — LL(1) Parser Generator

Everything needed to build, run, and demonstrate the tool. For theory and
per-module algorithms see `docs/algorithms.md`; for the grammar format rules
see `docs/input_format.md`.

---

## 1. Building

Requirements: any C99 compiler (GCC, Clang, MinGW-w64 on Windows).

```bash
make          # builds ll1 (or ll1.exe on Windows)
make test     # builds, then runs the testcase suite (testcases/run_tests.sh)
make clean    # removes objects and the executable
```

Manual build without make:

```bash
gcc -std=c99 -Wall -Wextra -pedantic -O2 -Iinclude src/*.c -o ll1
```

The build is warning-free with `-Wall -Wextra -pedantic`.

---

## 2. Running — three input modes

### Mode A — automatic pipeline (recommended for the demo)

```bash
./ll1 testcases/valid/expression_valid.txt
```

Runs the complete pipeline and prints:

1. Grammar (productions, non-terminals, terminals, start symbol)
2. FIRST sets
3. FOLLOW sets
4. Predictive parsing table
5. LL(1) analysis (conflicts or "Grammar is LL(1)")
6. A parse/translate prompt: type an input string per line and press Enter —
   each line is parsed **and** translated (postfix + three-address code)
   until you press Ctrl+D (Ctrl+Z then Enter on Windows) or enter `quit`.

Batch the inputs instead by piping a file:

```bash
./ll1 testcases/valid/expression_valid.txt < testcases/inputs/demo_inputs.txt
```

### Mode B — grammar on stdin

```bash
./ll1 - < testcases/valid/expression_valid.txt
```

Same pipeline, grammar read from stdin; the parse phase is skipped because
stdin is exhausted.

### Mode C — interactive menu

```bash
./ll1
```

```
=========================================
        LL(1) PARSER GENERATOR
=========================================
1. Load Grammar
2. Display Grammar
3. Compute FIRST Sets
4. Compute FOLLOW Sets
5. Construct Parsing Table
6. Check LL(1)
7. Parse Input
8. Syntax-Directed Translation
9. Exit
```

Type the grammar lines, finish with a line containing only `END`.
Options run in any order and the grammar stays in memory; choosing an
option before loading a grammar reminds you to load one first.

---

## 3. Grammar input format (quick reference)

```
E -> T Ep
Ep -> + T Ep | #
T -> F Tp
Tp -> * F Tp | #
F -> ( E ) | id
```

- One production per line; `->` separates LHS and RHS (spaces around it
  optional); `|` separates alternatives; `#` is epsilon; symbols are
  space-separated; the first production's LHS is the start symbol.
- Symbols may be multi-character (`Ep`, `id`, `Expr`).
- `$` and `#` are reserved and cannot be used as grammar symbols.
- `( E )` must be written with spaces — without them `(E)` is one symbol.

---

## 4. Reading the output

**FIRST/FOLLOW sets** — printed one per non-terminal in definition order;
`#` inside a FIRST set means the non-terminal is nullable.

**Parsing table** — rows are non-terminals, columns are terminals plus `$`;
cells show the selected production. Empty cells are where syntax errors
would be raised.

**LL(1) analysis** — either
`No conflicts detected. Grammar is LL(1).` or a list like:

```
Conflict at M[S, a]:
  existing:   S -> a A
  conflicting: S -> a B
```

A grammar with any conflict is **not** LL(1); the parser refuses to run on
it and reports why.

**Parsing trace** — three columns: stack (bottom→top, `$` at the left),
remaining input (current token first), and the action applied
(production expanded, terminal matched, or accept). On error you get the
stack position, the offending token, and the tokens that would have been
acceptable.

**Translation** — postfix expression then three-address code with
temporaries `t1, t2, ...`:

```
Input:    id + id * id
Postfix:  id id id * +
TAC:
t1 = id * id
t2 = id + t1
```

---

## 5. Demonstration script (reviews / viva)

1. `make clean && make` — show a clean, warning-free build.
2. `./ll1 testcases/valid/expression_valid.txt < testcases/inputs/demo_inputs.txt`
   — full pipeline on the locked grammar; point out FIRST/FOLLOW matching
   the theory (computed, not hardcoded).
3. Show the parsing trace for `id + id * id` — accepted; then `id + * id`
   — syntax error with the expected-token diagnostic.
4. `./ll1 testcases/valid/non_ll1_conflict.txt` — conflict reported at
   `M[S, a]`, "Grammar is NOT LL(1)".
5. `./ll1 testcases/valid/epsilon_propagation.txt` — FIRST(S) = { a, b }:
   epsilon propagation across a sequence.
6. `./ll1 testcases/edge/pure_epsilon.txt` — `S -> #` handled without
   crashing; FOLLOW(S) = { $ }.
7. `./ll1 testcases/invalid/undefined_nt.txt` — line-numbered grammar
   validation diagnostic.

Each step maps to one rubric line: correctness, conflict detection, epsilon
handling, error handling, diagnostics.

---

## 6. Test suite

```bash
make test          # or: bash testcases/run_tests.sh
```

`testcases/` layout:

| Directory | Contents |
|---|---|
| `valid/` | grammars that must load and analyse cleanly (standard expression, epsilon propagation, simple, indirect recursion, left-recursive detection, non-LL(1) conflict) |
| `invalid/` | grammars that must be **rejected** with a clear diagnostic (undefined non-terminal, missing/double arrow, bad symbol, empty alternative, `$` in grammar) |
| `edge/` | boundary grammars (pure epsilon, no-space arrow, optional symbol, single token, deep expression, empty input) |
| `inputs/` | sample parse inputs for the demo grammar |

`run_tests.sh` runs every grammar through the binary, reports pass/fail
against expected behaviour (valid ones analyse; invalid ones are rejected
with a nonzero status), and exits nonzero if anything fails — usable as a
regression check after any change.

---

## 7. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `error: expected '->'` on a line with `=>` or `→` | only ASCII `->` is accepted |
| `undefined non-terminal 'X' (line n)` | `X` appears on some RHS but has no production; add `X -> ...` |
| `(E)` treated as one symbol | symbols are space-separated; write `( E )` |
| Parser refuses: "grammar is not LL(1)" | the table has conflicts; run option 6 / see the conflict report; the grammar needs rewriting (left recursion, left factoring) |
| `input too long` or `too many symbols` | fixed capacity limits hit; shorten input or raise the named constant in `include/constants.h` |
| No executable after `make` | check the compiler is installed (`gcc --version`); see section 1 |
