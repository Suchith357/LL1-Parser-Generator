/*
 * first.c - Module 2 (Member 2: 24bde0155)
 *
 * Automatic FIRST set computation by fixed-point iteration.
 *
 * Algorithm
 * ---------
 * 1. Start with FIRST(A) = {} for every non-terminal A.
 * 2. For every production A -> X1 X2 ... Xn (one alternative):
 *      - if X1 is a terminal:  add X1 to FIRST(A)
 *      - if X1 is a non-terminal: add FIRST(X1)\{#} to FIRST(A);
 *        if X1 is nullable ( # in FIRST(X1) ) also consider X2, and so on
 *      - if every Xi is nullable (or n == 0), add # to FIRST(A)
 * 3. Repeat step 2 until no FIRST set changes (fixed point).
 *
 * The loop terminates because FIRST sets only ever grow and are bounded
 * by the terminal alphabet plus "#".
 */

#include <stdio.h>
#include <string.h>

#include "first.h"

/* ======================================================================
 * Small helpers
 * ====================================================================== */

void initializeFirstCollection(FirstCollection *fc) {
    fc->setCount = 0;
}

static void firstSetInit(FirstSet *set) {
    set->count = 0;
    set->hasEpsilon = 0;
}

static int firstSetContains(const FirstSet *set, const char *symbol) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->elements[i], symbol) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Adds one element; returns 1 if the set changed. Keeps hasEpsilon in
 * sync when the element is "#". */
static int firstSetAdd(FirstSet *set, const char *symbol) {
    if (firstSetContains(set, symbol)) {
        return 0;
    }
    if (set->count >= MAX_SET_ELEMENTS) {
        fprintf(stderr, "first: set overflow (max %d)\n", MAX_SET_ELEMENTS);
        return 0;
    }
    snprintf(set->elements[set->count], MAX_SYMBOL_LENGTH, "%s", symbol);
    set->count++;
    if (strcmp(symbol, "#") == 0) {
        set->hasEpsilon = 1;
    }
    return 1;
}

static int firstIndexOf(const FirstCollection *fc, const char *nonTerminal) {
    for (int i = 0; i < fc->setCount; i++) {
        if (strcmp(fc->sets[i].symbolName, nonTerminal) == 0) {
            return i;
        }
    }
    return INVALID_INDEX;
}

/* ======================================================================
 * Public queries
 * ====================================================================== */

int firstIsNullable(const FirstCollection *fc, const char *nonTerminal) {
    int idx = firstIndexOf(fc, nonTerminal);
    return idx != INVALID_INDEX && fc->sets[idx].hasEpsilon;
}

int firstContains(const FirstCollection *fc, const char *nonTerminal,
                  const char *terminal, int checkEpsilon) {
    int idx = firstIndexOf(fc, nonTerminal);
    if (idx == INVALID_INDEX) {
        return 0;
    }
    if (checkEpsilon && strcmp(terminal, "#") == 0) {
        return fc->sets[idx].hasEpsilon;
    }
    return firstSetContains(&fc->sets[idx], terminal);
}

/* ======================================================================
 * FIRST of a sequence (public helper used by FOLLOW and the table)
 * ====================================================================== */

void firstOfSequence(const Grammar *grammar,
                     const FirstCollection *fc,
                     const char *rhsAlt,
                     FirstSet *out) {
    firstSetInit(out);

    int symbolCount = productionAltSymbolCount(rhsAlt);
    int allNullable = 1;

    for (int i = 0; i < symbolCount; i++) {
        char token[MAX_SYMBOL_LENGTH];
        if (!productionAltGetSymbol(rhsAlt, i, token)) {
            break;
        }

        if (strcmp(token, "#") == 0) {
            /* Whole alternative is epsilon. */
            firstSetAdd(out, "#");
            return;
        }

        if (!grammarIsNonTerminal(grammar, token)) {
            /* Terminal: contributes itself, blocks epsilon. */
            firstSetAdd(out, token);
            allNullable = 0;
            break;
        }

        /* Non-terminal: union FIRST(token) \ {#}; continue past it
         * only when token is nullable. */
        int idx = firstIndexOf(fc, token);
        if (idx == INVALID_INDEX) {
            /* Undefined symbol should have been rejected by validation;
             * treat as blocking to stay safe. */
            allNullable = 0;
            break;
        }
        const FirstSet *source = &fc->sets[idx];
        for (int j = 0; j < source->count; j++) {
            if (strcmp(source->elements[j], "#") != 0) {
                firstSetAdd(out, source->elements[j]);
            }
        }
        if (!source->hasEpsilon) {
            allNullable = 0;
            break;
        }
    }

    if (allNullable && symbolCount > 0) {
        firstSetAdd(out, "#");
    }
}

