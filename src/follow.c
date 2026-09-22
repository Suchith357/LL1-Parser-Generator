/*
 * follow.c - Module 3 (Member 3: 24bce0556)
 *
 * Automatic FOLLOW set computation by fixed-point iteration.
 *
 * FOLLOW(A) is the set of terminals that can appear immediately to the
 * right of A in some sentential form, plus the end marker "$" when A can
 * end a complete derivation. Three rules are applied on every sweep:
 *
 *   Rule 1: "$" belongs to FOLLOW(start symbol).
 *   Rule 2: for every occurrence  A -> alpha B beta,
 *           add  FIRST(beta) \ {#}  to FOLLOW(B).
 *   Rule 3: for every occurrence  A -> alpha B beta  where beta is
 *           nullable (or absent), add FOLLOW(A) to FOLLOW(B).
 *
 * Sweeps repeat until no set changes (fixed point), which correctly
 * propagates indirect and mutual dependencies such as the cycle
 * FOLLOW(E) -> FOLLOW(Ep) -> FOLLOW(E) in the demo grammar.
 *
 * Termination: FOLLOW sets only grow and are bounded by the terminal
 * alphabet plus "$", so at most O(N * T) sweeps occur.
 *
 * FIRST of the trailing sequence beta is computed with the shared
 * firstOfSequence() helper from Module 2 - no logic is duplicated here.
 */

#include <stdio.h>
#include <string.h>

#include "follow.h"

/* ======================================================================
 * Small set helpers
 * ====================================================================== */

static void followSetInit(FollowSet *set) {
    set->count = 0;
}

void initializeFollowCollection(FollowCollection *fc) {
    fc->setCount = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        followSetInit(&fc->sets[i]);
        fc->sets[i].symbolName[0] = '\0';
    }
}

static int followSetContains(const FollowSet *set, const char *symbol) {
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->elements[i], symbol) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Returns 1 when the element is new (set changed). */
static int followSetAdd(FollowSet *set, const char *symbol) {
    if (followSetContains(set, symbol)) {
        return 0;
    }
    if (set->count >= MAX_SET_ELEMENTS) {
        fprintf(stderr, "follow: set overflow (max %d elements)\n",
                MAX_SET_ELEMENTS);
        return 0;
    }
    snprintf(set->elements[set->count], MAX_SYMBOL_LENGTH, "%s", symbol);
    set->count++;
    return 1;
}

/* Union src into dst; returns 1 when dst changed. */
static int followSetAddAll(FollowSet *dst, const FollowSet *src) {
    int changed = 0;
    for (int i = 0; i < src->count; i++) {
        changed |= followSetAdd(dst, src->elements[i]);
    }
    return changed;
}

static int followIndexOf(const FollowCollection *fc,
                         const char *nonTerminal) {
    for (int i = 0; i < fc->setCount; i++) {
        if (strcmp(fc->sets[i].symbolName, nonTerminal) == 0) {
            return i;
        }
    }
    return INVALID_INDEX;
}

int followContains(const FollowCollection *fc, const char *nonTerminal,
                   const char *terminal) {
    int idx = followIndexOf(fc, nonTerminal);
    if (idx == INVALID_INDEX) {
        return 0;
    }
    return followSetContains(&fc->sets[idx], terminal);
}

/* ======================================================================
 * Beta construction
 * ====================================================================== */

/* Builds the trailing sequence "beta" (all symbols after position k of
 * the alternative) into out. The alternative itself is bounded by
 * MAX_RHS_LENGTH, so any tail of it fits as well; the length check is
 * defensive. Returns 1 on success, 0 if the buffer would overflow. */
static int buildBeta(const char *rhsAlt, int k, char *out, size_t outSize) {
    size_t used = 0;
    out[0] = '\0';
    int n = productionAltSymbolCount(rhsAlt);
    for (int m = k + 1; m < n; m++) {
        char sym[MAX_SYMBOL_LENGTH];
        if (!productionAltGetSymbol(rhsAlt, m, sym)) {
            break;
        }
        size_t len = strlen(sym);
        if (used + len + 2 > outSize) {
            return 0;
        }
        if (used > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, sym, len + 1);
        used += len;
    }
    return 1;
}

/* ======================================================================
 * Display ordering
 * ====================================================================== */

/* Rank for display: grammar terminal order, "$" last, anything else
 * after that (cannot normally happen). Sorting is display-only; it does
 * not change set membership, and it makes the printed sets match the
 * course's expected output exactly. */
static int followRank(const Grammar *grammar, const char *symbol) {
    char endMarker[2] = { END_MARKER, '\0' };
    if (strcmp(symbol, endMarker) == 0) {
        return grammar->terminalCount;
    }
    int idx = grammarTerminalIndex(grammar, symbol);
    if (idx == INVALID_INDEX) {
        return grammar->terminalCount + 1;
    }
    return idx;
}

