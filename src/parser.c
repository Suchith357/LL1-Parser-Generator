/*
 * parser.c - Module 5 (Member 5: 24BDS0044)
 *
 * Table-driven LL(1) predictive parsing.
 *
 * The parser keeps an explicit stack (bottom marker "$", start symbol on
 * top initially), the tokenized input terminated by "$", and consults
 * the predictive parsing table M built by Module 4:
 *
 *   repeat
 *     X = top of stack; a = current input token
 *     if X == "$":            accept iff a == "$", else error
 *     if X is a terminal:     match (pop, advance) or error on mismatch
 *     if X is a non-terminal: M[X, a] must exist; pop X, push the RHS
 *                             symbols in REVERSE order (epsilon pushes
 *                             nothing)
 *   until accept or error
 *
 * Trace format (classic textbook layout; markers are never shown):
 *
 *   Step  Stack          Input           Action
 *   ----------------------------------------------------
 *   1     $ E            id + id $       E -> T Ep
 *
 * Syntax-directed translation support: for every expansion the parser
 * first pushes an invisible REDUCE marker below the RHS symbols. When a
 * marker surfaces, the corresponding production is complete and the
 * registered ParserActionFn fires. This gives translation.c exact
 * SHIFT / REDUCE / ACCEPT events without duplicating the parse loop.
 * Markers live in local parallel arrays; they are not grammar symbols
 * and never appear in the trace or in ParserState.stack content checks.
 *
 * Safety: bounded stacks, a step cap (MAX_PARSE_STEPS) that catches
 * runaway expansions, and full diagnostics with token positions.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "parser.h"

#define TRACE_LINE_MAX   512
#define TRACE_HEADER \
    "Step  Stack          Input           Action\n" \
    "----------------------------------------------------\n"

/* ======================================================================
 * State helpers
 * ====================================================================== */

void initializeParserState(ParserState *ps) {
    ps->stackTop = -1;
    ps->inputCount = 0;
    ps->inputPos = 0;
    ps->stepNumber = 0;
    ps->accepted = 0;
    ps->lastError[0] = '\0';
}

static void parserFail(ParserState *ps, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ps->lastError, sizeof(ps->lastError), fmt, ap);
    va_end(ap);
}

/* Push one grammar symbol; returns 0 when the stack is full. */
static int parserPush(ParserState *ps, const char *symbol) {
    if (ps->stackTop + 1 >= MAX_STACK_DEPTH) {
        return 0;
    }
    ps->stackTop++;
    snprintf(ps->stack[ps->stackTop], MAX_SYMBOL_LENGTH, "%s", symbol);
    return 1;
}

/* ======================================================================
 * Tokenization
 * ====================================================================== */

int parserTokenizeInput(const char *input, ParserState *ps) {
    ps->inputCount = 0;
    ps->inputPos = 0;
    ps->lastError[0] = '\0';

    char buffer[MAX_LINE_LENGTH];
    snprintf(buffer, sizeof(buffer), "%s", input ? input : "");

    for (char *tok = strtok(buffer, " \t");
         tok != NULL;
         tok = strtok(NULL, " \t")) {
        if (ps->inputCount >= MAX_INPUT_TOKENS - 1) {
            parserFail(ps, "too many input tokens (max %d)",
                       MAX_INPUT_TOKENS - 1);
            return 0;
        }
        if (strlen(tok) >= MAX_TOKEN_LENGTH) {
            parserFail(ps,
                       "input token '%s' is too long (max %d characters)",
                       tok, MAX_TOKEN_LENGTH - 1);
            return 0;
        }
        snprintf(ps->input[ps->inputCount], MAX_TOKEN_LENGTH, "%s", tok);
        ps->inputCount++;
    }

    /* Append the end marker; it is token number inputCount. */
    snprintf(ps->input[ps->inputCount], MAX_TOKEN_LENGTH, "%c", END_MARKER);
    ps->inputCount++;
    return 1;
}

/* ======================================================================
 * Trace rendering
 * ====================================================================== */

