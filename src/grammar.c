/*
 * grammar.c - Module 1 (Member 1: 24bce0092)
 *
 * Grammar input, production storage, symbol classification, and
 * validation. Migrated from the single-char prototype to string
 * symbols so multi-character names (Ep, Tp, id, num) work.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "grammar.h"

/* Signature declared in grammar.h; defined here (see file end). */
int validateProductionDetailed(const char *line,
                               char *errorOut, size_t errorSize);

/* ======================================================================
 * Lifetime
 * ====================================================================== */

void initializeGrammar(Grammar *grammar) {
    memset(grammar, 0, sizeof(*grammar));
    grammar->startSymbol[0] = '\0';
}

/* ======================================================================
 * Internal helpers
 * ====================================================================== */

/* Heuristic used for "undefined symbol" diagnostics: an RHS token that
 * was never defined on any LHS but starts with an uppercase letter is
 * most likely a mis-typed non-terminal, so we reject it. Lowercase and
 * operator-shaped tokens are ordinary terminals. */
static int looksLikeNonTerminal(const char *token) {
    return token[0] >= 'A' && token[0] <= 'Z';
}

/* Accepts ordinary terminal names ("id", "num", "+", "("). */
static int isValidTerminalName(const char *token) {
    return utilsIsValidSymbol(token);
}

/* ======================================================================
 * Line-level validation (no grammar mutation)
 * ====================================================================== */

int validateProduction(const char *line) {
    char errorOut[256];
    return validateProductionDetailed(line, errorOut, sizeof(errorOut));
}

int validateProductionDetailed(const char *line,
                               char *errorOut, size_t errorSize) {
    errorOut[0] = '\0';

    if (line == NULL || line[0] == '\0') {
        snprintf(errorOut, errorSize, "empty production line");
        return 0;
    }

    const char *arrow = strstr(line, "->");
    if (arrow == NULL) {
        snprintf(errorOut, errorSize, "missing '->' separator");
        return 0;
    }
    if (strstr(arrow + 2, "->") != NULL) {
        snprintf(errorOut, errorSize, "more than one '->' separator");
        return 0;
    }

    /* LHS: trim, must be one valid symbol. */
    char lhs[MAX_LINE_LENGTH];
    size_t lhsLen = (size_t)(arrow - line);
    if (lhsLen >= sizeof(lhs)) lhsLen = sizeof(lhs) - 1;
    memcpy(lhs, line, lhsLen);
    lhs[lhsLen] = '\0';

    char *lhsTrim = utilsTrim(lhs);
    if (lhsTrim[0] == '\0') {
        snprintf(errorOut, errorSize, "empty left-hand side");
        return 0;
    }
    if (strchr(lhsTrim, ' ') != NULL) {
        snprintf(errorOut, errorSize, "left-hand side must be a single symbol");
        return 0;
    }
    if (!utilsIsValidSymbol(lhsTrim)) {
        snprintf(errorOut, errorSize, "invalid left-hand side symbol '%s'",
                 lhsTrim);
        return 0;
    }

    /* RHS: one or more '|'-separated alternatives, each of one or more
     * valid symbols; "#" must appear alone in its alternative. */
    const char *rhs = arrow + 2;
    int altCount = 0;
    const char *p = rhs;

    for (;;) {
        const char *pipe = strchr(p, '|');
        size_t segLen = pipe ? (size_t)(pipe - p) : strlen(p);

        char segment[MAX_LINE_LENGTH];
        if (segLen >= sizeof(segment)) segLen = sizeof(segment) - 1;
        memcpy(segment, p, segLen);
        segment[segLen] = '\0';

        char *segTrim = utilsTrim(segment);
        if (segTrim[0] == '\0') {
            snprintf(errorOut, errorSize,
                     "empty alternative (check '|' placement)");
            return 0;
        }

        char scratch[MAX_LINE_LENGTH];
        snprintf(scratch, sizeof(scratch), "%s", segTrim);
        int tokenCount = 0;
        for (char *tok = strtok(scratch, " ");
             tok != NULL;
             tok = strtok(NULL, " ")) {
            tokenCount++;
            if (!utilsIsValidSymbol(tok) && strcmp(tok, "#") != 0) {
                snprintf(errorOut, errorSize, "invalid symbol '%s'", tok);
                return 0;
            }
        }
        if (tokenCount == 0) {
            snprintf(errorOut, errorSize, "empty alternative");
            return 0;
        }
        if (strcmp(utilsTrim(segment), "#") != 0 && strchr(segTrim, '#') != NULL) {
            snprintf(errorOut, errorSize,
                     "'#' must be the only symbol of its alternative");
            return 0;
        }
        if (strchr(segTrim, '$') != NULL) {
            snprintf(errorOut, errorSize,
                     "'%c' is reserved as the end marker", END_MARKER);
            return 0;
        }

        altCount++;
        if (!pipe) break;
        p = pipe + 1;
    }

    if (altCount > MAX_ALTERNATIVES) {
        snprintf(errorOut, errorSize,
                 "too many alternatives (max %d)", MAX_ALTERNATIVES);
        return 0;
    }
    return 1;
}