static void sortFollowSet(const Grammar *grammar, FollowSet *set) {
    for (int i = 1; i < set->count; i++) {
        char key[MAX_SYMBOL_LENGTH];
        snprintf(key, MAX_SYMBOL_LENGTH, "%s", set->elements[i]);
        int keyRank = followRank(grammar, key);
        int j = i - 1;
        while (j >= 0 && followRank(grammar, set->elements[j]) > keyRank) {
            snprintf(set->elements[j + 1], MAX_SYMBOL_LENGTH, "%s",
                     set->elements[j]);
            j--;
        }
        snprintf(set->elements[j + 1], MAX_SYMBOL_LENGTH, "%s", key);
    }
}

/* ======================================================================
 * Fixed-point computation
 * ====================================================================== */

int computeFollowSets(const Grammar *grammar,
                      const FirstCollection *first,
                      FollowCollection *fc) {
    if (grammar->nonTerminalCount == 0 || grammar->productionCount == 0) {
        fprintf(stderr,
                "follow: grammar has no symbols; run identifySymbols() first\n");
        return 0;
    }
    if (first->setCount != grammar->nonTerminalCount) {
        fprintf(stderr,
                "follow: FIRST collection does not match the grammar "
                "(compute FIRST sets first)\n");
        return 0;
    }

    /* One empty FOLLOW set per non-terminal, in grammar order. */
    initializeFollowCollection(fc);
    fc->setCount = grammar->nonTerminalCount;
    for (int i = 0; i < fc->setCount; i++) {
        followSetInit(&fc->sets[i]);
        snprintf(fc->sets[i].symbolName, MAX_SYMBOL_LENGTH, "%s",
                 grammar->nonTerminals[i]);
    }

    /* Rule 1: the start symbol is followed by the end marker. */
    char endMarker[2] = { END_MARKER, '\0' };
    int startIdx = followIndexOf(fc, grammar->startSymbol);
    if (startIdx == INVALID_INDEX) {
        fprintf(stderr, "follow: start symbol '%s' is not a non-terminal\n",
                grammar->startSymbol);
        return 0;
    }
    followSetAdd(&fc->sets[startIdx], endMarker);

    /* Fixed point: sweep every production until nothing changes. */
    int changed = 1;
    FirstSet firstOfBeta;
    char beta[MAX_RHS_LENGTH];

    while (changed) {
        changed = 0;
        for (int p = 0; p < grammar->productionCount; p++) {
            const Production *prod = &grammar->productions[p];
            int lhsIdx = followIndexOf(fc, prod->lhs);
            if (lhsIdx == INVALID_INDEX) {
                continue;
            }

            for (int a = 0; a < prod->rhsCount; a++) {
                const char *alt = prod->rhs[a];
                if (productionAltIsEpsilon(alt)) {
                    continue; /* no symbols, so no B to the left of beta */
                }
                int n = productionAltSymbolCount(alt);

                for (int k = 0; k < n; k++) {
                    char B[MAX_SYMBOL_LENGTH];
                    if (!productionAltGetSymbol(alt, k, B)) {
                        break;
                    }
                    if (!grammarIsNonTerminal(grammar, B)) {
                        continue; /* Rules 2/3 apply to non-terminals only */
                    }
                    int bIdx = followIndexOf(fc, B);
                    if (bIdx == INVALID_INDEX) {
                        continue;
                    }
                    if (!buildBeta(alt, k, beta, sizeof(beta))) {
                        continue;
                    }

                    if (beta[0] == '\0') {
                        /* B is the last symbol: beta is absent. */
                        changed |= followSetAddAll(&fc->sets[bIdx],
                                                   &fc->sets[lhsIdx]);
                    } else {
                        firstOfSequence(grammar, first, beta, &firstOfBeta);
                        /* Rule 2: FIRST(beta) \ {#}. */
                        for (int e = 0; e < firstOfBeta.count; e++) {
                            if (strcmp(firstOfBeta.elements[e], "#") != 0) {
                                changed |= followSetAdd(&fc->sets[bIdx],
                                                        firstOfBeta.elements[e]);
                            }
                        }
                        /* Rule 3: beta is nullable. */
                        if (firstOfBeta.hasEpsilon) {
                            changed |= followSetAddAll(&fc->sets[bIdx],
                                                       &fc->sets[lhsIdx]);
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < fc->setCount; i++) {
        sortFollowSet(grammar, &fc->sets[i]);
    }
    return 1;
}

/* ======================================================================
 * Display
 * ====================================================================== */

void displayFollowSets(const FollowCollection *fc) {
    printf("\n========== FOLLOW SETS ==========\n");
    for (int i = 0; i < fc->setCount; i++) {
        printf("FOLLOW(%s) = { ", fc->sets[i].symbolName);
        for (int j = 0; j < fc->sets[i].count; j++) {
            printf("%s", fc->sets[i].elements[j]);
            if (j < fc->sets[i].count - 1) {
                printf(", ");
            }
        }
        printf(" }\n");
    }
    printf("=================================\n");
}
