# Algorithms — LL(1) Parser Generator

Reference for each module's algorithm as actually implemented. Each section
lists the real inputs/outputs, the steps, the edge cases handled, and the
complexity **derived from the implementation** (no invented claims).

Notation: `N` = number of productions, `T` = number of terminals,
`V` = number of non-terminals, `L` = length of the longest alternative,
`n` = length of the input string, `S` = `MAX_ALTERNATIVES`, `R` = `MAX_RHS`.

---

## 1. Grammar processing (grammar.c)

**Input:** a grammar file / stream in the documented format
(`LHS -> alt1 | alt2`, `#` = epsilon, symbols space-separated).
**Output:** a filled `Grammar` (productions + symbol lists + start symbol),
or a diagnostic and failure status.

### Main steps

1. `readGrammarFromStream()` reads one line at a time into a
   `MAX_LINE_LENGTH` buffer with `fgets`.
2. `validateProduction()` checks each line *before* any parsing:
   - line must be non-empty (blank lines are skipped, not an error);
   - exactly one `->` (rejects missing arrow and double `->`);
   - non-empty LHS made of valid symbol characters;
   - non-empty RHS with non-empty alternatives (no `| |`, no trailing `|`).
3. `addProduction()` splits the RHS on `|`, trims each alternative, and
   stores alternatives as original space-separated strings in
   `Production.rhs[]` with `rhsCount` set.
4. `identifySymbols()` classifies: **non-terminals = symbols appearing as
   some LHS**; **terminals = RHS symbols that are not LHS symbols** and not
   `#`. The first production's LHS becomes the start symbol.
5. `grammarValidate()` runs whole-grammar checks and writes a line-numbered
   diagnostic into the caller's error buffer:
   - a non-terminal used on some RHS but never defined on any LHS;
   - the end marker `$` used inside the grammar (reserved);
   - capacity overflow (`MAX_PRODUCTIONS`, `MAX_ALTERNATIVES`,
     `MAX_RHS_LENGTH`) reported instead of silently truncating.

### Edge cases handled

- `E->T Ep` (no spaces around arrow) — arrow split works with or without
  surrounding spaces; `E->` and `->T` are rejected.
- `S -> #` — epsilon-only production: RHS stores `#`,
  `productionAltIsEpsilon()` returns true.
- Symbol charset = letters, digits, `+ - * / ( ) _ .`. `$`, `#`, `|`, `-` in
  `->`, and whitespace are reserved and rejected inside symbols.
- Multi-character symbols (`Ep`, `Tp`, `id`) are first-class strings.

### Complexity

Reading + validation is `O(total input length)`; symbol classification is
`O(N·L·(V+T))` with linear membership scans. Memory is fixed-size arrays
(see `constants.h` for the limits).

---

## 2. FIRST computation (first.c)

**Input:** a validated `Grammar`.
**Output:** `FirstCollection` with one `FirstSet` per non-terminal, indexed
exactly like `grammar->nonTerminals[]`; `#` marks nullable non-terminals.

### Algorithm (fixed-point iteration)

1. Initialize every FIRST set to empty.
2. Repeat until nothing changes:
   - For each production `A -> α` and each alternative:
     - `firstOfSequence()` walks the alternative's symbols left to right:
       - terminal → add it, stop;
       - non-terminal → add `FIRST(X) - {#}`; if `X` is **not** nullable,
         stop; otherwise continue to the next symbol;
       - if all symbols were nullable (or the alternative is `#`),
         add `#` to `FIRST(A)`.
   - Track a `changed` flag; loop exits only when a full pass makes no
     addition — the fixed point.

`firstIsNullable()` reads the `#` membership; `firstContains()` answers
membership queries for the parser and table builder; `firstOfSequence()` is
exposed for **Module 4**, which reuses it for `FIRST(α)` of full alternatives.

### Edge cases handled

- Direct recursion (`E -> E + T`): the fixed-point loop adds on later
  iterations instead of recursing infinitely.
