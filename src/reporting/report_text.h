#ifndef REPORTING_REPORT_TEXT_H
#define REPORTING_REPORT_TEXT_H

#include <stdio.h>

#include "diagnostics/diagnostics.h"

/* Human-readable renderer. Consumes collected diagnostics; creates and
 * interprets nothing. Output is deterministic: insertion order, no
 * timestamps, no absolute paths (golden tests diff it verbatim). */
void report_diagnostics(FILE *out, const DiagList *list);

typedef enum RuleVerdict {
    VERDICT_VALID,
    VERDICT_INVALID_SYNTAX,
    VERDICT_INVALID_SEMANTIC
} RuleVerdict;

/* Streamed per-rule verdict line, emitted as each rule completes. */
void report_rule_verdict(FILE *out, int rule_number, int line,
                         RuleVerdict verdict);

#endif /* REPORTING_REPORT_TEXT_H */
