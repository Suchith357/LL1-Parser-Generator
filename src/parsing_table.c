/*
 * parsing_table.c - Module 4 (Member 4: 24bci0103)
 *
 * LL(1) predictive parsing table construction and conflict detection.
 *
 * For every production A -> alpha:
 *   - for every terminal a in FIRST(alpha) \ {#}:  M[A, a] = A -> alpha
 *   - if # in FIRST(alpha): for every b in FOLLOW(A):  M[A, b] = A -> alpha
 *
 * The table stores production REFERENCES only (lhsIndex, altIndex) -
 * never duplicated production text. If a second production tries to
 * enter an occupied cell, the existing entry is preserved, the conflict
 * is recorded, and the grammar is marked NOT LL(1). Nothing is ever
 * silently overwritten.
 *
 * FIRST(alpha) comes from the shared firstOfSequence() helper (Module 2);
 * FOLLOW(A) comes from Module 3. No set logic is duplicated here.
 *
 * Complexity: P productions x R RHS symbols x (FIRST lookup) plus one
 * FOLLOW scan per nullable alternative -> O(P * R * T) worst case with
 * T terminals, in practice a single pass over the productions.
 */

#include <stdio.h>
#include <string.h>

#include "parsing_table.h"

/* Maximum characters of one entry rendered inside the table grid. */
#define TABLE_CELL_TEXT_MAX 24

/* ======================================================================
 * Lifetime
 * ====================================================================== */

void initializeParsingTable(ParsingTable *table) {
    table->nonTerminalCount = 0;
    table->terminalCount = 0;
    table->conflictCount = 0;
    for (int r = 0; r < MAX_TABLE_ROWS; r++) {
        for (int c = 0; c < MAX_TABLE_COLS; c++) {
            table->cells[r][c].lhsIndex = INVALID_INDEX;
            table->cells[r][c].altIndex = INVALID_INDEX;
        }
    }
}

/* ======================================================================
 * Entry placement and conflict recording
 * ====================================================================== */

/* Maps a terminal name (or "$") to its table column. */
static int tableColumn(const Grammar *grammar, const ParsingTable *table,
                       const char *terminal) {
    char endMarker[2] = { END_MARKER, '\0' };
    if (strcmp(terminal, endMarker) == 0) {
        return table->terminalCount; /* the reserved last column */
    }
    return grammarTerminalIndex(grammar, terminal);
}

/* Places one production reference into M[ntIndex, terminal]. Returns 1
 * if the cell was free (or held the same production); records a
 * conflict and returns 0 when a different production already owns it. */
static int placeEntry(const Grammar *grammar, ParsingTable *table,
                      int ntIndex, const char *terminal,
                      int lhsIndex, int altIndex) {
    int col = tableColumn(grammar, table, terminal);
    if (col == INVALID_INDEX || col >= MAX_TABLE_COLS) {
        return 0; /* unknown terminal: cannot happen after validation */
    }

    TableCell *cell = &table->cells[ntIndex][col];
    if (cell->lhsIndex == INVALID_INDEX) {
        cell->lhsIndex = lhsIndex;
        cell->altIndex = altIndex;
        return 1;
    }
    if (cell->lhsIndex == lhsIndex && cell->altIndex == altIndex) {
        return 1; /* same production again - not a conflict */
    }

    /* Conflict: keep the existing entry, record both productions. */
    if (table->conflictCount < MAX_CONFLICTS) {
        TableConflict *cf = &table->conflicts[table->conflictCount++];
        cf->lhsIndex = lhsIndex;          /* newcomer that tried to enter */
        cf->altIndex = altIndex;
        cf->lhsIndex2 = cell->lhsIndex;   /* production already in cell   */
        cf->altIndex2 = cell->altIndex;
        snprintf(cf->terminal, MAX_SYMBOL_LENGTH, "%s", terminal);
    } else {
        fprintf(stderr,
                "parsing_table: conflict list overflow (max %d)\n",
                MAX_CONFLICTS);
    }
    return 0;
}

/* ======================================================================
 * Construction
 * ====================================================================== */