- Indirect recursion (`S -> A c`, `A -> S b | #`): converges —
  `FIRST(A) = { #, c }` after the dependency propagates.
- Epsilon propagation through sequences: `S -> A B`, `A -> a | #`,
  `B -> b` gives `FIRST(S) = { a, b }`.
- Pure epsilon grammar (`S -> #`): `FIRST(S) = { # }`.

### Complexity

Each pass is `O(N·L·(V+T))` (membership scans are linear). The loop runs at
most `V·(V+T)` useful iterations because each iteration that changes
anything adds at least one new symbol to some set and the total number of
symbols across all sets is bounded by `V·(V+T)`.

---

## 3. FOLLOW computation (follow.c)

**Input:** a validated `Grammar` **and** a completed `FirstCollection`.
**Output:** `FollowCollection` with one set per non-terminal, indexed like
`nonTerminals[]`. `$` is guaranteed in `FOLLOW(start)`.

### Rules implemented (fixed-point iteration)

1. **Rule 1** — add `$` to `FOLLOW(start)` before the first pass.
2. Repeat until nothing changes; for each production `A -> α B β` (for each
   position where `B` is a non-terminal):
   - **Rule 2** — compute `FIRST(β)` with `firstOfSequence()` and add
     `FIRST(β) - {#}` to `FOLLOW(B)`.
   - **Rule 3** — if `firstIsNullable(β)` (β can derive ε, including β empty
     or β = `#`), add all of `FOLLOW(A)` to `FOLLOW(B)`.
3. Exit when a full pass adds nothing.

`followContains()` answers membership queries for Module 4 (the
ε-production rows of the table) and the parser (sync-set style reporting).

### Edge cases handled

- β empty (`A -> B`) — nullable by definition, so Rule 3 applies fully.
- Recursive dependency (`E -> T Ep`, `Ep -> + T Ep | #`): `FOLLOW(E)` flows
  into `FOLLOW(T)` and `FOLLOW(F)` only after the Ep iteration stabilizes —
  a single pass would not be enough, hence fixed-point.
- Multiple positions: each RHS symbol position is examined independently.

### Complexity

Same shape as FIRST: each pass `O(N·L·(V+T))`, at most `O(V²·(V+T))` work
overall across passes.

---

## 4. Predictive parsing table construction (parsing_table.c)

**Input:** validated `Grammar`, completed `FirstCollection` and
`FollowCollection`.
**Output:** `ParsingTable` — `cells[ntIndex][termIndex]`, each holding
`(lhsIndex, altIndex)` production references (never duplicated production
strings), plus a `conflictCount` list.

### Algorithm

For every production `A -> α` (each alternative independently):

1. Compute `FIRST(α)` with the shared `firstOfSequence()`.
2. For every terminal `a ∈ FIRST(α) - {#}`: place `M[A, a] = (A, α)`.
3. If `# ∈ FIRST(α)`: for every `b ∈ FOLLOW(A)` place `M[A, b] = (A, α)`.

### Conflict detection (no overwriting)

Before writing a cell, `cellOccupied()` is checked:

- empty cell → write the reference;
- occupied by the **same** production (same lhsIndex+altIndex, e.g. the ε
  rule claiming both a FIRST terminal and FOLLOW terminals) → not a
  conflict;
- occupied by a **different** production → keep the existing entry, record
  `(ntIndex, termIndex, firstProd, secondProd)` in `conflicts[]`, and mark
  the grammar not LL(1).

`tableHasEntry()` drives parser error reporting ("no table entry");
`tableEntryToString()` renders `A -> α` from the stored references;
`displayConflicts()` prints each conflict as coordinate + both productions.

### Edge cases handled

- ε-producing alternatives covering FOLLOW terminals;
- the demo grammar's `Ep -> + T Ep | #` cells (FIRST and FOLLOW claims must
  not false-positive against each other);
- duplicate cell claims like `S -> a A | a B` → conflict at `M[S, a]`,
  both productions preserved and reported.

### Complexity

