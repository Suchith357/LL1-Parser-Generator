/*
 * json_export.c - Additive JSON export for the ParseLab web adapter.
 *
 * Contract (documented in README "Running the website"):
 *   Usage:  ll1json <grammar_file> <input>
 *   Output: ONE JSON document on stdout, one of:
 *     {"ok":false,"stage":"grammar","message":"..."}
 *     {"ok":true,"grammar":...,"first":...,"follow":...,
 *      "table":...,"conflicts":[...],"isLL1":0|1,
 *      "parse":{...},"translation":{...}}
 *
 * NO algorithmic logic lives here: every set, entry, step and line is
 * taken directly from the existing engine functions (grammar.c,
 * first.c, follow.c, parsing_table.c, parser.c, translation.c).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grammar.h"
#include "first.h"
#include "follow.h"
#include "parsing_table.h"
#include "parser.h"
#include "translation.h"
#include "json_export.h"

/* ======================================================================
 * Minimal JSON writer (engine strings are ASCII, messages may embed \n)
 * ====================================================================== */

static void jsonStr(FILE *out, const char *s) {
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)(s ? s : "");
         *p != '\0'; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b", out);  break;
        case '\f': fputs("\\f", out);  break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (*p < 0x20) {
                fprintf(out, "\\u%04X", *p);
            } else {
                fputc(*p, out);
            }
        }
    }
    fputc('"', out);
}

#define JSEP() fputc(',', out)
#define JPUT(c) fputc(c, out)

/* ======================================================================
 * Grammar / set helpers
 * ====================================================================== */

static void jsonProductionGroup(FILE *out, const Production *prod) {
    fputc('{', out);
    jsonStr(out, "lhs"); fputc(':', out); jsonStr(out, prod->lhs);
    JSEP();
    jsonStr(out, "alternatives"); fputc(':', out); fputc('[', out);
    for (int a = 0; a < prod->rhsCount; a++) {
        if (a > 0) JSEP();
        fputc('{', out);
        jsonStr(out, "lhs"); fputc(':', out); jsonStr(out, prod->lhs);
        JSEP();
        jsonStr(out, "alt"); fprintf(out, ":%d", a);
        JSEP();
        int eps = productionAltIsEpsilon(prod->rhs[a]);
        jsonStr(out, "epsilon"); fprintf(out, ":%d", eps ? 1 : 0);
        JSEP();
        jsonStr(out, "symbols"); fputc(':', out); fputc('[', out);
        if (!eps) {
            int n = productionAltSymbolCount(prod->rhs[a]);
            for (int i = 0; i < n; i++) {
                char sym[MAX_SYMBOL_LENGTH];
                if (productionAltGetSymbol(prod->rhs[a], i, sym)) {
                    if (i > 0) JSEP();
                    jsonStr(out, sym);
                }
            }
        }
        JPUT(']');
        JSEP();
        jsonStr(out, "text"); fputc(':', out);
        char text[MAX_RHS_LENGTH];
        productionAltToString(prod, a, text, sizeof(text));
        jsonStr(out, text);
        JPUT('}');
    }
    JPUT(']');
    JPUT('}');
}

/* ======================================================================
 * Parse-trace recording (fired once per trace row, pre-action state,
 * via the additive parserRunRecordSteps hook)
 * ====================================================================== */

typedef struct {
    FILE *out;
    int   seen;
} StepCtx;

static void recordStep(void *vctx, const ParserState *ps,
                       const unsigned char *isMarker,
                       const char *actionText) {
    StepCtx *ctx = (StepCtx *)vctx;
    FILE *out = ctx->out;

    if (ctx->seen) JSEP();
    ctx->seen = 1;

    fputc('{', out);
    /* traceStep() already incremented stepNumber for this row. */
    jsonStr(out, "step"); fprintf(out, ":%d", ps->stepNumber);
    JSEP();
    jsonStr(out, "stack"); fputc(':', out); fputc('[', out);
    for (int i = ps->stackTop; i >= 0; i--) { /* top first */
        if (isMarker[i]) continue;
        if (i < ps->stackTop) JSEP();
        jsonStr(out, ps->stack[i]);
    }
    JPUT(']');
    JSEP();
    jsonStr(out, "input"); fputc(':', out); fputc('[', out);
    for (int i = ps->inputPos; i < ps->inputCount; i++) {
        if (i > ps->inputPos) JSEP();
        jsonStr(out, ps->input[i]);
    }
    JPUT(']');
    JSEP();
    const char *kind = "expand";
    if (strcmp(actionText, "accept") == 0) {
        kind = "accept";
    } else if (strncmp(actionText, "match ", 6) == 0) {
        kind = "match";
    }
    jsonStr(out, "action"); fputc(':', out); jsonStr(out, actionText);
    JSEP();
    jsonStr(out, "kind"); fputc(':', out); jsonStr(out, kind);
    JPUT('}');
}

