#ifndef TRANSLATION_H
#define TRANSLATION_H

/*
 * translation.h - Syntax-Directed Translation component
 * (Member 5 owns the parser+SDT pair; integration shared by all members)
 *
 * The translation is attached to the predictive parser as semantic
 * actions fired at three points of the LL(1) control loop:
 *
 *   (1) On SHIFT of terminal `tok`:     push(tok) onto the semantic stack
 *   (2) On REDUCE using production P:   pop |rhs(P)| semantic values,
 *                                       compute the attribute, push result
 *   (3) On ACCEPT (start symbol reduced with lookahead "$"):
 *                                       the semantic stack holds the final
 *                                       translation
 *
 * This mirrors the SDT scheme written below the grammar:
 *
 *   Ep -> + T Ep   { Ep.val = T.val + Ep1.val }   (left operand already
 *                                                  on the semantic stack)
 *   Ep -> #        { Ep.val = 0 }
 *   Tp -> * F Tp   { Tp.val = F.val * Tp1.val }
 *   Tp -> #        { Tp.val = 1 }
 *   F  -> ( E )    { F.val = E.val }
 *   F  -> id       { F.val = value of id }
 *
 * Because every action is placed at the END of its production, the
 * scheme is post-order and is safe for LL(1) parsing (no inherited
 * attributes are needed for these particular rules).
 */

#include "grammar.h"
#include "parsing_table.h"

/* MAX_SEMANTIC_TEXT, MAX_SEMANTIC_STACK, MAX_POSTFIX_LENGTH,
 * MAX_TAC_* are defined in constants.h (single source of truth). */

typedef struct {
    char text[MAX_SEMANTIC_TEXT];  /* e.g. "a", "b", "t1", "t2" */
} SemanticValue;

typedef struct {
    SemanticValue values[MAX_SEMANTIC_STACK];
    int top;                        /* index of top element, -1 empty */
    int overflow;                   /* set when a push hit the limit  */
} SemanticStack;

typedef struct {
    int tempCount;                  /* temporaries allocated so far  */
    char lines[MAX_TAC_LINES][MAX_TAC_LINE_LENGTH];
    int lineCount;
    char postfix[MAX_POSTFIX_LENGTH]; /* space-separated postfix form */
    char lastError[256];
} TranslationResult;

void initializeSemanticStack(SemanticStack *ss);
void initializeTranslationResult(TranslationResult *tr);

/* Allocates the next temporary name t1, t2, ... into out. */
void translationNewTemp(TranslationResult *tr, char *out, size_t outSize);

/* Emits one three-address instruction, e.g. "t1 = b * c". */
void translationEmit(TranslationResult *tr, const char *line);

/* Appends `token` (and a separating space) to the postfix buffer. */
void translationAppendPostfix(TranslationResult *tr, const char *token);

/* Semantic stack primitives shared with parser.c. */
void semanticPush(SemanticStack *ss, const char *text);
int  semanticPop(SemanticStack *ss, char *out, size_t outSize);

/* Full syntax-directed translation of one input string. Runs the same
 * LL(1) recognition as parserRun but fires the semantic actions above.
 * Returns 1 on success (result filled), 0 on syntax error. */
int  translateInput(const Grammar *grammar,
                    const ParsingTable *table,
                    const char *input,
                    TranslationResult *result);

void displayTranslation(const TranslationResult *result);

#endif /* TRANSLATION_H */
