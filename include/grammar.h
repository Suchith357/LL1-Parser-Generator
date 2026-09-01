#ifndef GRAMMAR_H
#define GRAMMAR_H

#define MAX_PRODUCTIONS 50
#define MAX_ALTERNATIVES 20
#define MAX_SYMBOLS 50
#define MAX_RHS_LENGTH 100
#define MAX_LINE_LENGTH 256

typedef struct {
    char lhs;
    char rhs[MAX_ALTERNATIVES][MAX_RHS_LENGTH];
    int rhsCount;
} Production;

typedef struct {
    Production productions[MAX_PRODUCTIONS];
    int productionCount;

    char nonTerminals[MAX_SYMBOLS];
    int nonTerminalCount;

    char terminals[MAX_SYMBOLS];
    int terminalCount;

    char startSymbol;
} Grammar;

void initializeGrammar(Grammar *grammar);

int readGrammar(Grammar *grammar);

int validateProduction(const char *line);

int addProduction(Grammar *grammar, const char *line);

void identifySymbols(Grammar *grammar);

void displayGrammar(const Grammar *grammar);

void displaySymbols(const Grammar *grammar);

int isNonTerminal(char symbol);

int grammarHasNonTerminal(const Grammar *grammar, char symbol);

#endif