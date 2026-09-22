/*
 * main.c - Integration driver for the LL(1) Parser Generator.
 *
 * Usage:
 *   ./ll1                  menu-driven mode (options 1..9)
 *   ./ll1 grammar.txt      automatic pipeline; afterwards each stdin line
 *                          is parsed and translated (EOF finishes)
 *   ./ll1 - < grammar.txt  automatic pipeline with the grammar on stdin;
 *                          stdin is then exhausted, so the parse phase
 *                          is skipped
 *
 * Pipeline (automatic mode):
 *   Load Grammar -> Validate -> Identify Symbols -> FIRST -> FOLLOW ->
 *   Parsing Table -> LL(1) Conflict Detection -> [Parsing + SDT]
 *
 * Parsing and translation only run for conflict-free (LL(1)) grammars;
 * a conflicting grammar is reported and the run ends there, because the
 * table cannot decide productions deterministically.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "grammar.h"
#include "first.h"
#include "follow.h"
#include "parsing_table.h"
#include "parser.h"
#include "translation.h"

/* ======================================================================
 * Application state
 * ====================================================================== */

typedef struct {
    Grammar          grammar;
    FirstCollection  first;
    FollowCollection follow;
    ParsingTable     table;

    int grammarLoaded;
    int firstComputed;
    int followComputed;
    int tableBuilt;
    int isLL1;
    int grammarFromStdin;
} AppState;

static void printBanner(void) {
    printf("=============================================\n");
    printf("         LL(1) PARSER GENERATOR\n");
    printf("=============================================\n");
    printf("Team 4 - Compiler Design project\n");
    printf("Automatic FIRST/FOLLOW, LL(1) table, predictive\n");
    printf("parsing and syntax-directed translation.\n\n");
}

/* ======================================================================
 * Lazy pipeline stages (menu support; automatic mode calls in order)
 * ====================================================================== */

static int ensureFirst(AppState *st) {
    if (st->firstComputed) {
        return 1;
    }
    if (!st->grammarLoaded) {
        printf("Load a grammar first (menu option 1).\n");
        return 0;
    }
    if (!computeFirstSets(&st->grammar, &st->first)) {
        printf("Error: FIRST computation failed.\n");
        return 0;
    }
    st->firstComputed = 1;
    return 1;
}

static int ensureFollow(AppState *st) {
    if (st->followComputed) {
        return 1;
    }
    if (!ensureFirst(st)) {
        return 0;
    }
    if (!computeFollowSets(&st->grammar, &st->first, &st->follow)) {
        printf("Error: FOLLOW computation failed.\n");
        return 0;
    }
    st->followComputed = 1;
    return 1;
}

static int ensureTable(AppState *st) {
    if (st->tableBuilt) {
        return 1;
    }
    if (!ensureFollow(st)) {
        return 0;
    }
    st->isLL1 = constructParsingTable(&st->grammar, &st->first,
                                      &st->follow, &st->table);
    st->tableBuilt = 1;
    return 1;
}

/* ======================================================================
 * Grammar loading
 * ====================================================================== */

/* Loads a grammar from a path, or from stdin when path is "-".
 * Returns 1 on success. */
static int loadGrammarFromPath(AppState *st, const char *path,
                               int fromStdin) {
    char errorOut[512];

    initializeGrammar(&st->grammar);
    st->grammarLoaded = 0;
    st->firstComputed = 0;
    st->followComputed = 0;
    st->tableBuilt = 0;
    st->isLL1 = 0;
    st->grammarFromStdin = fromStdin;

    if (fromStdin) {
        if (!readGrammarFromStream(&st->grammar, stdin,
                                   errorOut, sizeof(errorOut))) {
            printf("Error: invalid grammar: %s\n", errorOut);
            return 0;
        }
    } else {
        FILE *fp = fopen(path, "r");
        if (fp == NULL) {
            printf("Error: cannot open grammar file '%s'.\n", path);
            return 0;
        }
        int ok = readGrammarFromStream(&st->grammar, fp,
                                       errorOut, sizeof(errorOut));
        fclose(fp);
        if (!ok) {
            printf("Error: invalid grammar: %s\n", errorOut);
            return 0;
        }
    }

    st->grammarLoaded = 1;
    printf("Grammar loaded successfully.\n");
    return 1;
}

