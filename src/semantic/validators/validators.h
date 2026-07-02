#ifndef SEMANTIC_VALIDATORS_VALIDATORS_H
#define SEMANTIC_VALIDATORS_VALIDATORS_H

#include "diagnostics/diagnostics.h"
#include "models/rule.h"

/* One entry point per validator family; each is an isolated, independently
 * testable pass over (context, rule) that appends structured diagnostics.
 * Registered in the pass table in semantic.c — extending validation never
 * touches the parser. */

typedef struct SemanticContext SemanticContext;

/* header/: IP octet ranges, CIDR prefixes, port ranges, protocol/direction
 * coherence. */
void sem_validate_header(SemanticContext *sctx, const Rule *rule, DiagList *diags);

/* options/: sid presence/uniqueness/numeric, cross-rule duplicate sid via
 * the registry, rev numeric, empty msg/content, unknown option keys. */
void sem_validate_options(SemanticContext *sctx, const Rule *rule, DiagList *diags);

/* rule/: cross-field consistency (e.g. ports on icmp rules). */
void sem_validate_rule(SemanticContext *sctx, const Rule *rule, DiagList *diags);

/* Shared by validators: strict decimal parse. Returns 1 and stores the
 * value when `text` is entirely a non-negative decimal number that fits in
 * long, else 0. */
int sem_parse_number(const char *text, long *out);

#endif /* SEMANTIC_VALIDATORS_VALIDATORS_H */
