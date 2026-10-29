#ifndef PARSER_H
#define PARSER_H

/*
 * parser.h - Module 5 (Member 5: 24BDS0044)
 *
 * Table-driven LL(1) predictive parsing.
 *
 * The parser maintains:
 *   - an explicit stack (initially "$", then the start symbol on top)
 *   - the input buffer terminated by "$"
 *   - the predictive parsing table M built by Module 4
 *
 * Algorithm (classic predictive parsing):
 *   repeat
 *     X = top of stack; a = current input token
 *     if X is a terminal and X == a: pop, advance input
 *     else if X is a terminal and X != a: error (mismatch)
 *     else if X == "$": accept iff a == "$", else error
 *     else if M[X, a] is empty: error (no production predicted)
 *     else: pop X, push RHS of M[X, a] in reverse, record the step
 *   until accept or error
 *
 * Tokenization: the input string is split on whitespace, exactly like
 * grammar symbols. Multi-character tokens such as "id" or "num" work.
 */

#include "grammar.h"
#include "parsing_table.h"


typedef struct {
    char stack[MAX_STACK_DEPTH][MAX_SYMBOL_LENGTH];
    int  stackTop;                       /* index of top element, -1 empty */
    char input[MAX_INPUT_TOKENS][MAX_TOKEN_LENGTH];
    int  inputCount;
    int  inputPos;                       /* 0-based lookahead position     */
    int  stepNumber;
    int  accepted;
    char lastError[256];
} ParserState;

void initializeParserState(ParserState *ps);

/* Splits a whitespace-separated input string into tokens and appends
 * the "$" end marker. Returns 1 on success, 0 if too many tokens. */
int  parserTokenizeInput(const char *input, ParserState *ps);

/* Runs the table-driven parse on a pre-built table. Writes a formatted
 * trace (Stack | Input | Action) to `out` (may be NULL to disable).
 * Returns 1 if the input is accepted, 0 otherwise; ps->lastError holds
 * a diagnostic with the offending token and position. */
int  parserRun(const Grammar *grammar,
               const ParsingTable *table,
               ParserState *ps,
               char *traceOut, size_t traceSize);

/* Interactive prompt for input (used by the menu in main.c). */
int  parserParseInputInteractive(const Grammar *grammar,
                                 const ParsingTable *table);

/* ---- Semantic action hooks (used by the SDT module) ------------------- */

/* The parser fires three kinds of events so syntax-directed translation
 * can attach semantic behavior to parsing WITHOUT duplicating the parse
 * loop (see translation.c):
 *
 *   SHIFT   - terminal `terminal` was matched and consumed
 *   REDUCE  - production (lhsIndex, altIndex) of grammar->productions is
 *             fully processed (all of its RHS symbols handled)
 *   ACCEPT  - the whole input was parsed successfully
 */
typedef enum {
    PARSER_ACTION_SHIFT,
    PARSER_ACTION_REDUCE,
    PARSER_ACTION_ACCEPT
} ParserActionKind;

typedef void (*ParserActionFn)(void *ctx, ParserActionKind kind,
                               int lhsIndex, int altIndex,
                               const char *terminal);

/* Additive step-recorder (used by the JSON export mode, see
 * tools/json_main.c + src/json_export.c; off by default). Fires ONCE
 * per trace row, BEFORE the step mutates the stack or the input, so the
 * callback observes the pre-action configuration:
 *   ps        - stack inputPos etc. (markers may sit in ps->stack slots)
 *   isMarker  - parallel to ps->stack; skip stack[i] where isMarker[i]
 *   actionText- the trace action column ("E -> T Ep", "match id",
 *               "accept")
 * Error exits fire no callback; callers build the failing step from
 * ps->lastError + ps stack/inputPos (filter empty-string marker slots). */
typedef void (*ParserStepFn)(void *ctx, const ParserState *ps,
                             const unsigned char *isMarker,
                             const char *actionText);

/* As parserRun(), but additionally fires `action(actionCtx, ...)` at the
 * SHIFT/REDUCE/ACCEPT points. Pass action == NULL for plain parsing. */
int  parserRunWithActions(const Grammar *grammar,
                          const ParsingTable *table,
                          ParserState *ps,
                          ParserActionFn action, void *actionCtx,
                          char *traceOut, size_t traceSize);

/* Same loop as parserRunWithActions, additionally recording every trace
 * row through stepFn/stepCtx (additive; default OFF for all existing
 * callers). */
int  parserRunRecordSteps(const Grammar *grammar,
                          const ParsingTable *table,
                          ParserState *ps,
                          ParserActionFn action, void *actionCtx,
                          ParserStepFn stepFn, void *stepCtx,
                          char *traceOut, size_t traceSize);

#endif /* PARSER_H */