/* ======================================================================
 * Parsing + translation phase
 * ====================================================================== */

static void parseAndTranslateOne(AppState *st, const char *input) {
    ParserState ps;
    initializeParserState(&ps);

    if (!parserTokenizeInput(input, &ps)) {
        printf("Error: %s\n", ps.lastError);
        return;
    }

    static char trace[16384]; /* course-scale traces fit comfortably */
    int accepted = parserRun(&st->grammar, &st->table, &ps,
                             trace, sizeof(trace));
    printf("%s", trace);

    if (accepted) {
        printf("Result: INPUT ACCEPTED\n");
        TranslationResult tr;
        if (translateInput(&st->grammar, &st->table, input, &tr)) {
            displayTranslation(&tr);
        } else {
            printf("Translation failed: %s\n", tr.lastError);
        }
    } else {
        printf("Result: SYNTAX ERROR\n%s\n", ps.lastError);
    }
}

/* Reads input strings from stdin, one per line, and parses/translates
 * each. An empty line is the epsilon input. */
static void parseInputsFromStdin(AppState *st) {
    char line[MAX_LINE_LENGTH];

    printf("\n---------- PREDICTIVE PARSING + SDT ----------\n");
    printf("Enter input strings (one per line, empty line = epsilon\n");
    printf("input, 'quit' or EOF/Ctrl+D to finish):\n");

    while (fgets(line, sizeof(line), stdin) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            break;
        }
        printf("\nInput: \"%s\"\n", line);
        parseAndTranslateOne(st, line);
    }
}

/* ======================================================================
 * Automatic pipeline (file / stdin argument mode)
 * ====================================================================== */

static void runAutomaticPipeline(AppState *st) {
    displayGrammar(&st->grammar);
    displaySymbols(&st->grammar);

    if (!ensureFirst(st)) {
        return;
    }
    displayFirstSets(&st->first);

    if (!ensureFollow(st)) {
        return;
    }
    displayFollowSets(&st->follow);

    if (!ensureTable(st)) {
        return;
    }
    displayParsingTable(&st->grammar, &st->table);
    displayConflicts(&st->grammar, &st->table);

    if (!st->isLL1) {
        printf("\nParsing and translation are skipped: the grammar is\n");
        printf("not LL(1), so the table above cannot decide productions\n");
        printf("deterministically. Eliminate the conflicts and retry.\n");
        return;
    }

    if (st->grammarFromStdin) {
        printf("\n(Stdin was used for the grammar, so no input strings\n");
        printf("are available here. Run './ll1 grammar.txt' and type\n");
        printf("inputs, or start './ll1' for the menu.)\n");
        return;
    }

    parseInputsFromStdin(st);
}

/* ======================================================================
 * Menu mode
 * ====================================================================== */

static void menuLoadGrammar(AppState *st) {
    char path[MAX_LINE_LENGTH];

    printf("Enter grammar file path (empty = type productions here):\n");
    printf("> ");
    fflush(stdout);
    if (fgets(path, sizeof(path), stdin) == NULL) {
        printf("\nNo input read.\n");
        return;
    }
    path[strcspn(path, "\r\n")] = '\0';

    char *trimmed = utilsTrim(path);
    if (trimmed[0] == '\0') {
        /* Interactive entry: prompts for count + production lines. */
        initializeGrammar(&st->grammar);
        st->grammarLoaded = 0;
        st->firstComputed = 0;
        st->followComputed = 0;
        st->tableBuilt = 0;
        st->isLL1 = 0;
        st->grammarFromStdin = 0;
        if (readGrammar(&st->grammar)) {
            st->grammarLoaded = 1;
            printf("Grammar loaded successfully.\n");
        } else {
            printf("\nFailed to load grammar.\n");
        }
        return;
    }
    loadGrammarFromPath(st, trimmed, 0);
}

