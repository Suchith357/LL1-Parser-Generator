/*
 * translation.c - Syntax-Directed Translation (Module 6, Member 5:
 *                 24BDS0044 owns parser+SDT; integration shared)
 *
 * The translation is attached to the predictive parser through the
 * ParserActionFn hook (see parser.h). The parser fires:
 *
 *   SHIFT  when a terminal is matched  -> push the token on the
 *                                         semantic stack
 *   REDUCE when a production is done   -> pop |rhs| values, compute the
 *                                         attribute of the LHS, push it
 *   ACCEPT when the input is accepted  -> the semantic stack top is the
 *                                         final translation
 *
 * Translation scheme (actions at the END of each production, so every
 * attribute is synthesized and the scheme is safe for LL(1)):
 *
 *   E  -> T Ep      { E.val  = T.val Ep.val }        (concatenation)
 *   Ep -> + T Ep    { Ep.val = T.val Ep1.val '+' }   (postfix)
 *   Ep -> #         { Ep.val = "" }
 *   T  -> F Tp      { T.val  = F.val Tp.val }
 *   Tp -> * F Tp    { Tp.val = F.val Tp1.val '*' }
 *   Tp -> #         { Tp.val = "" }
 *   F  -> ( E )     { F.val  = E.val }               (drop parentheses)
 *   F  -> id        { F.val  = the token itself }
 *
 * The engine classifies each production by RHS SHAPE (not by hard-coded
 * production text), so it also translates structurally similar grammars
 * automatically:
 *
 *   1. "#"                  -> empty value
 *   2. single symbol        -> identity (F -> id, unit rules)
 *   3. "( X1 ... Xk )"      -> drop the parentheses, concatenate values
 *   4. "op X1 ... Xk" with one leading terminal operator and only
 *      non-terminals after  -> X1.val ... Xk.val op   (postfix form)
 *   5. only non-terminals   -> concatenate values in order
 *   6. anything else        -> concatenate all values (echo; documented
 *      fallback for shapes outside the scheme)
 *
 * Three-address code is generated afterwards from the postfix string by
 * the standard stack evaluation: operands are pushed, and for each
 * operator + - * / a temporary tN = left op right is emitted.
 *
 * Example: input "a + b * c"
 *   postfix  a b c * +
 *   TAC      t1 = b * c
 *            t2 = a + t1
 */

#include <stdio.h>
#include <string.h>

#include "translation.h"
#include "parser.h"

/* ======================================================================
 * Semantic stack primitives
 * ====================================================================== */

void initializeSemanticStack(SemanticStack *ss) {
    ss->top = -1;
    ss->overflow = 0;
}

void semanticPush(SemanticStack *ss, const char *text) {
    if (ss->top + 1 >= MAX_SEMANTIC_STACK) {
        ss->overflow = 1;
        fprintf(stderr,
                "translation: semantic stack overflow (max %d values)\n",
                MAX_SEMANTIC_STACK);
        return;
    }
    ss->top++;
    snprintf(ss->values[ss->top].text, MAX_SEMANTIC_TEXT, "%s",
             text ? text : "");
}

int semanticPop(SemanticStack *ss, char *out, size_t outSize) {
    out[0] = '\0';
    if (ss->top < 0) {
        return 0;
    }
    snprintf(out, outSize, "%s", ss->values[ss->top].text);
    ss->top--;
    return 1;
}

/* ======================================================================
 * Translation result helpers
 * ====================================================================== */

void initializeTranslationResult(TranslationResult *tr) {
    tr->tempCount = 0;
    tr->lineCount = 0;
    tr->postfix[0] = '\0';
    tr->lastError[0] = '\0';
}

void translationNewTemp(TranslationResult *tr, char *out, size_t outSize) {
    if (tr->tempCount >= MAX_TAC_TEMPS) {
        snprintf(out, outSize, "t?");
        snprintf(tr->lastError, sizeof(tr->lastError),
                 "too many temporaries (max %d)", MAX_TAC_TEMPS);
        return;
    }
    tr->tempCount++;
    snprintf(out, outSize, "t%d", tr->tempCount);
}

void translationEmit(TranslationResult *tr, const char *line) {
    if (tr->lineCount >= MAX_TAC_LINES) {
        snprintf(tr->lastError, sizeof(tr->lastError),
                 "too many three-address instructions (max %d)",
                 MAX_TAC_LINES);
        return;
    }
    snprintf(tr->lines[tr->lineCount], MAX_TAC_LINE_LENGTH, "%s", line);
    tr->lineCount++;
}

