#include "reporting/report_text.h"

void report_rule_verdict(FILE *out, int rule_number, int line,
                         RuleVerdict verdict)
{
    const char *text =
        verdict == VERDICT_VALID          ? "VALID"
        : verdict == VERDICT_INVALID_SYNTAX ? "INVALID (syntax)"
                                            : "INVALID (semantic)";
    fprintf(out, "Rule #%d (line %d): %s\n", rule_number, line, text);
}

void report_diagnostics(FILE *out, const DiagList *list)
{
    for (size_t i = 0; i < list->count; i++) {
        const Diagnostic *d = &list->items[i];

        if (d->category == DIAG_SEMANTIC) {
            /* Semantic diagnostics are value-level, not position-level:
             * rule and line identify the rule; a column would point at
             * nothing meaningful. */
            fprintf(out, "Rule #%d | Line %d | %s %s\n",
                    d->rule_number, d->line,
                    diag_category_name(d->category),
                    diag_severity_name(d->severity));
            if (d->field != NULL) {
                fprintf(out, "  Field:    %s\n", d->field);
            }
            if (d->value != NULL) {
                fprintf(out, "  Value:    %s\n", d->value);
            }
        } else {
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
