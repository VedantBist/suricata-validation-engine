#ifndef CORE_DISPATCH_H
#define CORE_DISPATCH_H

#include "models/context.h"
#include "models/rule.h"

/* The parser -> core handoff (the one deliberate inversion of the
 * core-drives-parser arrow): grammar actions call these as each line
 * completes, keeping the pipeline streaming — one rule is live at a time,
 * verdicts are emitted in input order, nothing accumulates in the parser.
 *
 * accepted: ownership of `rule` transfers here; core frees it after
 * validation (Phase 6 semantic checks will run inside this function).
 * rejected: the syntax diagnostic was already recorded by the parser's
 * error report; this only accounts for the failed rule. */

void dispatch_rule_accepted(ParserContext *ctx, Rule *rule);
void dispatch_rule_rejected(ParserContext *ctx, int rule_number, int line);

#endif /* CORE_DISPATCH_H */