static void jsonFirstSentence(FILE *out, const char *msg) {
    char buf[128];
    size_t i = 0;
    for (; msg[i] != '\0' && msg[i] != '\n' && i + 1 < sizeof(buf); i++) {
        buf[i] = msg[i];
    }
    buf[i] = '\0';
    jsonStr(out, buf);
}

/* Failing-step configuration: marker slots hold "" as stack text, so
 * filtering empty strings reproduces the pre-error stack (symbols are
 * never empty). */
static void jsonPlainStackTopFirst(FILE *out, const ParserState *ps) {
    fputc('[', out);
    int first = 1;
    for (int i = ps->stackTop; i >= 0; i--) {
        if (ps->stack[i][0] == '\0') {
            continue; /* REDUCE marker slot */
        }
        if (!first) JSEP();
        first = 0;
        jsonStr(out, ps->stack[i]);
    }
    JPUT(']');
}

static void jsonRemainingInput(FILE *out, const ParserState *ps) {
    fputc('[', out);
    for (int i = ps->inputPos; i < ps->inputCount; i++) {
        if (i > ps->inputPos) JSEP();
        jsonStr(out, ps->input[i]);
    }
    JPUT(']');
}

/* ======================================================================
 * Pipeline export
 * ====================================================================== */

int jsonExportPipeline(FILE *grammarStream, const char *input, FILE *out) {
    Grammar          grammar;
    FirstCollection  first;
    FollowCollection follow;
    ParsingTable     table;
    char             errorOut[512];

    initializeGrammar(&grammar);
    initializeFirstCollection(&first);
    initializeFollowCollection(&follow);
    initializeParsingTable(&table);

    fputc('{', out);
    jsonStr(out, "ok"); fputc(':', out);

    if (!readGrammarFromStream(&grammar, grammarStream,
                               errorOut, sizeof(errorOut))) {
        fprintf(out, "false,");
        jsonStr(out, "stage"); fputc(':', out); jsonStr(out, "grammar");
        JSEP();
        jsonStr(out, "message"); fputc(':', out); jsonStr(out, errorOut);
        JPUT('}');
        return 1;
    }

    fprintf(out, "true,");
    jsonStr(out, "stage"); fputc(':', out); jsonStr(out, "ok");
    JSEP();

    /* ------------------------------------------------ grammar ---------- */
    jsonStr(out, "grammar"); fputc(':', out);
    fputc('{', out);
    jsonStr(out, "startSymbol"); fputc(':', out);
    jsonStr(out, grammar.startSymbol);
    JSEP();
    jsonStr(out, "nonTerminals"); fputc(':', out); fputc('[', out);
    for (int i = 0; i < grammar.nonTerminalCount; i++) {
        if (i > 0) JSEP();
        jsonStr(out, grammar.nonTerminals[i]);
    }
    JPUT(']');
    JSEP();
    jsonStr(out, "terminals"); fputc(':', out); fputc('[', out);
    for (int i = 0; i < grammar.terminalCount; i++) {
        if (i > 0) JSEP();
        jsonStr(out, grammar.terminals[i]);
    }
    JPUT(']');
    JSEP();
    jsonStr(out, "productionGroups"); fputc(':', out); fputc('[', out);
    for (int i = 0; i < grammar.productionCount; i++) {
        if (i > 0) JSEP();
        jsonProductionGroup(out, &grammar.productions[i]);
    }
    JPUT(']');
    JPUT('}');
    JSEP();

    /* ------------------------------------------------ FIRST ------------ */
    if (!computeFirstSets(&grammar, &first)) {
        fprintf(out, "false,");
        jsonStr(out, "stage"); fputc(':', out); jsonStr(out, "first");
        JSEP();
        jsonStr(out, "message"); JPUT(':');
        jsonStr(out, "FIRST computation failed");
        JPUT('}');
        return 1;
    }
    jsonStr(out, "first"); fputc(':', out);
    fputc('{', out);
    for (int i = 0; i < first.setCount; i++) {
        const FirstSet *fs = &first.sets[i];
        if (i > 0) JSEP();
        jsonStr(out, fs->symbolName); fputc(':', out);
        fputc('{', out);
        jsonStr(out, "terminals"); fputc(':', out); fputc('[', out);
        int emitted = 0;
        for (int e = 0; e < fs->count; e++) {
            if (strcmp(fs->elements[e], "#") == 0) continue;
            if (emitted++ > 0) JSEP();
            jsonStr(out, fs->elements[e]);
        }
        JPUT(']');
        JSEP();
        jsonStr(out, "epsilon"); fprintf(out, ":%d", fs->hasEpsilon ? 1 : 0);
        JPUT('}');
    }
    JPUT('}');
    JSEP();

    /* ------------------------------------------------ FOLLOW ----------- */
    if (!computeFollowSets(&grammar, &first, &follow)) {
        fprintf(out, "false,");
        jsonStr(out, "stage"); fputc(':', out); jsonStr(out, "follow");
        JSEP();
        jsonStr(out, "message"); JPUT(':');
        jsonStr(out, "FOLLOW computation failed");
        JPUT('}');
        return 1;
    }
    jsonStr(out, "follow"); fputc(':', out);
    fputc('{', out);
    for (int i = 0; i < follow.setCount; i++) {
        const FollowSet *fset = &follow.sets[i];
        if (i > 0) JSEP();
        jsonStr(out, fset->symbolName); fputc(':', out);
        fputc('{', out);
        jsonStr(out, "terminals"); fputc(':', out); fputc('[', out);
        for (int e = 0; e < fset->count; e++) {
            if (e > 0) JSEP();
            jsonStr(out, fset->elements[e]); /* includes "$" */
        }
        JPUT(']');
        JPUT('}');
    }
    JPUT('}');
    JSEP();

    /* ------------------------------------------------ table ------------ */
    int isLL1 = constructParsingTable(&grammar, &first, &follow, &table);

    jsonStr(out, "isLL1"); fprintf(out, ":%d", isLL1 ? 1 : 0);
    JSEP();
    jsonStr(out, "table"); fputc(':', out);
    fputc('{', out);
    jsonStr(out, "nonTerminals"); fputc(':', out); fputc('[', out);
    for (int r = 0; r < table.nonTerminalCount; r++) {
        if (r > 0) JSEP();
        jsonStr(out, grammar.nonTerminals[r]);
    }
    JPUT(']');
    JSEP();
    jsonStr(out, "columns"); fputc(':', out); fputc('[', out);
    for (int c = 0; c < table.terminalCount; c++) {
        if (c > 0) JSEP();
        jsonStr(out, grammar.terminals[c]);
    }
    if (table.terminalCount > 0) JSEP();
    jsonStr(out, "$");
    JPUT(']');
    JSEP();
    jsonStr(out, "entries"); fputc(':', out); fputc('[', out);
    int entrySeen = 0;
    for (int r = 0; r < table.nonTerminalCount; r++) {
        int ntIndex = grammarNonTerminalIndex(&grammar,
                                              grammar.nonTerminals[r]);
        for (int c = 0; c <= table.terminalCount; c++) {
            if (!tableHasEntry(&table, ntIndex, c)) continue;
            if (entrySeen++ > 0) JSEP();
            const TableCell *cell = &table.cells[ntIndex][c];
            fputc('{', out);
            jsonStr(out, "nonTerminal"); fputc(':', out);
            jsonStr(out, grammar.nonTerminals[r]);
            JSEP();
            jsonStr(out, "column"); fputc(':', out);
            jsonStr(out, (c < table.terminalCount)
                             ? grammar.terminals[c] : "$");
            JSEP();
            jsonStr(out, "lhs"); fputc(':', out);
            jsonStr(out, grammar.productions[cell->lhsIndex].lhs);
            JSEP();
            jsonStr(out, "alt"); fprintf(out, ":%d", cell->altIndex);
            JPUT('}');
        }
    }
    JPUT(']');
    JPUT('}');
    JSEP();

    /* ------------------------------------------------ conflicts -------- */
    jsonStr(out, "conflicts"); fputc(':', out); fputc('[', out);
    for (int i = 0; i < table.conflictCount; i++) {
        const TableConflict *cf = &table.conflicts[i];
        if (i > 0) JSEP();
        fputc('{', out);
        jsonStr(out, "nonTerminal"); fputc(':', out);
        jsonStr(out, grammar.nonTerminals[cf->lhsIndex]);
        JSEP();
        jsonStr(out, "terminal"); fputc(':', out);
        jsonStr(out, cf->terminal);
        JSEP();
        jsonStr(out, "productions"); fputc(':', out); fputc('[', out);
        jsonStr(out, grammar.productions[cf->lhsIndex].lhs);
        /* altIndex2 = production already in the cell; altIndex = newcomer */
        {
            char text[MAX_RHS_LENGTH];
            productionAltToString(&grammar.productions[cf->lhsIndex],
                                  cf->altIndex2, text, sizeof(text));
            JSEP(); jsonStr(out, text);
            productionAltToString(&grammar.productions[cf->lhsIndex],
                                  cf->altIndex, text, sizeof(text));
            JSEP(); jsonStr(out, text);
        }
        JPUT(']');
        JSEP();
        /* Conflict TYPE derived from engine data with the standard rule:
         * FIRST/FOLLOW iff one competing alternative is nullable. */
        int nullable = productionAltIsEpsilon(
            grammar.productions[cf->lhsIndex].rhs[cf->altIndex]);
        if (!nullable) {
            nullable = productionAltIsEpsilon(
                grammar.productions[cf->lhsIndex].rhs[cf->altIndex2]);
        }
        if (!nullable) {
            FirstSet tmp;
            firstOfSequence(&grammar, &first,
                            grammar.productions[cf->lhsIndex]
                                .rhs[cf->altIndex],
                            &tmp);
            nullable = tmp.hasEpsilon;
        }
        jsonStr(out, "type"); fputc(':', out);
        jsonStr(out, nullable ? "FIRST/FOLLOW" : "FIRST/FIRST");
        JPUT('}');
    }
    JPUT(']');
    JSEP();

    /* ------------------------------------------------ parse ------------ */
    ParserState ps;
    initializeParserState(&ps);
    char inputBuf[MAX_LINE_LENGTH];
    snprintf(inputBuf, sizeof(inputBuf), "%s", input ? input : "");

    jsonStr(out, "parse"); fputc(':', out);
    fputc('{', out);
    jsonStr(out, "input"); fputc(':', out);
    jsonStr(out, inputBuf);

    int accepted = 0;
    int tokenized = parserTokenizeInput(inputBuf, &ps);
    if (!tokenized) {
        JSEP();
        jsonStr(out, "tokens"); fputc(':', out); fputc('[', out); JPUT(']');
        JSEP();
        jsonStr(out, "steps"); fputc(':', out); fputc('[', out); JPUT(']');
        JSEP();
        jsonStr(out, "status"); fputc(':', out); jsonStr(out, "rejected");
        JSEP();
        jsonStr(out, "error"); fputc(':', out);
        fputc('{', out);
        jsonStr(out, "message"); fputc(':', out); jsonStr(out, ps.lastError);
        JSEP();
        jsonStr(out, "step"); fprintf(out, ":0");
        JPUT('}');
        JSEP();
        goto translationSection;
    }

    JSEP();
    jsonStr(out, "tokens"); fputc(':', out); fputc('[', out);
    for (int i = 0; i < ps.inputCount; i++) {
        if (i > 0) JSEP();
        jsonStr(out, ps.input[i]); /* includes trailing "$" */
    }
    JPUT(']');

    JSEP();
    jsonStr(out, "steps"); fputc(':', out); fputc('[', out);

    static char trace[16384];
    StepCtx stepCtx;
    stepCtx.out = out;
    stepCtx.seen = 0;

    if (isLL1) {
        accepted = parserRunRecordSteps(&grammar, &table, &ps,
                                        NULL, NULL, recordStep, &stepCtx,
                                        trace, sizeof(trace));
    }
    JPUT(']'); /* steps */

    JSEP();
    jsonStr(out, "status"); fputc(':', out);
    if (!isLL1) {
        /* Same behavior as main.c: the engine never parses a
         * non-LL(1) grammar because the table cannot decide. */
        jsonStr(out, "skipped");
    } else {
        jsonStr(out, accepted ? "accepted" : "rejected");
    }

    if (!isLL1) {
        JSEP();
        jsonStr(out, "skipReason"); fputc(':', out);
        jsonStr(out, "the grammar is not LL(1): the engine refuses to "
                     "parse it because the table cannot decide "
                     "productions deterministically");
    } else if (!accepted) {
        JSEP();
        jsonStr(out, "error"); fputc(':', out);
        fputc('{', out);
        jsonStr(out, "message"); fputc(':', out);
        jsonStr(out, ps.lastError);
        JSEP();
        jsonStr(out, "action"); fputc(':', out);
        jsonFirstSentence(out, ps.lastError);
        JSEP();
        jsonStr(out, "step"); fprintf(out, ":%d", ps.stepNumber + 1);
        JSEP();
        jsonStr(out, "stack"); fputc(':', out);
        jsonPlainStackTopFirst(out, &ps);
        JSEP();
        jsonStr(out, "input"); fputc(':', out);
        jsonRemainingInput(out, &ps);
        JPUT('}');
    }
    JPUT('}'); /* parse */
    JSEP();

translationSection:
    /* ------------------------------------------------ translation ------ */
    jsonStr(out, "translation"); fputc(':', out);
    fputc('{', out);
    jsonStr(out, "available"); fputc(':', out);
    if (accepted) {
        TranslationResult tr;
        if (translateInput(&grammar, &table, inputBuf, &tr)) {
            fprintf(out, "1");
            JSEP();
            jsonStr(out, "postfix"); fputc(':', out); fputc('[', out);
            char work[MAX_POSTFIX_LENGTH];
            snprintf(work, sizeof(work), "%s", tr.postfix);
            int emitted = 0;
            for (char *tok = strtok(work, " "); tok != NULL;
                 tok = strtok(NULL, " ")) {
                if (emitted++ > 0) JSEP();
                jsonStr(out, tok);
            }
            JPUT(']');
            JSEP();
            jsonStr(out, "tac"); fputc(':', out); fputc('[', out);
            for (int i = 0; i < tr.lineCount; i++) {
                if (i > 0) JSEP();
                /* Strict shape "tN = L op R" (translation.c); anything
                 * else falls back to its raw text field. */
                fputc('{', out);
                {
                    char target[16], left[32], op[16], right[32];
                    if (sscanf(tr.lines[i], "%15s = %31s %15s %31s",
                               target, left, op, right) == 4) {
                        jsonStr(out, "target"); fputc(':', out);
                        jsonStr(out, target);
                        JSEP();
                        jsonStr(out, "left"); fputc(':', out);
                        jsonStr(out, left);
                        JSEP();
                        jsonStr(out, "op"); fputc(':', out);
                        jsonStr(out, op);
                        JSEP();
                        jsonStr(out, "right"); fputc(':', out);
                        jsonStr(out, right);
                    } else {
                        jsonStr(out, "text"); fputc(':', out);
                        jsonStr(out, tr.lines[i]);
                    }
                }
                JPUT('}');
            }
            JPUT(']');
            JSEP();
            jsonStr(out, "message"); JPUT(':');
            jsonStr(out, (tr.lineCount == 0)
                             ? "no three-address instructions needed"
                             : "");
        } else {
            fprintf(out, "0");
            JSEP();
            jsonStr(out, "message"); JPUT(':');
            jsonStr(out, tr.lastError);
        }
    } else {
        fprintf(out, "0");
        JSEP();
        jsonStr(out, "message"); JPUT(':');
        jsonStr(out, isLL1
                         ? "input was not accepted, so there is no translation"
                         : "the grammar is not LL(1), so the engine does not "
                           "parse or translate it");
    }
    JPUT('}'); /* translation */

    JPUT('}');
    return 0;
}
