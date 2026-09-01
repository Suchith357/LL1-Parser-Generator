#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "../include/grammar.h"

void initializeGrammar(Grammar *grammar) {
    grammar->productionCount = 0;
    grammar->nonTerminalCount = 0;
    grammar->terminalCount = 0;
    grammar->startSymbol = '\0';
}

int isNonTerminal(char symbol) {
    return symbol >= 'A' && symbol <= 'Z';
}

int grammarHasNonTerminal(const Grammar *grammar, char symbol) {
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        if (grammar->nonTerminals[i] == symbol) {
            return 1;
        }
    }
    return 0;
}

static int grammarHasTerminal(const Grammar *grammar, char symbol) {
    for (int i = 0; i < grammar->terminalCount; i++) {
        if (grammar->terminals[i] == symbol) {
            return 1;
        }
    }
    return 0;
}

static void removeSpaces(char *line) {
    int i = 0;
    int j = 0;

    while (line[i] != '\0') {
        if (!isspace((unsigned char)line[i])) {
            line[j++] = line[i];
        }
        i++;
    }

    line[j] = '\0';
}

int validateProduction(const char *line) {
    int length = strlen(line);

    if (length < 4) {
        return 0;
    }

    if (!isNonTerminal(line[0])) {
        return 0;
    }

    if (line[1] != '-' || line[2] != '>') {
        return 0;
    }

    if (line[3] == '\0') {
        return 0;
    }

    int previousWasPipe = 0;

    for (int i = 3; line[i] != '\0'; i++) {
        char ch = line[i];

        if (ch == '|') {
            if (i == 3 || previousWasPipe || line[i + 1] == '\0') {
                return 0;
            }

            previousWasPipe = 1;
        } else {
            previousWasPipe = 0;

            if (!isprint((unsigned char)ch)) {
                return 0;
            }
        }
    }

    return 1;
}

int addProduction(Grammar *grammar, const char *inputLine) {
    if (grammar->productionCount >= MAX_PRODUCTIONS) {
        return 0;
    }

    char line[MAX_LINE_LENGTH];

    strncpy(line, inputLine, MAX_LINE_LENGTH - 1);
    line[MAX_LINE_LENGTH - 1] = '\0';

    removeSpaces(line);

    if (!validateProduction(line)) {
        return 0;
    }

    char lhs = line[0];

    int productionIndex = -1;

    for (int i = 0; i < grammar->productionCount; i++) {
        if (grammar->productions[i].lhs == lhs) {
            productionIndex = i;
            break;
        }
    }

    if (productionIndex == -1) {
        productionIndex = grammar->productionCount++;

        grammar->productions[productionIndex].lhs = lhs;
        grammar->productions[productionIndex].rhsCount = 0;

        if (grammar->productionCount == 1) {
            grammar->startSymbol = lhs;
        }
    }

    char rhsBuffer[MAX_RHS_LENGTH];
    int bufferIndex = 0;

    for (int i = 3;; i++) {
        char ch = line[i];

        if (ch == '|' || ch == '\0') {
            rhsBuffer[bufferIndex] = '\0';

            if (bufferIndex == 0 ||
                grammar->productions[productionIndex].rhsCount >= MAX_ALTERNATIVES) {
                return 0;
            }

            strcpy(
                grammar->productions[productionIndex]
                    .rhs[grammar->productions[productionIndex].rhsCount],
                rhsBuffer
            );

            grammar->productions[productionIndex].rhsCount++;

            bufferIndex = 0;

            if (ch == '\0') {
                break;
            }
        } else {
            if (bufferIndex >= MAX_RHS_LENGTH - 1) {
                return 0;
            }

            rhsBuffer[bufferIndex++] = ch;
        }
    }

    return 1;
}

void identifySymbols(Grammar *grammar) {
    grammar->nonTerminalCount = 0;
    grammar->terminalCount = 0;

    for (int i = 0; i < grammar->productionCount; i++) {
        char lhs = grammar->productions[i].lhs;

        if (!grammarHasNonTerminal(grammar, lhs)) {
            grammar->nonTerminals[grammar->nonTerminalCount++] = lhs;
        }
    }

    for (int i = 0; i < grammar->productionCount; i++) {
        for (int j = 0; j < grammar->productions[i].rhsCount; j++) {

            char *rhs = grammar->productions[i].rhs[j];

            for (int k = 0; rhs[k] != '\0'; k++) {
                char symbol = rhs[k];

                if (symbol == '#') {
                    continue;
                }

                if (isNonTerminal(symbol)) {
                    continue;
                }

                if (!grammarHasTerminal(grammar, symbol)) {
                    grammar->terminals[grammar->terminalCount++] = symbol;
                }
            }
        }
    }
}

int readGrammar(Grammar *grammar) {
    int count;

    printf("Enter number of grammar productions: ");

    if (scanf("%d", &count) != 1 ||
        count <= 0 ||
        count > MAX_PRODUCTIONS) {

        while (getchar() != '\n');
        return 0;
    }

    while (getchar() != '\n');

    for (int i = 0; i < count; i++) {
        char line[MAX_LINE_LENGTH];

        printf("Production %d: ", i + 1);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            return 0;
        }

        line[strcspn(line, "\n")] = '\0';

        if (!addProduction(grammar, line)) {
            printf("Invalid grammar format: %s\n", line);
            return 0;
        }
    }

    identifySymbols(grammar);

    return 1;
}

void displayGrammar(const Grammar *grammar) {
    printf("\n========== GRAMMAR ==========\n");

    for (int i = 0; i < grammar->productionCount; i++) {

        printf("%c -> ", grammar->productions[i].lhs);

        for (int j = 0; j < grammar->productions[i].rhsCount; j++) {

            printf("%s", grammar->productions[i].rhs[j]);

            if (j < grammar->productions[i].rhsCount - 1) {
                printf(" | ");
            }
        }

        printf("\n");
    }

    printf("=============================\n");
}

void displaySymbols(const Grammar *grammar) {
    printf("\nNon-Terminals: ");

    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        printf("%c ", grammar->nonTerminals[i]);
    }

    printf("\nTerminals: ");

    for (int i = 0; i < grammar->terminalCount; i++) {
        printf("%c ", grammar->terminals[i]);
    }

    printf("\nStart Symbol: %c\n", grammar->startSymbol);
}