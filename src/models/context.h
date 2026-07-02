#ifndef MODELS_CONTEXT_H
#define MODELS_CONTEXT_H

#include "diagnostics/diagnostics.h"

/* All mutable per-run state, owned by core and threaded through the engine.
 * The lexer is the ONLY writer of the position fields (single source of
 * truth for line/column/rule tracking); everything else reads.
 *
 * Scan-state fields:
 *   line, column       — position of the next unconsumed character (1-based)
 *   rule_count         — rules seen so far; equals the current rule number
 *                        once a line has produced a token
 *   line_has_tokens    — whether the current physical line produced any
 *                        token yet (drives rule numbering and EOL emission)
 *   paren_depth        — option-parenthesis depth; selects PORT (header
 *                        context, depth 0) vs NUMBER (options context) for
 *                        bare integers; reset at every EOL so an unbalanced
 *                        ')' can never leak state into the next rule
 *   lines_seen         — physical lines consumed (run statistics) */
typedef struct ParserContext {
    int line;
    int column;
    int rule_count;
    int line_has_tokens;
    int paren_depth;
    int lines_seen;
    DiagList diagnostics;
} ParserContext;

ParserContext *context_create(void);
void context_destroy(ParserContext *ctx);

#endif /* MODELS_CONTEXT_H */
