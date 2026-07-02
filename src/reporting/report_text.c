#include "reporting/report_text.h"

void report_rule_verdict(FILE *out, int rule_number, int line, int valid)
{
    fprintf(out, "Rule #%d (line %d): %s\n", rule_number, line,
            valid ? "VALID" : "INVALID (syntax)");
}

void report_diagnostics(FILE *out, const DiagList *list)
{
    for (size_t i = 0; i < list->count; i++) {
        const Diagnostic *d = &list->items[i];

        fprintf(out, "Rule #%d | Line %d, Col %d | %s %s\n",
                d->rule_number, d->line, d->column,
                diag_category_name(d->category),
                diag_severity_name(d->severity));

        if (d->expected != NULL) {
            fprintf(out, "  Expected: %s\n", d->expected);
        }
        if (d->found != NULL) {
            fprintf(out, "  Found:    %s\n", d->found);
        }
        fprintf(out, "  %s\n", d->message);
        if (d->explanation != NULL) {
            fprintf(out, "  %s\n", d->explanation);
        }
        if (i + 1 < list->count) {
            fputc('\n', out);
        }
    }
}
