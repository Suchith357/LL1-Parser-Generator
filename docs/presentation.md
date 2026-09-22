# Presentation — LL(1) Parser Generator (Team 4)

Ready-to-present slide content for Review 1, Review 2, and the final
demonstration. One block = one slide; *speaker notes in italics* under each.

---

## Slide 1 — Title

**Design and Implementation of an LL(1) Parser Generator with Automatic
FIRST-FOLLOW Computation, Predictive Parsing Table Construction and
Syntax-Directed Translation**

- Course: Compiler Design · Team 4
- Members: 24bce0092 · 24bde0155 · 24bce0556 · 24bci0103 · 24BDS0044

*Notes: state the locked title exactly; introduce members with their module
(see Slide 13).*

---

## Slide 2 — Problem Statement

- Compilers need a **syntax analyzer** driven by a grammar, not by ad-hoc code.
- Hand-writing a parser for each grammar is error-prone and ungeneralizable.
- **Goal:** a *generator* — input ANY grammar in a text format, get
  FIRST/FOLLOW sets, a predictive parsing table, a working parser, and a
  translation of accepted inputs.

*Notes: stress the word GENERATOR — the tool works for multiple grammars,
nothing is hardcoded for one example.*

---

## Slide 3 — Objectives

1. Read and validate a context-free grammar from file/stdin/keyboard.
2. Automatically identify terminals and non-terminals.
3. Compute FIRST sets automatically (fixed point, epsilon-aware).
4. Compute FOLLOW sets automatically (all three rules, fixed point).
5. Construct the LL(1) predictive parsing table automatically.
6. Detect LL(1) conflicts and report them precisely.
7. Parse input strings table-driven, with a step trace.
8. Perform syntax-directed translation (postfix + three-address code).

---

## Slide 4 — Scope

**In scope:** CFG input in the documented format · validation · symbol
classification · FIRST · FOLLOW · LL(1) table · conflict detection ·
predictive parsing with diagnostics · syntax-directed translation ·
automated test suite.

**Out of scope (honest):** lexical analysis beyond tokenization · semantic
type checking · grammar transformations (no automatic left-recursion
elimination / left factoring) · LR/LALR parsing · GUI.

*Notes: stating out-of-scope items shows maturity; faculty rewards honesty —
do not claim transformations we never built.*

---

## Slide 5 — Grammar / Language Specification

```
E -> T Ep
Ep -> + T Ep | #
T -> F Tp
Tp -> * F Tp | #
F -> ( E ) | id
```

- `->` separates LHS/RHS · `|` alternatives · `#` epsilon
- symbols space-separated · first LHS = start symbol
- symbols are strings (max 31 chars): `Ep`, `Tp`, `id` work
- `$` reserved as end marker · undefined non-terminals rejected

*Notes: full rules live in docs/input_format.md and PROJECT_SPECIFICATION.txt
sections D–K.*

---

## Slide 6 — Architecture

```
CFG input → validate → identify symbols → FIRST → FOLLOW
          → parsing table → LL(1) conflict detection
          → [LL(1)?] → predictive parsing → syntax-directed translation
```

- 6 modules + shared core, one .c/.h pair each, one owner each
- shared: constants.h (all limits), utils.h (StringSet, SymbolTable)
- table stores production REFERENCES, not copied text
- parser exposes SHIFT/REDUCE/ACCEPT hooks; SDT attaches to them

*Notes: this is the "core algorithm/technique design" slide — 5 marks at
Review 1. Diagram available in docs/design.md.*

---

## Slide 7 — Data Structures

- `Production` — LHS + array of RHS alternative strings
- `Grammar` — productions + classified symbol lists + start symbol
- `FirstSet` / `FollowSet` — arrays of symbol strings, indexed by
  non-terminal order
- `TableCell { lhsIndex, altIndex }` — reference into the production array
- `ParserState` — stack + tokenized input + lookahead + step counter
- `SemanticStack` — postfix fragments reduced by SDT actions

*Notes: emphasize string symbols (MAX_SYMBOL_LENGTH 32) — the early
single-char design could not even hold the demo grammar.*

---

## Slide 8 — FIRST Computation  (Member 2)

FIRST(X) = terminals that can start a string derived from X (plus `#` if
X ⇒* ε).

```
repeat until no change:
  for each production A -> X1 X2 ... Xn:
    add FIRST(X1) \ {#}
    if X1 nullable → also add FIRST(X2) \ {#} ... and so on
    if ALL Xi nullable → add # to FIRST(A)
```

- fixed point ⇒ handles direct AND indirect recursion
- `firstOfSequence()` reused by FOLLOW and the table — one implementation

