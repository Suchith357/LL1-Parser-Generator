#ifndef FIRST_H
#define FIRST_H

/*
 * first.h - Module 2 (Member 2: 24bde0155)
 *
 * Automatic FIRST set computation.
 *
 * FIRST(X) is the set of terminals that can appear at the beginning of
 * any string derived from X, plus epsilon ("#") when X can derive the
 * empty string. The implementation iterates to a fixed point, so it
 * handles direct/indirect recursion and epsilon propagation:
 *
 *   FIRST(a) = {a}                       (terminal a)
 *   FIRST(X) includes FIRST(Y1)          (production X -> Y1 Y2 ...,
 *     minus "#" if Y1 is non-nullable)   Y1 a non-terminal)
 *   and continues through Y2, Y3 ... as long as earlier symbols are
 *   nullable; if every Yk is nullable, "#" is added.
 */

#include "grammar.h"

typedef struct {
    /* Non-terminal this set belongs to (matches grammar->nonTerminals[i]). */
    char symbolName[MAX_SYMBOL_LENGTH];
    /* elements[0..count-1] are terminal names, or "#". */
    char elements[MAX_SET_ELEMENTS][MAX_SYMBOL_LENGTH];
    int  count;
    /* 1 iff "#" is in the set (kept in sync with elements). */
    int  hasEpsilon;
} FirstSet;

typedef struct {
    /* sets[i] corresponds to grammar->nonTerminals[i] by construction. */
    FirstSet sets[MAX_SYMBOLS];
    int      setCount;
} FirstCollection;

void initializeFirstCollection(FirstCollection *fc);

/* Computes FIRST sets for every non-terminal of the grammar by fixed-
 * point iteration. Returns 1 on success, 0 if the grammar was not
 * validated (identifySymbols must have run first). */
int  computeFirstSets(const Grammar *grammar, FirstCollection *fc);

/* FIRST of a single sequence of symbols (a whole RHS alternative).
 * Fills `out` including "#" when the whole sequence is nullable.
 * `tempEpsilonWork` may be NULL; it is a scratch set for recursion. */
void firstOfSequence(const Grammar *grammar,
                     const FirstCollection *fc,
                     const char *rhsAlt,
                     FirstSet *out);

/* 1 iff "#" is in FIRST(X) for non-terminal X (by name). */
int  firstIsNullable(const FirstCollection *fc, const char *nonTerminal);

/* 1 iff terminal `terminal` (or "#" when checkEpsilon is 1) is in
 * FIRST(X) of non-terminal X (by name). */
int  firstContains(const FirstCollection *fc, const char *nonTerminal,
                   const char *terminal, int checkEpsilon);

void displayFirstSets(const FirstCollection *fc);

#endif /* FIRST_H */