static void menuTranslateInteractive(AppState *st) {
    char line[MAX_LINE_LENGTH];

    printf("Enter expression to translate (tokens separated by spaces):\n");
    printf("> ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        printf("\nNo input read.\n");
        return;
    }
    line[strcspn(line, "\r\n")] = '\0';

    printf("\nInput: \"%s\"\n", line);

    ParserState ps;
    initializeParserState(&ps);
    if (!parserTokenizeInput(line, &ps)) {
        printf("Error: %s\n", ps.lastError);
        return;
    }
    static char trace[16384];
    if (!parserRun(&st->grammar, &st->table, &ps, trace, sizeof(trace))) {
        printf("%s", trace);
        printf("Result: SYNTAX ERROR\n%s\n", ps.lastError);
        return;
    }
    printf("Result: INPUT ACCEPTED (syntax is valid)\n");
    printf("Syntax-directed translation of the same input:\n");

    TranslationResult tr;
    if (translateInput(&st->grammar, &st->table, line, &tr)) {
        displayTranslation(&tr);
    } else {
        printf("Translation failed: %s\n", tr.lastError);
    }
}

static void runMenu(AppState *st) {
    char choiceLine[64];

    for (;;) {
        printf("\n=========================================\n");
        printf("        LL(1) PARSER GENERATOR - MENU\n");
        printf("=========================================\n");
        printf("1. Load Grammar\n");
        printf("2. Display Grammar\n");
        printf("3. Compute FIRST Sets\n");
        printf("4. Compute FOLLOW Sets\n");
        printf("5. Construct Parsing Table\n");
        printf("6. Check LL(1)\n");
        printf("7. Parse Input\n");
        printf("8. Syntax-Directed Translation\n");
        printf("9. Exit\n");
        printf("Choice: ");
        fflush(stdout);

        if (fgets(choiceLine, sizeof(choiceLine), stdin) == NULL) {
            printf("\n");
            return;
        }

        int choice = atoi(choiceLine);
        switch (choice) {
        case 1:
            menuLoadGrammar(st);
            break;
        case 2:
            if (!st->grammarLoaded) {
                printf("Load a grammar first (option 1).\n");
            } else {
                displayGrammar(&st->grammar);
                displaySymbols(&st->grammar);
            }
            break;
        case 3:
            if (ensureFirst(st)) {
                displayFirstSets(&st->first);
            }
            break;
        case 4:
            if (ensureFollow(st)) {
                displayFollowSets(&st->follow);
            }
            break;
        case 5:
            if (ensureTable(st)) {
                displayParsingTable(&st->grammar, &st->table);
            }
            break;
        case 6:
            if (ensureTable(st)) {
                displayConflicts(&st->grammar, &st->table);
            }
            break;
        case 7:
            if (!ensureTable(st)) {
                break;
            }
            if (!st->isLL1) {
                printf("Grammar is not LL(1); parsing is disabled until\n");
                printf("the conflicts are resolved.\n");
                break;
            }
            parserParseInputInteractive(&st->grammar, &st->table);
            break;
        case 8:
            if (!ensureTable(st)) {
                break;
            }
            if (!st->isLL1) {
                printf("Grammar is not LL(1); translation is disabled\n");
                printf("until the conflicts are resolved.\n");
                break;
            }
            menuTranslateInteractive(st);
            break;
        case 9:
            printf("Goodbye.\n");
            return;
        default:
            printf("Invalid choice (enter 1-9).\n");
            break;
        }
    }
}

/* ======================================================================
 * Entry point
 * ====================================================================== */

int main(int argc, char *argv[]) {
    AppState st;

    initializeGrammar(&st.grammar);
    initializeFirstCollection(&st.first);
    initializeFollowCollection(&st.follow);
    initializeParsingTable(&st.table);
    st.grammarLoaded = 0;
    st.firstComputed = 0;
    st.followComputed = 0;
    st.tableBuilt = 0;
    st.isLL1 = 0;
    st.grammarFromStdin = 0;

    printBanner();

    if (argc >= 2) {
        /* File argument or "-" for stdin. */
        if (!loadGrammarFromPath(&st, argv[1],
                                 strcmp(argv[1], "-") == 0)) {
            return 1;
        }
        runAutomaticPipeline(&st);
    } else {
        runMenu(&st);
    }
    return 0;
}