Verified on the demo grammar:
FIRST(E)={ (, id } · FIRST(Ep)={ +, # } · FIRST(T)={ (, id } ·
FIRST(Tp)={ *, # } · FIRST(F)={ (, id }

---

## Slide 9 — FOLLOW Computation  (Member 3)

FOLLOW(A) = terminals that can appear immediately right of A (plus `$` for
the start symbol).

- **Rule 1:** `$` ∈ FOLLOW(start)
- **Rule 2:** A → α B β ⇒ add FIRST(β)\{#} to FOLLOW(B)
- **Rule 3:** β nullable (or absent) ⇒ add FOLLOW(A) to FOLLOW(B)
- iterate to fixed point — handles cycles like FOLLOW(E)↔FOLLOW(Ep)

Verified: FOLLOW(E)={ ), $ } · FOLLOW(T)={ +, ), $ } ·
FOLLOW(F)={ *, +, ), $ }

*Notes: explain why $ must be in FOLLOW(start): a complete derivation is
followed by nothing except end-of-input.*

---

## Slide 10 — Parsing Table + LL(1) Conflicts  (Member 4)

For each production A → α:
- for every terminal a ∈ FIRST(α)\{#} → M[A, a] = A → α
- if # ∈ FIRST(α) → for every b ∈ FOLLOW(A) → M[A, b] = A → α

**Conflict = two productions claim one cell.** We keep the first, record
both, report the cell — never overwrite.

Demo (S → a A | a B): `Conflict detected at M[S, a]` → **NOT LL(1)**.
Demo grammar: all cells single → **Grammar is LL(1).**

*Notes: parsing/translation are refused for non-LL(1) grammars — a design
decision worth saying aloud.*

---

## Slide 11 — Predictive Parser  (Member 5)

Stack + input + table loop:

```
$ E            id + id $       E -> T Ep
$ Ep T         id + id $       T -> F Tp
$ Ep Tp F      id + id $       F -> id
...                            (match id, ε-productions, ...)
$              $               accept
```

- push RHS in reverse · epsilon pushes nothing · accept at `$ $`
- diagnostics with token position: e.g. `id + * id` →
  "No parsing-table entry for [T, *] — Expected one of: id, ("
- safety: step cap + bounded stack ⇒ no infinite loops, no crashes

---

## Slide 12 — Syntax-Directed Translation  (Member 5)

Semantic actions fire on parser events (SHIFT / REDUCE / ACCEPT):

```
Ep -> + T Ep   { print T.val, Ep1.val, then '+' }
Tp -> * F Tp   { print F.val, Tp1.val, then '*' }
F  -> id       { print the token }
...
```

Input `a + b * c` →

```
Postfix:            a b c * +
Three-address code: t1 = b * c
                    t2 = a + t1
```

*Notes: actions sit at the END of productions ⇒ all attributes synthesized
⇒ safe for LL(1). TAC is generated from the postfix by stack evaluation.*

---

## Slide 13 — Team Contributions

| Member | Module | Files |
|---|---|---|
| 24bce0092 | Grammar processing + validation | grammar.h/.c |
| 24bde0155 | FIRST computation | first.h/.c |
| 24bce0556 | FOLLOW computation | follow.h/.c |
| 24bci0103 | Parsing table + LL(1) conflicts | parsing_table.h/.c |
| 24BDS0044 | Parser + syntax-directed translation | parser.h/.c, translation.h/.c |
| all | shared core, integration, tests, docs | constants.h, utils.h, main.c, testcases/ |

*Notes: each member commits their own module on feature/* branches — the
git history must show this.*

---

## Slide 14 — Testing

- Automated suite: `make test` → 19 checks, PASS/FAIL per test
- Categories: valid · invalid · epsilon · pure-epsilon · non-LL(1) ·
  indirect recursion · valid/invalid inputs · edge syntax
- Examples:
  - `id + id * id` → ACCEPTED (full trace)
  - `id + * id` → positioned syntax error
  - `S -> a A | a B` → conflict at M[S, a]
  - `S -> A` / `A -> b X` → "X is used but never defined"

*Notes: testcases/ folder mirrors the report's test table — same IDs.*

---

## Slide 15 — Demo Script (live)

1. `make` → clean build, zero warnings
2. `make run` → full pipeline on the standard grammar
3. point at FIRST/FOLLOW output = course-expected sets
4. point at the parsing table + "Grammar is LL(1)."
5. show trace of `id + id * id` → INPUT ACCEPTED
6. show postfix + TAC of `a + b * c`
7. `make test` → 19/19 PASS
8. show a rejected grammar (undefined non-terminal) + a conflict report

---

## Slide 16 — Limitations & Future Work

Limitations: fixed-size structures (documented limits) · no grammar
transformations · line-oriented grammar format · LL(1) only.

Future: automatic left-recursion elimination / left factoring · more
translation schemes (infix→prefix, AST construction) · richer error
recovery · configurable limits.

*Notes: never claim a future item as present — the report must match
reality (plagiarism/AI rules also require this).*

---

## Slide 17 — Conclusion

- A genuine, generic LL(1) parser generator in C — not a demo for one grammar
- All six stages implemented, integrated, and automated-tested
- Every result computed from the input grammar; nothing hardcoded
- Each member owns a module end-to-end: code + tests + docs + viva

---

## Viva Quick Answers (appendix slide — keep hidden, use if asked)

- **Why LL(1)?** one lookahead suffices to choose deterministically; table
  gives O(n) parsing without backtracking.
- **Why is $ in FOLLOW(start)?** end-of-input follows every complete
  derivation; the parser accepts exactly at stack-$ + input-$.
- **What causes a conflict?** two alternatives indistinguishable with one
  lookahead (common prefix without factoring, or left recursion).
- **Why a stack?** it holds the predicted RHS symbols still to be matched —
  the leftmost derivation in reverse.
- **Syntax analysis vs semantic actions?** analysis decides *shape*
  (accept/reject); SDT computes *meaning* (translation) during the same
  scan.
- **How does the parser detect errors?** terminal mismatch, or empty table
  cell M[X, a] — both with position and expectation in our diagnostics.