void translationAppendPostfix(TranslationResult *tr, const char *token) {
    size_t used = strlen(tr->postfix);
    size_t len = strlen(token);
    if (used + len + 2 > sizeof(tr->postfix)) {
        snprintf(tr->lastError, sizeof(tr->lastError),
                 "postfix translation too long (max %d characters)",
                 MAX_POSTFIX_LENGTH - 1);
        return;
    }
    if (used > 0) {
        tr->postfix[used++] = ' ';
    }
    memcpy(tr->postfix + used, token, len + 1);
}

/* Appends a fragment to a value buffer, inserting exactly one space
 * between non-empty fragments ("a" + "b c *" -> "a b c *"). */
static void sdtConcat(char *dst, size_t dstSize, const char *fragment) {
    if (fragment[0] == '\0') {
        return;
    }
    size_t used = strlen(dst);
    size_t len = strlen(fragment);
    if (used + len + 2 > dstSize) {
        return; /* bounds-checked: caller reports overflow via the stack */
    }
    if (used > 0) {
        dst[used++] = ' ';
    }
    memcpy(dst + used, fragment, len + 1);
}

/* ======================================================================
 * The semantic actions
 * ====================================================================== */

typedef struct {
    const Grammar *grammar;
    SemanticStack ss;
    TranslationResult *result;
} SdtContext;

/* Pops the |rhs| values of a completed production. vals[0] is popped
 * first, i.e. the value of the LAST RHS symbol; the value of RHS symbol
 * i (0-based, left to right) therefore lives in vals[n - 1 - i]. */
static void sdtApplyProduction(SdtContext *ctx, int lhsIndex, int altIndex) {
    const Grammar *grammar = ctx->grammar;
    TranslationResult *tr = ctx->result;

    if (lhsIndex < 0 || lhsIndex >= grammar->productionCount ||
        altIndex < 0 || altIndex >= grammar->productions[lhsIndex].rhsCount) {
        snprintf(tr->lastError, sizeof(tr->lastError),
                 "internal error: bad reduce action (%d, %d)",
                 lhsIndex, altIndex);
        return;
    }
    const Production *prod = &grammar->productions[lhsIndex];
    const char *alt = prod->rhs[altIndex];

    char value[MAX_SEMANTIC_TEXT];
    value[0] = '\0';

    if (productionAltIsEpsilon(alt)) {
        /* Rule 1: epsilon contributes the empty string, so enclosing
         * reductions still pop the expected number of values. */
        semanticPush(&ctx->ss, value);
        return;
    }

    int n = productionAltSymbolCount(alt);
    char vals[MAX_RHS_SYMBOLS][MAX_SEMANTIC_TEXT];
    if (n > MAX_RHS_SYMBOLS) {
        snprintf(tr->lastError, sizeof(tr->lastError),
                 "alternative of '%s' is too long for translation",
                 prod->lhs);
        return;
    }
    for (int i = 0; i < n; i++) {
        if (!semanticPop(&ctx->ss, vals[i], sizeof(vals[i]))) {
            snprintf(tr->lastError, sizeof(tr->lastError),
                     "internal error: semantic stack underflow at '%s -> %s'",
                     prod->lhs, alt);
            return;
        }
    }

    /* Shape of the RHS drives the action. */
    char firstSym[MAX_SYMBOL_LENGTH];
    char lastSym[MAX_SYMBOL_LENGTH];
    productionAltGetSymbol(alt, 0, firstSym);
    productionAltGetSymbol(alt, n - 1, lastSym);
    int firstIsTerminal = !grammarIsNonTerminal(grammar, firstSym);
    int lastIsTerminal = !grammarIsNonTerminal(grammar, lastSym);

    int middlesAreNonTerminals = 1;
    for (int i = 1; i < n - 1 && middlesAreNonTerminals; i++) {
        char s[MAX_SYMBOL_LENGTH];
        productionAltGetSymbol(alt, i, s);
        if (grammarIsNonTerminal(grammar, s)) {
            continue;
        }
        middlesAreNonTerminals = 0;
    }

    if (n == 1) {
        /* Rule 2: single symbol - identity. */
        snprintf(value, sizeof(value), "%s", vals[0]);
    } else if (firstIsTerminal && lastIsTerminal &&
               strcmp(firstSym, "(") == 0 && strcmp(lastSym, ")") == 0) {
        /* Rule 3: grouping parentheses - drop them. */
        for (int i = n - 2; i >= 1; i--) {
            sdtConcat(value, sizeof(value), vals[i]);
        }
    } else if (firstIsTerminal && middlesAreNonTerminals && !lastIsTerminal) {
        /* Rule 4: one leading operator terminal, non-terminals after:
         * postfix form = X1.val ... Xk.val op. */
        for (int i = n - 2; i >= 0; i--) {
            sdtConcat(value, sizeof(value), vals[i]);
        }
        sdtConcat(value, sizeof(value), vals[0]);
        sdtConcat(value, sizeof(value), vals[n - 1]);
    } else {
        /* Rules 5/6: concatenate all values left to right. */
        for (int i = n - 1; i >= 0; i--) {
            sdtConcat(value, sizeof(value), vals[i]);
        }
    }

    semanticPush(&ctx->ss, value);
}

