#ifndef REPORTING_REPORT_TEXT_H
#define REPORTING_REPORT_TEXT_H

#include <stdio.h>

#include "diagnostics/diagnostics.h"

/* Human-readable renderer. Consumes collected diagnostics; creates and
 * interprets nothing. Output is deterministic: insertion order, no
 * timestamps, no absolute paths (golden tests diff it verbatim). */
void report_diagnostics(FILE *out, const DiagList *list);

/* Streamed per-rule verdict line, emitted as each rule completes. */
void report_rule_verdict(FILE *out, int rule_number, int line, int valid);

#endif /* REPORTING_REPORT_TEXT_H */
