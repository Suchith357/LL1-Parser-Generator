#ifndef UTILS_H
#define UTILS_H

/*
 * utils.h - Shared data structures and helpers used by every module.
 *
 * Design decision (shared architecture, agreed by all five members):
 *
 * 1. A SymbolTable is a small string->index registry that maps every
 *    distinct grammar symbol (terminal or non-terminal) to an index.
 *    The numeric index is the row/column of the parsing table, so table
 *    construction and the predictive parser work on integers, not strings.
 *
 * 2. StringSet is the generic "set of symbols" used for FIRST/FOLLOW
 *    elements and for membership queries. It supports a used-bitmap so
 *    O(1) set-difference tests are possible later.
 *
 * 3. END_MARKER ('$', defined in constants.h) is reserved: it is never a
 *    grammar symbol and never a token.
 */

#include "constants.h"

#define MAX_REGISTRY_ENTRIES (2 * MAX_SYMBOLS) /* terminals+nonterminals */

/* ---- StringSet: generic set of symbols ------------------------------- */

typedef struct {
    char elements[MAX_SET_ELEMENTS][MAX_SYMBOL_LENGTH];
    int  count;
    /* used[i] == 1 iff elements[i] is currently a member. Kept in sync by
     * the add/remove helpers; used for O(1) difference tests. */
    unsigned char used[MAX_SET_ELEMENTS];
} StringSet;

void setInit(StringSet *set);              /* make it empty            */
int  setContains(const StringSet *set, const char *symbol); /* 1/0     */
int  setAdd(StringSet *set, const char *symbol); /* 1 if newly added   */
void setRemove(StringSet *set, const char *symbol);
void setCopy(const StringSet *src, StringSet *dst);
int  setEquals(const StringSet *src, const StringSet *dst);
void setClear(StringSet *set);             /* empty but keep capacity  */

/* ---- SymbolTable: string <-> dense index registry --------------------- */

typedef struct {
    char names[MAX_REGISTRY_ENTRIES][MAX_SYMBOL_LENGTH];
    int  count;
} SymbolTable;

void symtabInit(SymbolTable *st);
/* Returns the index of `name`, inserting it if absent. */
int  symtabIntern(SymbolTable *st, const char *name);
/* Returns index or INVALID_INDEX when absent. */
int  symtabLookup(const SymbolTable *st, const char *name);
const char *symtabName(const SymbolTable *st, int index);
/* 1 iff index is inside [0, count). */
int  symtabValidIndex(const SymbolTable *st, int index);

/* ---- Lexical helpers shared by grammar reader and parser ------------- */

/* 1 iff `name` is "#" (the epsilon marker used in the input format). */
int  utilsIsEpsilon(const char *name);

/* 1 iff `name` is "$" (reserved end-of-input marker; rejected in grammars). */
int  utilsIsEndMarker(const char *name);

/* 1 iff name is a valid symbol: 1..MAX_SYMBOL_LENGTH-1 characters from
 * alphanumerics, underscore, and the operators + - * / ( ) =. */
int  utilsIsValidSymbol(const char *name);

/* 1 iff `name` is one of the grammar's LHS names (non-terminal). */
int  utilsIsNonTerminal(const SymbolTable *nonterminals, const char *name);

/* In-place trim of leading/trailing whitespace, returns pointer into s. */
char *utilsTrim(char *s);

/* Replace '\r', '\n', '\t' inside s with single spaces (defensive). */
void utilsNormalizeWhitespace(char *s);

#endif /* UTILS_H */