int constructParsingTable(const Grammar *grammar,
                          const FirstCollection *first,
                          const FollowCollection *follow,
                          ParsingTable *table) {
    initializeParsingTable(table);

    if (grammar->nonTerminalCount == 0 || grammar->productionCount == 0) {
        fprintf(stderr,
                "parsing_table: grammar has no symbols; run "
                "identifySymbols() first\n");
        return 0;
    }
    if (grammar->nonTerminalCount > MAX_TABLE_ROWS) {
        fprintf(stderr,
                "parsing_table: too many non-terminals for the table "
                "(max %d)\n", MAX_TABLE_ROWS);
        return 0;
    }
    if (grammar->terminalCount + 1 > MAX_TABLE_COLS) {
        fprintf(stderr,
                "parsing_table: too many terminals for the table "
                "(max %d including '$')\n", MAX_TABLE_COLS);
        return 0;
    }
    if (follow->setCount != grammar->nonTerminalCount) {
        fprintf(stderr,
                "parsing_table: FOLLOW collection does not match the "
                "grammar (compute FOLLOW sets first)\n");
        return 0;
    }

    table->nonTerminalCount = grammar->nonTerminalCount;
    table->terminalCount = grammar->terminalCount;

    for (int i = 0; i < grammar->productionCount; i++) {
        const Production *prod = &grammar->productions[i];
        int ntIndex = grammarNonTerminalIndex(grammar, prod->lhs);
        if (ntIndex == INVALID_INDEX) {
            continue; /* cannot happen after validation */
        }

        for (int j = 0; j < prod->rhsCount; j++) {
            FirstSet firstAlpha;
            firstOfSequence(grammar, first, prod->rhs[j], &firstAlpha);

            /* Terminals of FIRST(alpha) \ {#}. */
            for (int e = 0; e < firstAlpha.count; e++) {
                if (strcmp(firstAlpha.elements[e], "#") == 0) {
                    continue;
                }
                placeEntry(grammar, table, ntIndex, firstAlpha.elements[e],
                           i, j);
            }

            /* Nullable alpha: the production predicts on FOLLOW(A),
             * which includes "$". */
            if (firstAlpha.hasEpsilon) {
                const FollowSet *fl = &follow->sets[ntIndex];
                for (int e = 0; e < fl->count; e++) {
                    placeEntry(grammar, table, ntIndex, fl->elements[e],
                               i, j);
                }
            }
        }
    }

    return table->conflictCount == 0; /* 1 = grammar is LL(1) */
}

/* ======================================================================
 * Queries
 * ====================================================================== */

int tableHasEntry(const ParsingTable *table, int ntIndex, int termIndex) {
    if (ntIndex < 0 || ntIndex >= table->nonTerminalCount) {
        return 0;
    }
    if (termIndex < 0 || termIndex > table->terminalCount) {
        return 0; /* terminalCount is the "$" column, so <= is correct */
    }
    return table->cells[ntIndex][termIndex].lhsIndex != INVALID_INDEX;
}

int tableEntryToString(const Grammar *grammar, const ParsingTable *table,
                       int ntIndex, int termIndex,
                       char *out, size_t outSize) {
    out[0] = '\0';
    if (!tableHasEntry(table, ntIndex, termIndex)) {
        return 0;
    }
    if (ntIndex >= grammar->nonTerminalCount ||
        termIndex > grammar->terminalCount) {
        return 0;
    }
    int lhsIndex = table->cells[ntIndex][termIndex].lhsIndex;
    int altIndex = table->cells[ntIndex][termIndex].altIndex;
    if (lhsIndex < 0 || lhsIndex >= grammar->productionCount) {
        return 0;
    }
    const Production *prod = &grammar->productions[lhsIndex];
    char altText[MAX_RHS_LENGTH];
    productionAltToString(prod, altIndex, altText, sizeof(altText));
    snprintf(out, outSize, "%s -> %s", prod->lhs, altText);
    return 1;
}

/* ======================================================================
 * Display
 * ====================================================================== */

/* RHS text of the production referenced by a cell, truncated for the
 * grid so one long alternative cannot wreck the layout. */
static void tableCellText(const Grammar *grammar, const ParsingTable *table,
                          int ntIndex, int termIndex,
                          char *out, size_t outSize) {
    out[0] = '\0';
    if (!tableHasEntry(table, ntIndex, termIndex)) {
        return;
    }
    int lhsIndex = table->cells[ntIndex][termIndex].lhsIndex;
    int altIndex = table->cells[ntIndex][termIndex].altIndex;
    if (lhsIndex < 0 || lhsIndex >= grammar->productionCount) {
        return;
    }
    char altText[MAX_RHS_LENGTH];
    productionAltToString(&grammar->productions[lhsIndex], altIndex,
                          altText, sizeof(altText));
    if (strlen(altText) >= TABLE_CELL_TEXT_MAX) {
        snprintf(out, outSize, "%.*s...",
                 TABLE_CELL_TEXT_MAX - 4, altText);
    } else {
        snprintf(out, outSize, "%s", altText);
    }
}