static void traceAppend(char *out, size_t outSize, size_t *used,
                        int *truncated, const char *line) {
    if (out == NULL || *truncated) {
        return;
    }
    if (*used >= outSize) {
        *truncated = 1;
        return;
    }
    int w = snprintf(out + *used, outSize - *used, "%s", line);
    if (w < 0) {
        *truncated = 1;
        return;
    }
    if ((size_t)w >= outSize - *used) {
        *used += outSize - *used - 1; /* snprintf wrote a truncated prefix */
        *truncated = 1;
        return;
    }
    *used += (size_t)w;
}

/* Stack rendered bottom-to-top ("$ E"), markers hidden, bounded. */
static void buildStackDisplay(const ParserState *ps,
                              const unsigned char *isMarker,
                              char *out, size_t outSize) {
    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i <= ps->stackTop; i++) {
        if (isMarker[i]) {
            continue;
        }
        const char *sym = ps->stack[i];
        size_t len = strlen(sym);
        if (used + len + 6 > outSize) { /* room for " ..." */
            snprintf(out + used, outSize - used, " ...");
            return;
        }
        if (used > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, sym, len + 1);
        used += len;
    }
}

/* Remaining input tokens joined with spaces ("id + id $"). */
static void buildInputDisplay(const ParserState *ps,
                              char *out, size_t outSize) {
    size_t used = 0;
    out[0] = '\0';
    for (int i = ps->inputPos; i < ps->inputCount; i++) {
        const char *tok = ps->input[i];
        size_t len = strlen(tok);
        if (used + len + 6 > outSize) {
            snprintf(out + used, outSize - used, " ...");
            return;
        }
        if (used > 0) {
            out[used++] = ' ';
        }
        memcpy(out + used, tok, len + 1);
        used += len;
    }
}

static void traceStep(char *traceOut, size_t traceSize, size_t *used,
                      int *truncated, ParserState *ps,
                      const unsigned char *isMarker, const char *actionText) {
    char stackText[256];
    char inputText[256];
    char line[TRACE_LINE_MAX];

    buildStackDisplay(ps, isMarker, stackText, sizeof(stackText));
    buildInputDisplay(ps, inputText, sizeof(inputText));
    snprintf(line, sizeof(line), "%-5d %-14s %-15s %s\n",
             ps->stepNumber + 1, stackText, inputText, actionText);
    ps->stepNumber++;
    traceAppend(traceOut, traceSize, used, truncated, line);
}

/* ======================================================================
 * Parse loop
 * ====================================================================== */

