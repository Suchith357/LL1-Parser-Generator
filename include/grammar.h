#ifndef GRAMMAR_H
#define GRAMMAR_H

/*
 * grammar.h - Module 1 (Member 1: 24bce0092)
 *
 * Grammar input, production storage, symbol classification, and
 * validation for the LL(1) Parser Generator.
 *
 * IMPORTANT INTERFACE CHANGE (v2 shared architecture):
 * Earlier drafts stored symbols as single chars, which cannot represent
 * multi-character symbols such as "Ep", "Tp" or "id" required by the
 * locked demo grammar. All symbols are now fixed-size strings of up to
 * MAX_SYMBOL_LENGTH-1 characters. All downstream modules use this header.
 *
 * Grammar input format (documented in docs/input_format.md):
 *   LHS -> ALT1 | ALT2
 *   "->" separates LHS and RHS, "|" separates alternatives,
 *   "#" denotes epsilon, symbols are separated by spaces,
 *   the first production's LHS is the start symbol.
 */

#include "constants.h"
#include "utils.h"

#include <stdio.h>
#include <stddef.h>

/* One production: a single LHS with one or more RHS alternatives.
 * Each alternative is stored as the original space-separated symbol
 * string, e.g. rhs[0] = "+ T Ep". Tokenization helpers below split it
 * into symbols on demand. */
typedef struct {
    char lhs[MAX_SYMBOL_LENGTH];
    char rhs[MAX_ALTERNATIVES][MAX_RHS_LENGTH];
    int  rhsCount;
} Production;

/* Whole grammar: productions plus the symbol classification computed by
 * identifySymbols(). nonTerminals are exactly the symbols that appear on
 * some LHS (the "variables" of the grammar); everything else that
 * appears on an RHS (except "#") is a terminal. */
typedef struct {
    Production productions[MAX_PRODUCTIONS];
    int        productionCount;

    char nonTerminals[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    int  nonTerminalCount;

    char terminals[MAX_SYMBOLS][MAX_SYMBOL_LENGTH];
    int  terminalCount;

    char startSymbol[MAX_SYMBOL_LENGTH];
} Grammar;

/* ---- Lifetime and input ---------------------------------------------- */

void initializeGrammar(Grammar *grammar);

/* Interactive entry: prompts for the production count and productions
 * on stdin. Returns 1 on success, 0 on failure (with a message printed). */
int  readGrammar(Grammar *grammar);

/* Reads productions from any stream (file or stdin). Blank lines are
 * skipped. Returns 1 on success, 0 with a message in errorOut. */
int  readGrammarFromStream(Grammar *grammar, FILE *stream,
                           char *errorOut, size_t errorSize);

/* Adds one line "LHS -> ALT1 | ALT2". Merges into an existing group when
 * the LHS was seen before. Returns 1 on success, 0 with message. */
int  addProduction(Grammar *grammar, const char *line,
                   char *errorOut, size_t errorSize);

/* Cheap syntax check of a single line (no grammar mutation). 1/0. */
int  validateProduction(const char *line);

/* Whole-grammar semantic validation: undefined non-terminals, invalid
 * symbols, reserved "$" usage. Returns 1 if valid, 0 with message. */
int  grammarValidate(const Grammar *grammar, char *errorOut, size_t errorSize);

/* ---- Symbol classification ------------------------------------------- */

void identifySymbols(Grammar *grammar);

/* 1 iff `name` appears as some production's LHS. */
int  grammarIsNonTerminal(const Grammar *grammar, const char *name);

/* 1 iff `name` is in the terminal list. */
int  grammarIsTerminal(const Grammar *grammar, const char *name);

/* Index of name in nonTerminals / terminals, or INVALID_INDEX. */
int  grammarNonTerminalIndex(const Grammar *grammar, const char *name);
int  grammarTerminalIndex(const Grammar *grammar, const char *name);

/* ---- RHS tokenization helpers (used by FIRST/FOLLOW/table/parser) ---- */

/* Number of symbols in a space-separated RHS alternative ("T Ep" -> 2). */
int  productionAltSymbolCount(const char *rhsAlt);

/* Copies symbol number `index` of the alternative into out. 1/0. */
int  productionAltGetSymbol(const char *rhsAlt, int index, char *out);

/* 1 iff the alternative is exactly "#" (epsilon). */
int  productionAltIsEpsilon(const char *rhsAlt);

/* Pretty-prints one alternative (epsilon shown as "#"). */
void productionAltToString(const Production *p, int altIndex,
                           char *out, size_t outSize);

/* ---- Display ---------------------------------------------------------- */

void displayGrammar(const Grammar *grammar);
void displaySymbols(const Grammar *grammar);

#endif /* GRAMMAR_H */
