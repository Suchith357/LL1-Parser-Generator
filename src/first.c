#include <stdio.h>
#include "../include/first.h"

void initializeFirstCollection(FirstCollection *firstCollection) {
    firstCollection->setCount = 0;
}

int getFirstSetIndex(const FirstCollection *firstCollection, char symbol) {
    for (int i = 0; i < firstCollection->setCount; i++) {
        if (firstCollection->sets[i].symbol == symbol) {
            return i;
        }
    }

    return -1;
}

int firstSetContains(const FirstSet *set, char symbol) {
    for (int i = 0; i < set->count; i++) {
        if (set->elements[i] == symbol) {
            return 1;
        }
    }

    return 0;
}

void addToFirstSet(FirstSet *set, char symbol) {
    if (!firstSetContains(set, symbol) &&
        set->count < MAX_FIRST_SET) {

        set->elements[set->count++] = symbol;
    }
}

static void initializeSetsForNonTerminals(
    const Grammar *grammar,
    FirstCollection *firstCollection
) {
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        FirstSet *set =
            &firstCollection->sets[firstCollection->setCount++];

        set->symbol = grammar->nonTerminals[i];
        set->count = 0;
    }
}

static int addFirstOfSequence(
    const char *rhs,
    FirstCollection *firstCollection,
    FirstSet *target
) {
    int changed = 0;
    int allCanProduceEpsilon = 1;

    for (int i = 0; rhs[i] != '\0'; i++) {
        char symbol = rhs[i];

        if (symbol == '#') {
            if (!firstSetContains(target, '#')) {
                addToFirstSet(target, '#');
                changed = 1;
            }
            return changed;
        }

        if (!isNonTerminal(symbol)) {
            if (!firstSetContains(target, symbol)) {
                addToFirstSet(target, symbol);
                changed = 1;
            }

            allCanProduceEpsilon = 0;
            break;
        }

        int index = getFirstSetIndex(firstCollection, symbol);

        if (index == -1) {
            allCanProduceEpsilon = 0;
            break;
        }

        FirstSet *source = &firstCollection->sets[index];
        int hasEpsilon = 0;

        for (int j = 0; j < source->count; j++) {
            if (source->elements[j] == '#') {
                hasEpsilon = 1;
            } else if (!firstSetContains(target, source->elements[j])) {
                addToFirstSet(target, source->elements[j]);
                changed = 1;
            }
        }

        if (!hasEpsilon) {
            allCanProduceEpsilon = 0;
            break;
        }
    }

    if (allCanProduceEpsilon) {
        if (!firstSetContains(target, '#')) {
            addToFirstSet(target, '#');
            changed = 1;
        }
    }

    return changed;
}

void computeFirstSets(
    const Grammar *grammar,
    FirstCollection *firstCollection
) {
    initializeFirstCollection(firstCollection);

    initializeSetsForNonTerminals(grammar, firstCollection);

    int changed = 1;

    while (changed) {
        changed = 0;

        for (int i = 0; i < grammar->productionCount; i++) {
            char lhs = grammar->productions[i].lhs;

            int lhsIndex =
                getFirstSetIndex(firstCollection, lhs);

            if (lhsIndex == -1) {
                continue;
            }

            FirstSet *target =
                &firstCollection->sets[lhsIndex];

            for (
                int j = 0;
                j < grammar->productions[i].rhsCount;
                j++
            ) {
                if (addFirstOfSequence(
                        grammar->productions[i].rhs[j],
                        firstCollection,
                        target
                    )) {
                    changed = 1;
                }
            }
        }
    }
}

void displayFirstSets(const FirstCollection *firstCollection) {
    printf("\n========== FIRST SETS ==========\n");

    for (int i = 0; i < firstCollection->setCount; i++) {
        printf(
            "FIRST(%c) = { ",
            firstCollection->sets[i].symbol
        );

        for (int j = 0; j < firstCollection->sets[i].count; j++) {
            printf("%c", firstCollection->sets[i].elements[j]);

            if (j < firstCollection->sets[i].count - 1) {
                printf(", ");
            }
        }

        printf(" }\n");
    }

    printf("================================\n");
}