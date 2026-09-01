#ifndef FIRST_H
#define FIRST_H

#include "grammar.h"

#define MAX_FIRST_SET 50

typedef struct {
    char symbol;
    char elements[MAX_FIRST_SET];
    int count;
} FirstSet;

typedef struct {
    FirstSet sets[MAX_SYMBOLS];
    int setCount;
} FirstCollection;

void initializeFirstCollection(FirstCollection *firstCollection);

void computeFirstSets(const Grammar *grammar, FirstCollection *firstCollection);

void displayFirstSets(const FirstCollection *firstCollection);

int getFirstSetIndex(const FirstCollection *firstCollection, char symbol);

int firstSetContains(const FirstSet *set, char symbol);

void addToFirstSet(FirstSet *set, char symbol);

#endif