`O(N·L·(V+T) + N·(T+V·T))` — FIRST-of-sequence per alternative plus set
claims; memory `O(V·T)` cells.

---

## 5. Predictive parsing (parser.c)

**Input:** validated `Grammar` + completed `ParsingTable`; input string.
**Output:** step-by-step trace, accept/reject verdict, diagnostic on error.

### Data

- **Stack:** `MAX_STACK_DEPTH` entries, bottom is `$`, initialized to
  `$` then the start symbol.
- **Input:** `parserTokenizeInput()` splits the line on whitespace into
  tokens and appends `$`.
- Guard flags (`stackOverflow`, `inputOverflow`) prevent any
  out-of-bounds write.

### Algorithm (standard LL(1) machine)

1. `X = top of stack`, `a = current token`.
2. If `X == $` and `a == $` → **accept**.
3. If `X == $` but `a != $` → error: unexpected extra input after a complete
   parse.
4. If `X` is a terminal: match iff `X == a` → pop, advance; else →
   *"expected X, found a"* diagnostic.
5. If `X` is a non-terminal: look up `M[X, a]`:
   - entry → pop `X`, push the alternative's symbols **in reverse order**
     (leftmost symbol ends on top), skip `#` entirely (never pushed);
   - no entry → *"no parsing-table entry for [X, a]"* with the expected
     token list gathered from the occupied cells of row `X`.
6. Loop with a step counter; each step prints stack (bottom→top), remaining
   input, and the action taken.

`parserRunWithActions()` runs the identical machine but instead of printing
calls a caller-supplied `ParserActionFn` callback at every shift, expand,
and accept — **Module 6 hooks here** so translation reuses the exact parse
rather than duplicating it.

### Error handling

Every error path returns immediately with a structured verdict; there are
no crashes on arbitrary input and no infinite loops (each accepted error
terminates, each step either consumes input, pops the stack, or terminates).

### Complexity

`O(n)` steps for an `O(n)`-token input (each step advances the input or the
stack toward termination), with `O(1)` table lookup.

---

## 6. Syntax-directed translation (translation.c)

**Input:** validated `Grammar`, `ParsingTable`, input string.
**Output:** `TranslationResult` — postfix string, three-address code lines,
error status.

### Scheme (schematic, evaluated during the LL(1) parse)

Semantic stack holds operand names (identifiers, temps, operators):

| Production | Action (runs when the production is applied) |
|---|---|
| `Ep -> + T Ep` | pop the operator `+` and two operands; emit `t = a + b`; push `t` |
| `Ep -> #` | none |
| `Tp -> * F Tp` | same pattern for `*` |
| `Tp -> #` | none |
| `F -> ( E )` | no code; result of `E` stays on the stack |
| `F -> id` | push the identifier |
| accept | stack top (or empty for empty input) is the expression result |

The mechanism is **`parserRunWithActions()`** (Module 5): `translateInput()`
supplies a callback that inspects which production the parser applied and
executes the matching action at expansion time. Postfix is emitted by
appending terminals as they are matched, operators when their production is
applied — no second parse, no hardcoded answers.

`translationNewTemp()` generates `t1, t2, ...` for TAC;
`translationEmit()` records `t = a op b` lines;
`displayTranslation()` prints both sections.

### Edge cases handled

- Single operand (`a`) → postfix `a`, no TAC lines.
- Empty input → empty translation.
- Overflow of the semantic stack / temp counter → error status, not
  corruption.
- Any input the parser rejects → translation aborts with the parse error.

### Complexity

`O(n)` — proportional to the number of parse actions (which is `O(n)`).

---

## 7. End-to-end integration (main.c)

File mode (`./ll1 grammar.txt`) runs the automatic pipeline: load → validate
→ display → FIRST → FOLLOW → table → LL(1) check → parse/translate lines
read from stdin until EOF. Menu mode (no arguments) exposes options 1–9 and
keeps the grammar in memory between steps. Grammar files can also come from
stdin redirection (`./ll1 - < grammar.txt`).
