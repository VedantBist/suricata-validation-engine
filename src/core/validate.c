#include "core/validate.h"

#include <stdlib.h>

#include "parser.tab.h"

#include "core/dispatch.h"
#include "lexer/lexer.h"
#include "models/context.h"
#include "reporting/report_text.h"
#include "semantic/semantic.h"

void dispatch_rule_accepted(ParserContext *ctx, Rule *rule)
{
    /* The semantic pass runs here — after a clean parse, before the rule
     * dies. The parser built structure; this is where meaning is judged.
     * Streaming contract unchanged: the rule is freed before the next line
     * finishes parsing, whatever the verdict. */
    RuleVerdict verdict = VERDICT_VALID;
    if (ctx->semantics != NULL
        && semantic_validate(ctx->semantics, rule, &ctx->diagnostics) > 0) {
        verdict = VERDICT_INVALID_SEMANTIC;
        ctx->semantic_invalid_count++;
    } else {
        ctx->valid_count++;
    }
    if (ctx->report_out != NULL) {
        report_rule_verdict(ctx->report_out, rule->span.rule_number,
                            rule->span.first_line, verdict);
    }
    rule_free(rule);
}

void dispatch_rule_rejected(ParserContext *ctx, int rule_number, int line)
{
    ctx->invalid_count++;
    if (ctx->report_out != NULL) {
        report_rule_verdict(ctx->report_out, rule_number, line,
                            VERDICT_INVALID_SYNTAX);
    }
}

int validate_run(FILE *input, FILE *out, int syntax_only)
{
    ParserContext *ctx = context_create();
    ctx->report_out = out;
    if (!syntax_only) {
        ctx->semantics = semantic_context_create();
    }
    lexer_begin(input, ctx);

#if defined(YYDEBUG) && YYDEBUG
    /* Recovery debugging: full shift/reduce/recovery trace on demand
     * (debug builds only; see Makefile). */
    if (getenv("SV_PARSER_TRACE") != NULL) {
        extern int yydebug;
        yydebug = 1;
    }
#endif

    int parse_rc = yyparse(ctx);
    lexer_end();

    int exit_code;
    if (parse_rc != 0) {
        fprintf(stderr, "error: parser aborted (internal failure)\n");
        exit_code = 2;
    } else {
        if (ctx->diagnostics.count > 0) {
            fputc('\n', out);
            report_diagnostics(out, &ctx->diagnostics);
        }
        fprintf(out,
                "\n== rules: %d | valid: %d | syntax-invalid: %d"
                " | semantic-invalid: %d | diagnostics: %zu ==\n",
                ctx->rule_count, ctx->valid_count, ctx->invalid_count,
                ctx->semantic_invalid_count, ctx->diagnostics.count);
        exit_code = (ctx->invalid_count > 0 || ctx->semantic_invalid_count > 0
                     || ctx->diagnostics.count > 0)
                        ? 1
                        : 0;
    }

    semantic_context_destroy(ctx->semantics);
    context_destroy(ctx);
    return exit_code;
}
