#ifndef PARSING_TABLE_H
#define PARSING_TABLE_H

/*
 * parsing_table.h - Module 4 (Member 4: 24bci0103)
 *
 * LL(1) predictive parsing table construction and conflict detection.
 *
 * For every production A -> alpha:
 *   - for every terminal a in FIRST(alpha) \ {#}:  M[A, a] = A -> alpha
 *   - if # in FIRST(alpha): for every b in FOLLOW(A):  M[A, b] = A -> alpha
 *
 * If two productions compete for the same cell M[A, a], the grammar is
 * NOT LL(1) and the conflict is recorded (never silently overwritten).
 */

#include "grammar.h"
#include "first.h"
#include "follow.h"

typedef struct {
    /* Production origin of the entry: (lhsIndex, altIndex) into
     * grammar->productions[lhsIndex].rhs[altIndex]. INVALID_INDEX for
     * an empty cell. Entries store indices only, so no production text
     * is duplicated. */
    int lhsIndex;
    int altIndex;
} TableCell;

typedef struct {
    int lhsIndex;
    int altIndex;   /* second competing production */
    int altIndex2;  /* production already in the cell */
    int lhsIndex2;
    char terminal[MAX_SYMBOL_LENGTH];
} TableConflict;

typedef struct {
    /* M[nonTerminalIndex][terminalIndex]; "$" is the last column. */
    TableCell cells[MAX_TABLE_ROWS][MAX_TABLE_COLS];
    int nonTerminalCount;
    int terminalCount;   /* grammar terminals; "$" handled as +1 */
    int conflictCount;
    TableConflict conflicts[MAX_CONFLICTS];
} ParsingTable;

void initializeParsingTable(ParsingTable *table);

/* Fills M from FIRST/FOLLOW and collects conflicts. Returns 1 if the
 * table is conflict-free (grammar is LL(1)), 0 otherwise. The table is
 * still usable for parsing when conflicts exist only if the user
 * resolves them manually; main() should refuse to parse in that case. */
int  constructParsingTable(const Grammar *grammar,
                           const FirstCollection *first,
                           const FollowCollection *follow,
                           ParsingTable *table);

/* 1 iff a production was predicted for M[ntIndex, termIndex]. */
int  tableHasEntry(const ParsingTable *table, int ntIndex, int termIndex);

/* Human-readable entry text "E -> T Ep" into out. 1/0 if empty. */
int  tableEntryToString(const Grammar *grammar, const ParsingTable *table,
                        int ntIndex, int termIndex,
                        char *out, size_t outSize);

void displayParsingTable(const Grammar *grammar, const ParsingTable *table);

void displayConflicts(const Grammar *grammar, const ParsingTable *table);

#endif /* PARSING_TABLE_H */
