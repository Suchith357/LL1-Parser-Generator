#ifndef CONSTANTS_H
#define CONSTANTS_H

/*
 * constants.h - Shared compile-time limits for all modules.
 *
 * Every module includes this header (directly or via grammar.h) so that
 * buffer sizes agree everywhere. If one member needs a larger limit,
 * change it HERE only; never redefine sizes locally.
 */

/* ---- Symbol representation ------------------------------------------- */

/* A grammar symbol is a short identifier such as "E", "Ep", "Expr", "id".
 * +1 reserves room for the terminating '\0'. */
#define MAX_SYMBOL_LENGTH 32

/* ---- Grammar capacities ---------------------------------------------- */

#define MAX_PRODUCTIONS        50  /* maximum distinct LHS productions      */
#define MAX_ALTERNATIVES       20  /* max alternatives per LHS              */
#define MAX_RHS_SYMBOLS        20  /* max symbols in one RHS alternative    */
#define MAX_RHS_LENGTH        128  /* max characters of one RHS alternative */
#define MAX_LINE_LENGTH       256  /* max characters of one input line      */
#define MAX_SYMBOLS           50  /* max terminals or non-terminals        */

/* ---- FIRST / FOLLOW sets --------------------------------------------- */

#define MAX_SET_ELEMENTS       64  /* max terminals (+epsilon) in one set   */

/* ---- Parsing table --------------------------------------------------- */

#define MAX_TABLE_ROWS         MAX_SYMBOLS
#define MAX_TABLE_COLS         MAX_SYMBOLS
#define MAX_CONFLICTS          32  /* max reported LL(1) conflicts  */

/* ---- Predictive parser ----------------------------------------------- */

#define MAX_STACK_DEPTH      1024  /* parse stack depth                     */
#define MAX_INPUT_TOKENS      256  /* max tokens in one input string        */
#define MAX_TOKEN_LENGTH       32  /* must be <= MAX_SYMBOL_LENGTH          */
#define MAX_PARSE_STEPS     20000  /* safety cap on trace steps             */

/* "lhs -> rhs" action text: symbol + " -> " + RHS + slack. */
#define MAX_ACTION_LENGTH    (MAX_SYMBOL_LENGTH + MAX_RHS_LENGTH + 8)

/* ---- Syntax-directed translation ------------------------------------- */

#define MAX_TAC_TEMPS          64  /* max temporaries t1, t2, ...           */
#define MAX_TAC_LINES         128  /* max emitted three-address lines       */
#define MAX_TAC_LINE_LENGTH   128

/* Semantic values on the SDT stack (postfix fragments). Raised from the
 * draft value 64 during integration so postfix fragments of longer
 * inputs fit; every append is bounds-checked regardless. */
#define MAX_SEMANTIC_TEXT     128
#define MAX_SEMANTIC_STACK    256  /* max values on the semantic stack      */

/* Buffer for the complete postfix translation of one input. */
#define MAX_POSTFIX_LENGTH   1024

/* ---- Sentinel values -------------------------------------------------- */

/* End-of-input marker. It is never a grammar symbol and never a token,
 * so every module may use it as an unambiguous "no entry" value. */
#define END_MARKER             '$'

/* Index meaning "no symbol / not found / empty table cell". */
#define INVALID_INDEX         (-1)
#endif /* CONSTANTS_H */