int parserRunWithActions(const Grammar *grammar,
                         const ParsingTable *table,
                         ParserState *ps,
                         ParserActionFn action, void *actionCtx,
                         char *traceOut, size_t traceSize) {
    size_t used = 0;
    int truncated = 0;

    /* Local marker bookkeeping, parallel to ps->stack. Markers are NOT
     * grammar symbols: they record "the production pushed below me is
     * complete" for the SDT hook. */
    unsigned char isMarker[MAX_STACK_DEPTH];
    int markerLhs[MAX_STACK_DEPTH];
    int markerAlt[MAX_STACK_DEPTH];
    memset(isMarker, 0, sizeof(isMarker));

    ps->stackTop = -1;
    ps->inputPos = 0;
    ps->stepNumber = 0;
    ps->accepted = 0;
    ps->lastError[0] = '\0';

    char endMarker[2] = { END_MARKER, '\0' };

    /* Initial configuration: "$" at the bottom, start symbol on top. */
    if (!parserPush(ps, endMarker) ||
        !parserPush(ps, grammar->startSymbol)) {
        parserFail(ps, "parse stack overflow during initialization");
        return 0;
    }

    traceAppend(traceOut, traceSize, &used, &truncated, TRACE_HEADER);

    while (!ps->accepted) {
        if (ps->stepNumber >= MAX_PARSE_STEPS) {
            parserFail(ps,
                       "parse aborted: exceeded %d steps (possible "
                       "runaway expansion)", MAX_PARSE_STEPS);
            return 0;
        }
        if (ps->stackTop < 0) {
            parserFail(ps, "internal error: parse stack underflow");
            return 0;
        }

        /* Fire REDUCE actions for completed productions: a marker on
         * top means everything above it has been fully processed. */
        while (ps->stackTop >= 0 && isMarker[ps->stackTop]) {
            int lhs = markerLhs[ps->stackTop];
            int alt = markerAlt[ps->stackTop];
            isMarker[ps->stackTop] = 0;
            ps->stackTop--;
            if (action != NULL) {
                action(actionCtx, PARSER_ACTION_REDUCE, lhs, alt, NULL);
            }
        }
        if (ps->stackTop < 0) {
            parserFail(ps, "internal error: parse stack underflow");
            return 0;
        }

        const char *top = ps->stack[ps->stackTop];
        const char *current = (ps->inputPos < ps->inputCount)
                                  ? ps->input[ps->inputPos]
                                  : endMarker;
        int position = ps->inputPos + 1; /* 1-based token position */

        /* Bottom of the stack: accept or "trailing garbage". */
        if (strcmp(top, endMarker) == 0) {
            if (strcmp(current, endMarker) == 0) {
                traceStep(traceOut, traceSize, &used, &truncated, ps,
                          isMarker, "accept");
                ps->accepted = 1;
                if (action != NULL) {
                    action(actionCtx, PARSER_ACTION_ACCEPT,
                           INVALID_INDEX, INVALID_INDEX, NULL);
                }
                break;
            }
            parserFail(ps,
                       "Unexpected token '%s' (position %d) after the "
                       "parse was complete\nExpected: end of input '%c'",
                       current, position, END_MARKER);
            return 0;
        }

        if (grammarIsTerminal(grammar, top)) {
            /* Terminal on the stack: it must match the lookahead. */
            if (strcmp(top, current) == 0) {
                char actionText[MAX_ACTION_LENGTH];
                snprintf(actionText, sizeof(actionText), "match %s", top);
                traceStep(traceOut, traceSize, &used, &truncated, ps,
                          isMarker, actionText);
                if (action != NULL) {
                    action(actionCtx, PARSER_ACTION_SHIFT,
                           INVALID_INDEX, INVALID_INDEX, top);
                }
                ps->stackTop--;
                ps->inputPos++;
            } else {
                parserFail(ps,
                           "Unexpected token '%s' (position %d)\n"
                           "Expected: '%s'",
                           current, position, top);
                return 0;
            }
            continue;
        }

        if (!grammarIsNonTerminal(grammar, top)) {
            parserFail(ps, "internal error: unknown stack symbol '%s'", top);
            return 0;
        }

        /* Non-terminal: consult the table. */
        int ntIndex = grammarNonTerminalIndex(grammar, top);
        int col;
        if (strcmp(current, endMarker) == 0) {
            col = table->terminalCount; /* the "$" column */
        } else {
            col = grammarTerminalIndex(grammar, current);
            if (col == INVALID_INDEX) {
                parserFail(ps,
                           "Unexpected token '%s' (position %d): it is "
                           "not a terminal of the grammar",
                           current, position);
                return 0;
            }
        }

        if (!tableHasEntry(table, ntIndex, col)) {
            /* Build the "expected one of" list from the row. */
            char expected[512];
            size_t eUsed = 0;
            expected[0] = '\0';
            for (int c = 0; c <= table->terminalCount; c++) {
                if (!tableHasEntry(table, ntIndex, c)) {
                    continue;
                }
                const char *name =
                    (c < table->terminalCount)
                        ? grammar->terminals[c]
                        : endMarker;
                int w = snprintf(expected + eUsed, sizeof(expected) - eUsed,
                                 "%s%s", (eUsed > 0) ? ", " : "", name);
                if (w < 0 || (size_t)w >= sizeof(expected) - eUsed) {
                    break;
                }
                eUsed += (size_t)w;
            }
            parserFail(ps,
                       "Unexpected token '%s' (position %d)\n"
                       "No parsing-table entry for [%s, %s]\n"
                       "Expected one of: %s",
                       current, position, top, current,
                       (eUsed > 0) ? expected : "(none)");
            return 0;
        }

        int lhsIndex = table->cells[ntIndex][col].lhsIndex;
        int altIndex = table->cells[ntIndex][col].altIndex;
        if (lhsIndex < 0 || lhsIndex >= grammar->productionCount ||
            altIndex < 0 ||
            altIndex >= grammar->productions[lhsIndex].rhsCount) {
            parserFail(ps, "internal error: corrupt table entry");
            return 0;
        }

        const Production *prod = &grammar->productions[lhsIndex];
        const char *altTextRaw = prod->rhs[altIndex];

        char actionText[MAX_ACTION_LENGTH];
        snprintf(actionText, sizeof(actionText), "%s -> %s",
                 prod->lhs, altTextRaw);
        traceStep(traceOut, traceSize, &used, &truncated, ps, isMarker,
                  actionText);

        ps->stackTop--; /* pop the non-terminal */

        if (productionAltIsEpsilon(altTextRaw)) {
            /* Epsilon: nothing to push, the production is immediately
             * complete, so fire the reduce now. */
            if (action != NULL) {
                action(actionCtx, PARSER_ACTION_REDUCE, lhsIndex, altIndex,
                       NULL);
            }
            continue;
        }

        /* Push a REDUCE marker first, then the RHS in reverse order so
         * the first RHS symbol ends up on top. The marker surfaces when
         * the whole RHS has been processed. */
        if (ps->stackTop + 1 >= MAX_STACK_DEPTH) {
            parserFail(ps, "parse stack overflow (max depth %d)",
                       MAX_STACK_DEPTH);
            return 0;
        }
        ps->stackTop++;
        ps->stack[ps->stackTop][0] = '\0'; /* marker slot: unused text */
        isMarker[ps->stackTop] = 1;
        markerLhs[ps->stackTop] = lhsIndex;
        markerAlt[ps->stackTop] = altIndex;

        int n = productionAltSymbolCount(altTextRaw);
        for (int i = n - 1; i >= 0; i--) {
            char sym[MAX_SYMBOL_LENGTH];
            if (!productionAltGetSymbol(altTextRaw, i, sym)) {
                break;
            }
            if (!parserPush(ps, sym)) {
                parserFail(ps, "parse stack overflow (max depth %d)",
                           MAX_STACK_DEPTH);
                return 0;
            }
        }
    }

    if (truncated && traceOut != NULL) {
        int t2 = 0;
        traceAppend(traceOut, traceSize, &used, &t2,
                    "...(trace truncated)\n");
    }
    return 1; /* accepted */
}

