#ifndef REPORTING_REPORT_JSON_H
#define REPORTING_REPORT_JSON_H

#include <stdio.h>

#include "models/context.h"

/* Machine-readable renderer. Same inputs as the text renderer (the
 * diagnostics list and run counters — never parser state), fixed field
 * order, insertion-order diagnostics, no timestamps: byte-identical output
 * for identical input, safe to golden-diff and to pipe into CI.
 *
 * Schema (schema_version 1): optional diagnostic fields (column, expected,
 * found, field, value, explanation) are present when applicable and
 * omitted otherwise; consumers must tolerate omission. Memory profile is
 * bounded by diagnostic count, never rule count — rules were freed as they
 * streamed by. */
void report_json_run(FILE *out, const ParserContext *ctx, const char *mode);

#endif /* REPORTING_REPORT_JSON_H */
