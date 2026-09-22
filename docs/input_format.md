# Grammar Input Format

## Rules

- One production group per line: `LHS -> ALT1 | ALT2 | ...`
- `->` separates the left-hand side from the right-hand side (exactly one
  `->` per line).
- `|` separates alternatives of the same LHS.
- `#` denotes epsilon and must be the **only** symbol of its alternative.
- Symbols are separated by one or more spaces. `E->T Ep` (no spaces
  around the arrow) is accepted because the arrow is not a symbol; but
  `( E )` must still be spaced — `(E)` would be read as ONE terminal.
  Symbols themselves may never contain spaces.
- The LHS of the **first** production is the start symbol.
- The end marker `$` is reserved and may not appear in a grammar.

## Symbol names

A symbol is 1..31 characters from alphanumerics, `_` and the operator
characters `+ - * / ( ) =`.

Convention used in this project:

- **Non-terminals** start with an uppercase letter (`E`, `Ep`, `Expr`).
  A name that starts uppercase but has no production with it as LHS is
  rejected with *"non-terminal 'X' is used but never defined"* — this
  catches typos early instead of failing inside FIRST computation.
- **Terminals** are everything else: `id`, `num`, `+`, `*`, `(`, `)`, ...

## Example (locked demo grammar)

```
E -> T Ep
Ep -> + T Ep | #
T -> F Tp
Tp -> * F Tp | #
F -> ( E ) | id
```

Compact-arrow variants such as `Ep -> + T Ep | #` may also be written
`Ep->+ T Ep|#`.

## Providing the grammar

```
./ll1                 # interactive: prompts for count, then productions
./ll1 grammar.txt     # read productions from a file (blank lines ignored)
./ll1 - < grammar.txt # read from stdin
```

## Rejected inputs (with diagnostics)

| Input | Diagnostic |
|---|---|
| `E T Ep` | missing '->' separator |
| `E -> a -> b` | more than one '->' separator |
| `E -> | a` | empty alternative (check '|' placement) |
| `E -> a b #` | '#' must be the only symbol of its alternative |
| `E -> a $` | invalid symbol '$' |
| `E -> a$b` | invalid symbol 'a$b' |

(`$` is rejected because it is not a legal symbol character — this keeps
the end marker out of grammars.)
| `E -> T X` | non-terminal 'X' is used but never defined |
| `E -> a#b` | invalid symbol 'a#b' |
