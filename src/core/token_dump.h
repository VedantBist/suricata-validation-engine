#ifndef CORE_TOKEN_DUMP_H
#define CORE_TOKEN_DUMP_H

#include <stdio.h>

/* Developer inspection mode: streams the token sequence for an input file,
 * one token per line with position/rule metadata, followed by a summary
 * line and any lexical diagnostics. Used for lexer debugging, parser
 * bring-up, and golden-file verification.
 *
 * Returns the process exit code: 0 clean, 1 lexical errors found. */
int token_dump_run(FILE *input, FILE *out);

#endif /* CORE_TOKEN_DUMP_H */
