/*
 * tools/json_main.c - Entry point for the additive `ll1json` binary
 * (machine-readable mode used by the ParseLab web adapter).
 *
 *   Usage:  ll1json <grammar_file> <input>
 *           ll1json - <input>          (grammar text on stdin)
 *
 * ALL parsing/computation happens in the unchanged engine modules;
 * this file only wires them to src/json_export.c. The default ll1
 * binary and menu behavior are untouched.
 */

#include <stdio.h>
#include <string.h>

#include "json_export.h"

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr,
                "usage: ll1json <grammar_file> <input>   (or: ll1json - <input>)\n");
        fprintf(stderr,
                "{\"ok\":false,\"stage\":\"usage\",\"message\":"
                "\"usage: ll1json <grammar_file> <input>\"}\n");
        return 2;
    }

    /* Input string is argv[2] when given, else the empty (epsilon) input. */
    const char *input = (argc >= 3) ? argv[2] : "";

    /* Guard against absurd CLI sizes before touching the engine. */
    if (strlen(input) >= 4096) {
        fprintf(stdout,
                "{\"ok\":false,\"stage\":\"input\","
                "\"message\":\"input too long (max 4095 characters)\"}\n");
        return 1;
    }

    int rc;
    if (strcmp(argv[1], "-") == 0) {
        rc = jsonExportPipeline(stdin, input, stdout);
    } else {
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            fprintf(stdout,
                    "{\"ok\":false,\"stage\":\"grammar\","
                    "\"message\":\"cannot open grammar file\"}\n");
            return 1;
        }
        rc = jsonExportPipeline(fp, input, stdout);
        fclose(fp);
    }

    fputc('\n', stdout);
    return rc;
}