/* ======================================================================
 * Production storage
 * ====================================================================== */

/* Tokenizes a raw alternative into single-space-separated symbols. */
static int normalizeAlternative(const char *raw, char *out, size_t outSize) {
    char buffer[MAX_LINE_LENGTH];
    snprintf(buffer, sizeof(buffer), "%s", raw);

    out[0] = '\0';
    size_t used = 0;
    for (char *tok = strtok(buffer, " \t");
         tok != NULL;
         tok = strtok(NULL, " \t")) {
        size_t len = strlen(tok);
        if (used + len + 2 > outSize) {
            return 0;
        }
        if (used > 0) out[used++] = ' ';
        memcpy(out + used, tok, len + 1);
        used += len;
    }
    return out[0] != '\0';
}

int addProduction(Grammar *grammar, const char *line,
                  char *errorOut, size_t errorSize) {
    if (!validateProductionDetailed(line, errorOut, errorSize)) {
        return 0;
    }

    char working[MAX_LINE_LENGTH];
    snprintf(working, sizeof(working), "%s", line);
    utilsNormalizeWhitespace(working);

    char *arrow = strstr(working, "->");
    *arrow = '\0';
    const char *lhs = utilsTrim(working);
    const char *rhsRaw = arrow + 2;

    /* Locate or create the production group for this LHS. */
    int group = -1;
    for (int i = 0; i < grammar->productionCount; i++) {
        if (strcmp(grammar->productions[i].lhs, lhs) == 0) {
            group = i;
            break;
        }
    }
    if (group == -1) {
        if (grammar->productionCount >= MAX_PRODUCTIONS) {
            snprintf(errorOut, errorSize,
                     "too many productions (max %d)", MAX_PRODUCTIONS);
            return 0;
        }
        group = grammar->productionCount++;
        snprintf(grammar->productions[group].lhs,
                 MAX_SYMBOL_LENGTH, "%s", lhs);
        grammar->productions[group].rhsCount = 0;
        if (grammar->productionCount == 1) {
            /* First production's LHS is the start symbol. */
            snprintf(grammar->startSymbol, MAX_SYMBOL_LENGTH, "%s", lhs);
        }
    }

    /* Append each alternative. */
    const char *p = rhsRaw;
    for (;;) {
        const char *pipe = strchr(p, '|');
        size_t segLen = pipe ? (size_t)(pipe - p) : strlen(p);

        char segment[MAX_LINE_LENGTH];
        if (segLen >= sizeof(segment)) segLen = sizeof(segment) - 1;
        memcpy(segment, p, segLen);
        segment[segLen] = '\0';

        char normalized[MAX_RHS_LENGTH];
        if (!normalizeAlternative(utilsTrim(segment),
                                  normalized, sizeof(normalized))) {
            snprintf(errorOut, errorSize, "empty alternative");
            return 0;
        }

        Production *prod = &grammar->productions[group];
        if (prod->rhsCount >= MAX_ALTERNATIVES) {
            snprintf(errorOut, errorSize,
                     "too many alternatives for '%s' (max %d)",
                     prod->lhs, MAX_ALTERNATIVES);
            return 0;
        }
        snprintf(prod->rhs[prod->rhsCount], MAX_RHS_LENGTH, "%s", normalized);
        prod->rhsCount++;

        if (!pipe) break;
        p = pipe + 1;
    }
    return 1;
}

/* ======================================================================
 * Whole-grammar input
 * ====================================================================== */

