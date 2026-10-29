#ifndef JSON_EXPORT_H
#define JSON_EXPORT_H

/*
 * json_export.h - Additive machine-readable export for the ParseLab
 * web adapter. Reuses every existing engine function unchanged
 * (grammar.c validation, first.c, follow.c, parsing_table.c,
 * parser.c semantics, translation.c SDT) and emits ONE JSON document
 * describing: grammar, FIRST, FOLLOW, LL(1) table, conflicts, the
 * complete predictive-parse trace (per-step stack / remaining input /
 * action / kind) and the syntax-directed translation.
 *
 * This module owns NO parsing logic of its own; it only formats
 * engine-produced data. It is compiled into the default ll1 binary as
 * plain object code (no separate main) so ll1's behavior is untouched;
 * tools/json_main.c provides the ll1json entry point.
 */

#include <stdio.h>

/*
 * Runs the full pipeline and writes the JSON document to `out`.
 *   grammar : already-open text stream containing the grammar
 *             (grammar.c readGrammarFromStream format)
 *   input   : whitespace-separated input string to parse and translate
 *             ("" parses the empty/epsilon input)
 * Returns 0 when the document was produced; the adapter reads the
 * "ok" / "stage" keys inside the document for deeper failures.
 */
int jsonExportPipeline(FILE *grammarStream, const char *input, FILE *out);

#endif /* JSON_EXPORT_H */