/* ======================================================================
 * Fixed-point computation
 * ====================================================================== */

static void initializeSetsForNonTerminals(const Grammar *grammar,
                                          FirstCollection *fc) {
    fc->setCount = 0;
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        FirstSet *set = &fc->sets[fc->setCount];
        firstSetInit(set);
        snprintf(set->symbolName, MAX_SYMBOL_LENGTH, "%s",
                 grammar->nonTerminals[i]);
        fc->setCount++;
    }
}

/* Processes one alternative A -> alpha against FIRST(A).
 * Returns 1 if FIRST(A) changed. */
static int addFirstOfAlternative(const Grammar *grammar,
                                 FirstCollection *fc,
                                 const char *lhs,
                                 const char *rhsAlt) {
    int lhsIdx = firstIndexOf(fc, lhs);
    if (lhsIdx == INVALID_INDEX) {
        return 0;
    }
    FirstSet *target = &fc->sets[lhsIdx];

    int changed = 0;
    int symbolCount = productionAltSymbolCount(rhsAlt);
    int allNullable = 1;

    for (int i = 0; i < symbolCount; i++) {
        char token[MAX_SYMBOL_LENGTH];
        if (!productionAltGetSymbol(rhsAlt, i, token)) {
            break;
        }

        if (strcmp(token, "#") == 0) {
            changed |= firstSetAdd(target, "#");
            return changed;
        }

        if (!grammarIsNonTerminal(grammar, token)) {
            changed |= firstSetAdd(target, token);
            allNullable = 0;
            break;
        }

        int srcIdx = firstIndexOf(fc, token);
        if (srcIdx == INVALID_INDEX) {
            allNullable = 0;
            break;
        }
        FirstSet *source = &fc->sets[srcIdx];
        for (int j = 0; j < source->count; j++) {
            if (strcmp(source->elements[j], "#") != 0) {
                changed |= firstSetAdd(target, source->elements[j]);
            }
        }
        if (!source->hasEpsilon) {
            allNullable = 0;
            break;
        }
    }

    if (allNullable) {
        changed |= firstSetAdd(target, "#");
    }
    return changed;
}

int computeFirstSets(const Grammar *grammar, FirstCollection *fc) {
    if (grammar->nonTerminalCount == 0 || grammar->productionCount == 0) {
        fprintf(stderr,
                "first: grammar has no symbols; run identifySymbols() first\n");
        return 0;
    }

    initializeFirstCollection(fc);
    initializeSetsForNonTerminals(grammar, fc);

    int changed = 1;
    while (changed) {
        changed = 0;
        for (int i = 0; i < grammar->productionCount; i++) {
            const Production *prod = &grammar->productions[i];
            for (int j = 0; j < prod->rhsCount; j++) {
                if (addFirstOfAlternative(grammar, fc, prod->lhs,
                                          prod->rhs[j])) {
                    changed = 1;
                }
            }
        }
    }
    return 1;
}

/* ======================================================================
 * Display
 * ====================================================================== */

void displayFirstSets(const FirstCollection *fc) {
    printf("\n========== FIRST SETS ==========\n");
    for (int i = 0; i < fc->setCount; i++) {
        const FirstSet *set = &fc->sets[i];
        printf("FIRST(%s) = { ", set->symbolName);
        for (int j = 0; j < set->count; j++) {
            printf("%s", set->elements[j]);
            if (j < set->count - 1) {
                printf(", ");
            }
        }
        printf(" }\n");
    }
    printf("================================\n");
}
