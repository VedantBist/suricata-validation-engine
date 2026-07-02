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
 * validation (the semantic pass runs inside this function).
 * rejected: the syntax diagnostic was already recorded by the parser's
 * error report; this only accounts for the failed rule.
 *
 * Return value is the continue/stop protocol for the max-errors cap:
 * 1 = keep parsing, 0 = the error cap is reached — the grammar's line
 * actions then YYACCEPT, ending the parse cleanly at a rule boundary
 * (bison's accept path runs destructors for anything left on the stack,
 * so early stop inherits the same cleanup guarantees as recovery). */

int dispatch_rule_accepted(ParserContext *ctx, Rule *rule);
int dispatch_rule_rejected(ParserContext *ctx, int rule_number, int line);

#endif /* CORE_DISPATCH_H */