int readGrammarFromStream(Grammar *grammar, FILE *stream,
                          char *errorOut, size_t errorSize) {
    char line[MAX_LINE_LENGTH];
    int lineNumber = 0;
    int productionLines = 0;

    while (fgets(line, sizeof(line), stream) != NULL) {
        lineNumber++;
        utilsNormalizeWhitespace(line);
        char *trimmed = utilsTrim(line);
        if (trimmed[0] == '\0') {
            continue; /* skip blank lines */
        }
        if (!addProduction(grammar, trimmed, errorOut, errorSize)) {
            char context[160];
            snprintf(context, sizeof(context),
                     " (line %d: \"%s\")", lineNumber, trimmed);
            strncat(errorOut, context, errorSize - strlen(errorOut) - 1);
            return 0;
        }
        productionLines++;
    }

    if (productionLines == 0) {
        snprintf(errorOut, errorSize, "no productions supplied");
        return 0;
    }

    identifySymbols(grammar);
    if (!grammarValidate(grammar, errorOut, errorSize)) {
        return 0;
    }
    return 1;
}

int readGrammar(Grammar *grammar) {
    char errorOut[256];
    int count = 0;

    printf("Enter number of grammar productions: ");
    if (scanf("%d", &count) != 1) {
        printf("Error: invalid number of productions.\n");
        return 0;
    }
    if (count <= 0 || count > MAX_PRODUCTIONS) {
        printf("Error: number of productions must be between 1 and %d.\n",
               MAX_PRODUCTIONS);
        return 0;
    }

    /* Consume the rest of the line after the number. */
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) { }

    for (int i = 0; i < count; i++) {
        char line[MAX_LINE_LENGTH];
        printf("Production %d: ", i + 1);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("Error: failed to read production.\n");
            return 0;
        }
        utilsNormalizeWhitespace(line);
        char *trimmed = utilsTrim(line);
        if (trimmed[0] == '\0') {
            printf("Error: empty production is not allowed.\n");
            return 0;
        }
        if (!addProduction(grammar, trimmed, errorOut, sizeof(errorOut))) {
            printf("Error: invalid production: %s\n", errorOut);
            return 0;
        }
    }

    identifySymbols(grammar);
    if (!grammarValidate(grammar, errorOut, sizeof(errorOut))) {
        printf("Error: %s\n", errorOut);
        return 0;
    }
    return 1;
}

/* ======================================================================
 * Symbol classification
 * ====================================================================== */

void identifySymbols(Grammar *grammar) {
    grammar->nonTerminalCount = 0;
    grammar->terminalCount = 0;

    /* Non-terminals: every LHS, in first-appearance order. */
    for (int i = 0; i < grammar->productionCount; i++) {
        const char *lhs = grammar->productions[i].lhs;
        int seen = 0;
        for (int j = 0; j < grammar->nonTerminalCount; j++) {
            if (strcmp(grammar->nonTerminals[j], lhs) == 0) {
                seen = 1;
                break;
            }
        }
        if (!seen && grammar->nonTerminalCount < MAX_SYMBOLS) {
            snprintf(grammar->nonTerminals[grammar->nonTerminalCount],
                     MAX_SYMBOL_LENGTH, "%s", lhs);
            grammar->nonTerminalCount++;
        }
    }

    /* Terminals: every RHS symbol that is not "#" and not a non-terminal,
     * in first-appearance order. */
    for (int i = 0; i < grammar->productionCount; i++) {
        for (int j = 0; j < grammar->productions[i].rhsCount; j++) {
            const char *alt = grammar->productions[i].rhs[j];
            int symbols = productionAltSymbolCount(alt);
            for (int k = 0; k < symbols; k++) {
                char token[MAX_SYMBOL_LENGTH];
                productionAltGetSymbol(alt, k, token);
                if (strcmp(token, "#") == 0) {
                    continue;
                }
                int isNT = 0;
                for (int t = 0; t < grammar->nonTerminalCount; t++) {
                    if (strcmp(grammar->nonTerminals[t], token) == 0) {
                        isNT = 1;
                        break;
                    }
                }
                if (isNT) {
                    continue;
                }
                int seen = 0;
                for (int t = 0; t < grammar->terminalCount; t++) {
                    if (strcmp(grammar->terminals[t], token) == 0) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen && grammar->terminalCount < MAX_SYMBOLS) {
                    snprintf(grammar->terminals[grammar->terminalCount],
                             MAX_SYMBOL_LENGTH, "%s", token);
                    grammar->terminalCount++;
                }
            }
        }
    }
}

int grammarIsNonTerminal(const Grammar *grammar, const char *name) {
    return grammarNonTerminalIndex(grammar, name) != INVALID_INDEX;
}

int grammarIsTerminal(const Grammar *grammar, const char *name) {
    return grammarTerminalIndex(grammar, name) != INVALID_INDEX;
}

