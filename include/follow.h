#ifndef FOLLOW_H
#define FOLLOW_H

/*
 * follow.h - Module 3 (Member 3: 24bce0556)
 *
 * Automatic FOLLOW set computation.
 *
 * FOLLOW(A) is the set of terminals that can appear immediately to the
 * right of A in some sentential form, plus "$" when A can be the last
 * symbol before end-of-input. Rules applied at each iteration:
 *
 *   Rule 1: FOLLOW(start) contains "$".
 *   Rule 2: for A -> ... B beta, add FIRST(beta) \ {#} to FOLLOW(B).
 *   Rule 3: for A -> ... B (beta nullable or absent),
 *           add FOLLOW(A) to FOLLOW(B).
 *
 * The loop repeats until no FOLLOW set changes (fixed point), which
 * handles indirect and mutual dependencies such as
 * FOLLOW(E) -> FOLLOW(Ep) -> FOLLOW(E) in the demo grammar.
 */

#include "grammar.h"
#include "first.h"
#include <stddef.h>

typedef struct {
    /* Non-terminal this set belongs to (matches grammar->nonTerminals[i]).
     * Added during integration so followContains() can address sets by
     * name, mirroring FirstSet.symbolName. */
    char symbolName[MAX_SYMBOL_LENGTH];
    /* Terminals plus "$". elements[0..count-1]. */
    char elements[MAX_SET_ELEMENTS][MAX_SYMBOL_LENGTH];
    int  count;
} FollowSet;

typedef struct {
    /* sets[i] corresponds to grammar->nonTerminals[i]. */
    FollowSet sets[MAX_SYMBOLS];
    int       setCount;
} FollowCollection;

void initializeFollowCollection(FollowCollection *fc);

/* Computes FOLLOW sets by fixed-point iteration. Requires FIRST sets
 * (for Rule 2) and a validated grammar (identifySymbols already run).
 * Returns 1 on success, 0 if inputs are missing/inconsistent. */
int  computeFollowSets(const Grammar *grammar,
                       const FirstCollection *first,
                       FollowCollection *fc);

/* 1 iff terminal (or "$") is in FOLLOW(X) for non-terminal X by name. */
int  followContains(const FollowCollection *fc, const char *nonTerminal,
                    const char *terminal);

void displayFollowSets(const FollowCollection *fc);

#endif /* FOLLOW_H */
