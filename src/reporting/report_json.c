#include "reporting/report_json.h"

/* Emits a JSON string literal with escaping per RFC 8259: quote,
 * backslash, and control characters. Diagnostic text can contain quotes
 * (lexemes, msg values), so this is not optional. */
static void json_string(FILE *out, const char *s)
{
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\b': fputs("\\b", out);  break;
        case '\f': fputs("\\f", out);  break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (*p < 0x20) {
                fprintf(out, "\\u%04x", *p);
            } else {
                fputc(*p, out);
            }
        }
    }
    fputc('"', out);
}

static void field_str(FILE *out, const char *key, const char *value,
                      const char *sep)
{
    fprintf(out, "%s      ", sep);
    json_string(out, key);
    fputs(": ", out);
    json_string(out, value);
}

static void diagnostic_json(FILE *out, const Diagnostic *d)
{
    fputs("    {\n", out);
    fprintf(out, "      \"rule\": %d,\n", d->rule_number);
    fprintf(out, "      \"line\": %d", d->line);

    /* Column is positional information: meaningful for lexical/syntax,
     * omitted for value-level semantic diagnostics. */
    if (d->category != DIAG_SEMANTIC) {
        fprintf(out, ",\n      \"column\": %d", d->column);
    }
    field_str(out, "category", diag_category_name(d->category), ",\n");
    field_str(out, "severity", diag_severity_name(d->severity), ",\n");
    if (d->expected != NULL) {
        field_str(out, "expected", d->expected, ",\n");
    }
    if (d->found != NULL) {
        field_str(out, "found", d->found, ",\n");
    }
    if (d->field != NULL) {
        field_str(out, "field", d->field, ",\n");
    }
    if (d->value != NULL) {
        field_str(out, "value", d->value, ",\n");
    }
    field_str(out, "message", d->message, ",\n");
    if (d->explanation != NULL) {
        field_str(out, "explanation", d->explanation, ",\n");
    }
    fputs("\n    }", out);
}

void report_json_run(FILE *out, const ParserContext *ctx, const char *mode)
{
    const DiagList *diags = &ctx->diagnostics;

    fputs("{\n", out);
    fputs("  \"tool\": \"suricata-validate\",\n", out);
    fputs("  \"schema_version\": 1,\n", out);
    fprintf(out, "  \"mode\": \"%s\",\n", mode);
    fprintf(out, "  \"stopped_early\": %s,\n",
            ctx->stopped_early ? "true" : "false");

    fputs("  \"summary\": {\n", out);
    fprintf(out, "    \"total_rules\": %d,\n", ctx->rule_count);
    fprintf(out, "    \"valid_rules\": %d,\n", ctx->valid_count);
    fprintf(out, "    \"syntax_invalid\": %d,\n", ctx->invalid_count);
    fprintf(out, "    \"semantic_invalid\": %d,\n", ctx->semantic_invalid_count);
    fprintf(out, "    \"errors\": %zu,\n", diags->errors);
    fprintf(out, "    \"warnings\": %zu,\n", diags->warnings);
    fprintf(out, "    \"lexical_errors\": %zu,\n", diags->lexical);
    fprintf(out, "    \"lines\": %d\n", ctx->lines_seen);
    fputs("  },\n", out);

    fputs("  \"diagnostics\": [", out);
    for (size_t i = 0; i < diags->count; i++) {
        fputs(i == 0 ? "\n" : ",\n", out);
        diagnostic_json(out, &diags->items[i]);
    }
    fputs(diags->count > 0 ? "\n  ]\n" : "]\n", out);
    fputs("}\n", out);
}