int grammarNonTerminalIndex(const Grammar *grammar, const char *name) {
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        if (strcmp(grammar->nonTerminals[i], name) == 0) {
            return i;
        }
    }
    return INVALID_INDEX;
}

int grammarTerminalIndex(const Grammar *grammar, const char *name) {
    for (int i = 0; i < grammar->terminalCount; i++) {
        if (strcmp(grammar->terminals[i], name) == 0) {
            return i;
        }
    }
    return INVALID_INDEX;
}

/* ======================================================================
 * Whole-grammar semantic validation
 * ====================================================================== */

int grammarValidate(const Grammar *grammar, char *errorOut,
                    size_t errorSize) {
    errorOut[0] = '\0';

    if (grammar->productionCount == 0) {
        snprintf(errorOut, errorSize, "grammar has no productions");
        return 0;
    }
    if (grammar->startSymbol[0] == '\0') {
        snprintf(errorOut, errorSize, "start symbol not set");
        return 0;
    }

    for (int i = 0; i < grammar->productionCount; i++) {
        const Production *prod = &grammar->productions[i];
        for (int j = 0; j < prod->rhsCount; j++) {
            const char *alt = prod->rhs[j];
            int symbols = productionAltSymbolCount(alt);
            if (symbols > MAX_RHS_SYMBOLS) {
                snprintf(errorOut, errorSize,
                         "alternative of '%s' has too many symbols (max %d)",
                         prod->lhs, MAX_RHS_SYMBOLS);
                return 0;
            }
            for (int k = 0; k < symbols; k++) {
                char token[MAX_SYMBOL_LENGTH];
                productionAltGetSymbol(alt, k, token);

                if (strcmp(token, "#") == 0) {
                    continue;
                }
                if (grammarIsNonTerminal(grammar, token)) {
                    continue;
                }
                if (!isValidTerminalName(token)) {
                    snprintf(errorOut, errorSize,
                             "invalid symbol '%s' in production of '%s'",
                             token, prod->lhs);
                    return 0;
                }
                if (looksLikeNonTerminal(token)) {
                    snprintf(errorOut, errorSize,
                             "non-terminal '%s' is used but never defined "
                             "(no production with LHS '%s')",
                             token, token);
                    return 0;
                }
            }
        }
    }
    return 1;
}

/* ======================================================================
 * RHS tokenization helpers
 * ====================================================================== */

int productionAltSymbolCount(const char *rhsAlt) {
    if (rhsAlt == NULL) return 0;
    int count = 0;
    int inToken = 0;
    for (const char *p = rhsAlt; *p != '\0'; p++) {
        if (*p == ' ' || *p == '\t') {
            inToken = 0;
        } else if (!inToken) {
            inToken = 1;
            count++;
        }
    }
    return count;
}

int productionAltGetSymbol(const char *rhsAlt, int index, char *out) {
    char buffer[MAX_RHS_LENGTH];
    snprintf(buffer, sizeof(buffer), "%s", rhsAlt);

    int i = 0;
    for (char *tok = strtok(buffer, " \t");
         tok != NULL;
         tok = strtok(NULL, " \t")) {
        if (i == index) {
            snprintf(out, MAX_SYMBOL_LENGTH, "%s", tok);
            return 1;
        }
        i++;
    }
    out[0] = '\0';
    return 0;
}

int productionAltIsEpsilon(const char *rhsAlt) {
    return rhsAlt != NULL && strcmp(rhsAlt, "#") == 0;
}

void productionAltToString(const Production *prod, int altIndex,
                           char *out, size_t outSize) {
    if (prod == NULL || altIndex < 0 || altIndex >= prod->rhsCount) {
        snprintf(out, outSize, "?");
        return;
    }
    if (productionAltIsEpsilon(prod->rhs[altIndex])) {
        snprintf(out, outSize, "#");
        return;
    }
    snprintf(out, outSize, "%s", prod->rhs[altIndex]);
}

/* ======================================================================
 * Display
 * ====================================================================== */

void displayGrammar(const Grammar *grammar) {
    printf("\n========== GRAMMAR ==========\n");
    for (int i = 0; i < grammar->productionCount; i++) {
        printf("%s -> ", grammar->productions[i].lhs);
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
        printf("%s ", grammar->nonTerminals[i]);
    }
    printf("\nTerminals: ");
    for (int i = 0; i < grammar->terminalCount; i++) {
        printf("%s ", grammar->terminals[i]);
    }
    printf("\nStart Symbol: %s\n", grammar->startSymbol);
}