int parserRun(const Grammar *grammar,
              const ParsingTable *table,
              ParserState *ps,
              char *traceOut, size_t traceSize) {
    return parserRunWithActions(grammar, table, ps, NULL, NULL,
                                traceOut, traceSize);
}

/* ======================================================================
 * Interactive entry point (menu option 7 in main.c)
 * ====================================================================== */

int parserParseInputInteractive(const Grammar *grammar,
                                const ParsingTable *table) {
    char line[MAX_LINE_LENGTH];
    static char trace[16384]; /* large enough for course-scale inputs */

    printf("Enter input string (tokens separated by spaces): ");
    fflush(stdout);
    if (fgets(line, sizeof(line), stdin) == NULL) {
        printf("\nNo input read.\n");
        return 0;
    }
    line[strcspn(line, "\r\n")] = '\0';

    ParserState ps;
    initializeParserState(&ps);
    if (!parserTokenizeInput(line, &ps)) {
        printf("Error: %s\n", ps.lastError);
        return 0;
    }

    int ok = parserRun(grammar, table, &ps, trace, sizeof(trace));
    printf("%s", trace);
    if (ok) {
        printf("Result: INPUT ACCEPTED\n");
    } else {
        printf("Result: SYNTAX ERROR\n%s\n", ps.lastError);
    }
    return ok;
}
