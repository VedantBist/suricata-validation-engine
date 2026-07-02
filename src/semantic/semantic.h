#ifndef SEMANTIC_SEMANTIC_H
#define SEMANTIC_SEMANTIC_H

#include "diagnostics/diagnostics.h"
#include "models/rule.h"

/* Semantic validation layer (ARCHITECTURE.md §4 stage contract):
 *
 *   - runs ONLY on rules that parsed cleanly, immediately after dispatch —
 *     the parser constructs structure, this layer evaluates meaning,
 *     diagnostics narrates failures
 *   - completely independent of grammar/lexer state: input is the Rule
 *     object and nothing else
 *   - multiple semantic diagnostics per rule are allowed (unlike syntax) —
 *     the structure is trustworthy, so every bad value is reported at once
 *   - the SemanticContext outlives individual rules but holds only the sid
 *     registry (cross-rule provenance), never rules themselves
 *
 * Validation runs as an ordered table of independent passes (see
 * validators/validators.h); adding a check means adding a pass or extending
 * one file — never touching the parser. */

typedef struct SemanticContext SemanticContext;

SemanticContext *semantic_context_create(void);
void semantic_context_destroy(SemanticContext *sctx);

/* Runs every validator pass over the rule, appending diagnostics.
 * Returns the number of ERROR-severity semantic diagnostics emitted for
 * this rule (0 = semantically valid; warnings do not invalidate). */
int semantic_validate(SemanticContext *sctx, const Rule *rule, DiagList *diags);

#endif /* SEMANTIC_SEMANTIC_H */