/* Parser event hook. */
static void sdtAction(void *vctx, ParserActionKind kind,
                      int lhsIndex, int altIndex, const char *terminal) {
    SdtContext *ctx = (SdtContext *)vctx;

    switch (kind) {
    case PARSER_ACTION_SHIFT:
        if (terminal != NULL) {
            semanticPush(&ctx->ss, terminal);
        }
        break;
    case PARSER_ACTION_REDUCE:
        sdtApplyProduction(ctx, lhsIndex, altIndex);
        break;
    case PARSER_ACTION_ACCEPT: {
        char final[MAX_SEMANTIC_TEXT];
        if (semanticPop(&ctx->ss, final, sizeof(final))) {
            snprintf(ctx->result->postfix, sizeof(ctx->result->postfix),
                     "%s", final);
        }
        break;
    }
    default:
        break;
    }
}

/* ======================================================================
 * Three-address code from the postfix string
 * ====================================================================== */

static int isTacOperator(const char *token) {
    return (token[0] != '\0' && token[1] == '\0') &&
           (token[0] == '+' || token[0] == '-' ||
            token[0] == '*' || token[0] == '/');
}

static void sdtGenerateTAC(TranslationResult *tr) {
    char work[MAX_POSTFIX_LENGTH];
    snprintf(work, sizeof(work), "%s", tr->postfix);

    char names[MAX_INPUT_TOKENS][MAX_TOKEN_LENGTH];
    int top = -1;

    for (char *tok = strtok(work, " ");
         tok != NULL;
         tok = strtok(NULL, " ")) {
        if (!isTacOperator(tok)) {
            if (top + 1 >= MAX_INPUT_TOKENS) {
                snprintf(tr->lastError, sizeof(tr->lastError),
                         "expression too long for three-address code");
                return;
            }
            top++;
            snprintf(names[top], MAX_TOKEN_LENGTH, "%s", tok);
            continue;
        }
        if (top < 1) {
            snprintf(tr->lastError, sizeof(tr->lastError),
                     "malformed postfix expression for three-address code");
            return;
        }
        char right[MAX_TOKEN_LENGTH];
        char left[MAX_TOKEN_LENGTH];
        snprintf(right, sizeof(right), "%s", names[top--]);
        snprintf(left, sizeof(left), "%s", names[top--]);

        char temp[16];
        translationNewTemp(tr, temp, sizeof(temp));
        char line[MAX_TAC_LINE_LENGTH];
        snprintf(line, sizeof(line), "%s = %s %s %s",
                 temp, left, tok, right);
        translationEmit(tr, line);

        top++;
        snprintf(names[top], MAX_TOKEN_LENGTH, "%s", temp);
    }
}

/* ======================================================================
 * Public entry point and display
 * ====================================================================== */

int translateInput(const Grammar *grammar,
                   const ParsingTable *table,
                   const char *input,
                   TranslationResult *result) {
    initializeTranslationResult(result);

    ParserState ps;
    initializeParserState(&ps);
    if (!parserTokenizeInput(input, &ps)) {
        snprintf(result->lastError, sizeof(result->lastError), "%s",
                 ps.lastError);
        return 0;
    }

    SdtContext ctx;
    ctx.grammar = grammar;
    ctx.result = result;
    initializeSemanticStack(&ctx.ss);

    /* The same LL(1) recognition as parserRun, with actions attached.
     * No trace is requested (traceOut = NULL). */
    int ok = parserRunWithActions(grammar, table, &ps, sdtAction, &ctx,
                                  NULL, 0);
    if (!ok) {
        snprintf(result->lastError, sizeof(result->lastError), "%s",
                 ps.lastError);
        return 0;
    }
    if (ctx.ss.overflow || result->lastError[0] != '\0') {
        if (result->lastError[0] == '\0') {
            snprintf(result->lastError, sizeof(result->lastError),
                     "expression too complex for translation (semantic "
                     "stack limit %d)", MAX_SEMANTIC_STACK);
        }
        return 0;
    }

    sdtGenerateTAC(result);
    return 1;
}

void displayTranslation(const TranslationResult *result) {
    printf("\n========== SYNTAX-DIRECTED TRANSLATION ==========\n");
    printf("Postfix: %s\n",
           (result->postfix[0] != '\0') ? result->postfix : "(empty)");
    if (result->lineCount > 0) {
        printf("Three-address code:\n");
        for (int i = 0; i < result->lineCount; i++) {
            printf("  %s\n", result->lines[i]);
        }
    } else {
        printf("Three-address code: (none needed)\n");
    }
    printf("=================================================\n");
}
