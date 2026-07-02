#include "core/token_dump.h"

#include <stdlib.h>

#include "lexer/lexer.h"
#include "lexer/token_names.h"
#include "models/context.h"
#include "reporting/report_text.h"

int token_dump_run(FILE *input, FILE *out)
{
    ParserContext *ctx = context_create();
    lexer_begin(input, ctx);

    long tokens = 0;
    Token tok;
    while (lexer_next(&tok) != 0) {
        tokens++;
        const char *name = token_kind_name(tok.kind);
        const char *text = tok.lexeme != NULL ? tok.lexeme
                                              : token_static_text(tok.kind);
        fprintf(out, "%5d:%-4d r%-5d ",
                tok.span.first_line, tok.span.first_column,
                tok.span.rule_number);
        if (text == NULL) {
            /* No payload (EOL): bare name, no trailing padding — output is
             * diffed verbatim by golden tests. */
            fprintf(out, "%s\n", name);
        } else if (tok.lexeme != NULL) {
            fprintf(out, "%-14s \"%s\"\n", name, text);
            free(tok.lexeme);
        } else {
            fprintf(out, "%-14s %s\n", name, text);
        }
    }

    lexer_end();

    size_t lexical_errors = ctx->diagnostics.count;
    fprintf(out, "== tokens: %ld | rules: %d | lines: %d | lexical errors: %zu ==\n",
            tokens, ctx->rule_count, ctx->lines_seen, lexical_errors);

    if (lexical_errors > 0) {
        fputc('\n', out);
        report_diagnostics(out, &ctx->diagnostics);
    }

    context_destroy(ctx);
    return lexical_errors > 0 ? 1 : 0;
}
