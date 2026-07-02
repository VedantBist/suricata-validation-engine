#ifndef MODELS_CONTEXT_H
#define MODELS_CONTEXT_H

#include <stdio.h>

#include "diagnostics/diagnostics.h"

/* Progress cursor: the furthest reduction the current rule completed.
 * Updated by grammar actions, reset at every EOL. Diagnostics combines this
 * with the table-derived expected-token class to name positional roles
 * ("SrcIP" vs "DstIP" — identical token sets, different positions). This is
 * parser reduction state, not input-pattern guessing.
 *
 * Note: with lookahead correction (parse.lac full) a unit reduction whose
 * follow set excludes the offending token never executes, so at error time
 * the cursor holds the last reduction with a viable lookahead — mappings in
 * diagnostics use <=/>= thresholds, never equality on a single stage. */
typedef enum RuleProgress {
    PROGRESS_LINE_START = 0,
    PROGRESS_ACTION,
    PROGRESS_PROTOCOL,
    PROGRESS_SRC_IP,
    PROGRESS_SRC_PORT,
    PROGRESS_DIRECTION,
    PROGRESS_DST_IP,
    PROGRESS_DST_PORT
} RuleProgress;

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

    /* Parser state (Phase 3) */
    int progress;               /* RuleProgress cursor for diagnostics */
    char found_text[160];       /* raw text of the last token the lexer
                                   returned == the offending lookahead when
                                   a syntax error is reported */

    /* Run results (written by core's dispatch layer) */
    int valid_count;
    int invalid_count;
    FILE *report_out;           /* verdict stream; NULL outside validate mode */
} ParserContext;

ParserContext *context_create(void);
void context_destroy(ParserContext *ctx);

#endif /* MODELS_CONTEXT_H */
