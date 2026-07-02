#ifndef CORE_VALIDATE_H
#define CORE_VALIDATE_H

#include <stdio.h>

/* Validation mode: parse the whole input with error recovery, stream
 * per-rule verdicts, then render collected diagnostics and a summary.
 *
 * Returns the process exit code: 0 all rules valid, 1 violations found,
 * 2 engine failure (parser abort — cannot happen through malformed input,
 * only through internal failure such as memory exhaustion). */
int validate_run(FILE *input, FILE *out);

#endif /* CORE_VALIDATE_H */
