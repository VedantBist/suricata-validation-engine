#ifndef CORE_VALIDATE_H
#define CORE_VALIDATE_H

#include <stdio.h>

/* Validation mode: parse the whole input with error recovery, run the
 * semantic pass on each cleanly parsed rule, stream per-rule verdicts,
 * then render collected diagnostics and a summary.
 *
 * syntax_only skips the semantic layer entirely (structural lint —
 * also what keeps parser golden tests isolated from semantic churn).
 *
 * Returns the process exit code: 0 all rules valid, 1 violations found,
 * 2 engine failure (parser abort — cannot happen through malformed input,
 * only through internal failure such as memory exhaustion). */
int validate_run(FILE *input, FILE *out, int syntax_only);

#endif /* CORE_VALIDATE_H */