void displayParsingTable(const Grammar *grammar, const ParsingTable *table) {
    printf("\n========== PREDICTIVE PARSING TABLE ==========\n");

    int cols = grammar->terminalCount + 1; /* last column is "$" */
    char endMarker[2] = { END_MARKER, '\0' };

    int width[MAX_TABLE_COLS];
    int nameWidth = 4;
    for (int i = 0; i < grammar->nonTerminalCount; i++) {
        int l = (int)strlen(grammar->nonTerminals[i]);
        if (l > nameWidth) {
            nameWidth = l;
        }
    }
    for (int c = 0; c < cols; c++) {
        const char *header = (c < grammar->terminalCount)
                                 ? grammar->terminals[c]
                                 : endMarker;
        width[c] = (int)strlen(header);
        if (width[c] < 3) {
            width[c] = 3;
        }
    }
    for (int r = 0; r < grammar->nonTerminalCount; r++) {
        for (int c = 0; c < cols; c++) {
            char text[MAX_RHS_LENGTH];
            tableCellText(grammar, table, r, c, text, sizeof(text));
            int l = (int)strlen(text);
            if (l > width[c]) {
                width[c] = l;
            }
        }
    }
    for (int c = 0; c < cols; c++) {
        if (width[c] > TABLE_CELL_TEXT_MAX) {
            width[c] = TABLE_CELL_TEXT_MAX;
        }
    }

    /* Header row. */
    printf("%-*s ", nameWidth, "");
    for (int c = 0; c < cols; c++) {
        const char *header = (c < grammar->terminalCount)
                                 ? grammar->terminals[c]
                                 : endMarker;
        printf("| %-*s ", width[c], header);
    }
    printf("|\n");

    /* Separator. */
    int total = nameWidth + 1;
    for (int c = 0; c < cols; c++) {
        total += width[c] + 4;
    }
    total += 1;
    for (int i = 0; i < total; i++) {
        printf("-");
    }
    printf("\n");

    /* One row per non-terminal. */
    for (int r = 0; r < grammar->nonTerminalCount; r++) {
        printf("%-*s ", nameWidth, grammar->nonTerminals[r]);
        for (int c = 0; c < cols; c++) {
            char text[MAX_RHS_LENGTH];
            tableCellText(grammar, table, r, c, text, sizeof(text));
            printf("| %-*s ", width[c], text);
        }
        printf("|\n");
    }
    printf("==============================================\n");
}

/* ======================================================================
 * Conflict report / LL(1) verdict
 * ====================================================================== */

static void conflictProductionText(const Grammar *grammar, int lhsIndex,
                                   int altIndex, char *out, size_t outSize) {
    out[0] = '\0';
    if (lhsIndex < 0 || lhsIndex >= grammar->productionCount) {
        snprintf(out, outSize, "?");
        return;
    }
    const Production *prod = &grammar->productions[lhsIndex];
    char altText[MAX_RHS_LENGTH];
    productionAltToString(prod, altIndex, altText, sizeof(altText));
    snprintf(out, outSize, "%s -> %s", prod->lhs, altText);
}

void displayConflicts(const Grammar *grammar, const ParsingTable *table) {
    printf("\n========== LL(1) ANALYSIS ==========\n");
    if (table->conflictCount == 0) {
        printf("No LL(1) conflicts detected.\n");
        printf("Grammar is LL(1).\n");
    } else {
        printf("Grammar is NOT LL(1): %d conflict(s) detected.\n\n",
               table->conflictCount);
        for (int i = 0; i < table->conflictCount; i++) {
            const TableConflict *cf = &table->conflicts[i];
            char existing[MAX_RHS_LENGTH + 16];
            char conflicting[MAX_RHS_LENGTH + 16];
            conflictProductionText(grammar, cf->lhsIndex2, cf->altIndex2,
                                   existing, sizeof(existing));
            conflictProductionText(grammar, cf->lhsIndex, cf->altIndex,
                                   conflicting, sizeof(conflicting));
            printf("Conflict detected at M[%s, %s]\n",
                   (cf->lhsIndex >= 0 && cf->lhsIndex < grammar->nonTerminalCount)
                       ? grammar->nonTerminals[cf->lhsIndex]
                       : "?",
                   cf->terminal);
            printf("Production 1: %s\n", existing);
            printf("Production 2: %s\n\n", conflicting);
        }
        printf("Grammar is NOT LL(1).\n");
    }
    printf("====================================\n");
}
