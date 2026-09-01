#include <stdio.h>
#include "../include/grammar.h"
#include "../include/first.h"

int main() {
    Grammar grammar;
    FirstCollection firstCollection;

    initializeGrammar(&grammar);
    initializeFirstCollection(&firstCollection);

    printf("====================================\n");
    printf("      LL(1) PARSER GENERATOR\n");
    printf("====================================\n\n");

    printf("Grammar Format:\n");
    printf("A->alpha|beta\n");
    printf("# represents epsilon\n\n");

    if (!readGrammar(&grammar)) {
        printf("\nFailed to load grammar.\n");
        return 1;
    }

    printf("\nGrammar loaded successfully.\n");

    displayGrammar(&grammar);
    displaySymbols(&grammar);

    computeFirstSets(&grammar, &firstCollection);

    displayFirstSets(&firstCollection);

    return 0;
}