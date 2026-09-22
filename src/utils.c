#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "constants.h"
#include "utils.h"

/* ======================================================================
 * StringSet: generic set of grammar symbols
 * ====================================================================== */

void setInit(StringSet *set) {
    set->count = 0;
    memset(set->used, 0, sizeof(set->used));
}

int setContains(const StringSet *set, const char *symbol) {
    if (symbol == NULL) {
        return 0;
    }
    for (int i = 0; i < set->count; i++) {
        if (set->used[i] && strcmp(set->elements[i], symbol) == 0) {
            return 1;
        }
    }
    return 0;
}

int setAdd(StringSet *set, const char *symbol) {
    if (symbol == NULL) {
        return 0;
    }
    if (setContains(set, symbol)) {
        return 0;
    }
    if (set->count >= MAX_SET_ELEMENTS) {
        fprintf(stderr,
                "utils: set overflow (max %d elements)\n",
                MAX_SET_ELEMENTS);
        return 0;
    }
    strncpy(set->elements[set->count], symbol, MAX_SYMBOL_LENGTH - 1);
    set->elements[set->count][MAX_SYMBOL_LENGTH - 1] = '\0';
    set->used[set->count] = 1;
    set->count++;
    return 1;
}

void setRemove(StringSet *set, const char *symbol) {
    if (symbol == NULL) {
        return;
    }
    for (int i = 0; i < set->count; i++) {
        if (set->used[i] && strcmp(set->elements[i], symbol) == 0) {
            set->used[i] = 0;
            return;
        }
    }
}

void setCopy(const StringSet *src, StringSet *dst) {
    *dst = *src;
}

/* Number of currently-used slots. setRemove keeps `count` as the
 * high-water mark, so equality tests must use this, not `count`. */
static int setActiveCount(const StringSet *set) {
    int active = 0;
    for (int i = 0; i < set->count; i++) {
        if (set->used[i]) {
            active++;
        }
    }
    return active;
}

int setEquals(const StringSet *a, const StringSet *b) {
    if (setActiveCount(a) != setActiveCount(b)) {
        return 0;
    }
    for (int i = 0; i < a->count; i++) {
        if (a->used[i] && !setContains(b, a->elements[i])) {
            return 0;
        }
    }
    return 1;
}

void setClear(StringSet *set) {
    setInit(set);
}

/* ======================================================================
 * SymbolTable: string <-> dense index registry
 * ====================================================================== */

void symtabInit(SymbolTable *st) {
    st->count = 0;
}

int symtabLookup(const SymbolTable *st, const char *name) {
    if (name == NULL) {
        return INVALID_INDEX;
    }
    for (int i = 0; i < st->count; i++) {
        if (strcmp(st->names[i], name) == 0) {
            return i;
        }
    }
    return INVALID_INDEX;
}

int symtabIntern(SymbolTable *st, const char *name) {
    int existing = symtabLookup(st, name);
    if (existing != INVALID_INDEX) {
        return existing;
    }
    if (st->count >= MAX_REGISTRY_ENTRIES) {
        fprintf(stderr,
                "utils: symbol table overflow (max %d)\n",
                MAX_REGISTRY_ENTRIES);
        return INVALID_INDEX;
    }
    strncpy(st->names[st->count], name, MAX_SYMBOL_LENGTH - 1);
    st->names[st->count][MAX_SYMBOL_LENGTH - 1] = '\0';
    return st->count++;
}

int symtabValidIndex(const SymbolTable *st, int index) {
    return index >= 0 && index < st->count;
}

const char *symtabName(const SymbolTable *st, int index) {
    if (symtabValidIndex(st, index)) {
        return st->names[index];
    }
    return "?";
}

/* ======================================================================
 * Lexical helpers
 * ====================================================================== */

int utilsIsEpsilon(const char *name) {
    return name != NULL && name[0] == '#' && name[1] == '\0';
}

int utilsIsEndMarker(const char *name) {
    return name != NULL && name[0] == END_MARKER && name[1] == '\0';
}

int utilsIsValidSymbol(const char *name) {
    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    if (utilsIsEpsilon(name) || utilsIsEndMarker(name)) {
        return 0;
    }
    if ((int)strlen(name) >= MAX_SYMBOL_LENGTH) {
        return 0;
    }
    for (int i = 0; name[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!(isalnum(ch) || ch == '_' || ch == '+' || ch == '-' ||
              ch == '*' || ch == '/' || ch == '(' || ch == ')' ||
              ch == '=')) {
            return 0;
        }
    }
    return 1;
}

int utilsIsNonTerminal(const SymbolTable *nonterminals, const char *name) {
    return symtabLookup(nonterminals, name) != INVALID_INDEX;
}

char *utilsTrim(char *s) {
    if (s == NULL) {
        return NULL;
    }
    while (isspace((unsigned char)*s)) {
        s++;
    }
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return s;
}

void utilsNormalizeWhitespace(char *s) {
    if (s == NULL) {
        return;
    }
    for (char *p = s; *p != '\0'; p++) {
        if (*p == '\r' || *p == '\n' || *p == '\t') {
            *p = ' ';
        }
    }
}
