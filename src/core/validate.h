#ifndef CORE_VALIDATE_H
#define CORE_VALIDATE_H

#include <stdio.h>

/* Run configuration, assembled by the CLI layer. Output shapes are
 * mutually exclusive (main.c enforces): default streams verdicts +
 * diagnostics + summary; quiet drops the verdict stream; summary_only
 * prints just the summary line; json emits the machine-readable report. */
typedef struct ValidateOptions {
    int syntax_only;    /* skip the semantic layer (structural lint) */
    int json;           /* machine-readable output */
    int quiet;          /* no per-rule verdict stream */
    int summary_only;   /* summary line only */
    int timing;         /* timing block on stderr (never stdout — output
                           stays deterministic and golden-diffable) */
    int max_errors;     /* stop after N ERROR diagnostics; 0 = unlimited */
} ValidateOptions;

/* Validation mode: parse the whole input with error recovery, run the
 * semantic pass on each cleanly parsed rule, render per the options.
 *
 * Returns the process exit code: 0 all rules valid, 1 violations found,
 * 2 engine failure (parser abort or internal invariant violation —
 * neither reachable through malformed input). */
int validate_run(FILE *input, FILE *out, const ValidateOptions *opts);

#endif /* CORE_VALIDATE_H */
