#include "core/validate.h"

#include <stdlib.h>
#include <time.h>

#include "parser.tab.h"

#include "core/dispatch.h"
#include "lexer/lexer.h"
#include "models/context.h"
#include "reporting/report_json.h"
#include "reporting/report_text.h"
#include "semantic/semantic.h"

/* Continue/stop decision for the fail-fast policy — evaluated after each
 * completed rule, so a rule is never abandoned halfway and the accounting
 * invariant (valid + invalid == rules seen) survives an early stop. */
static int under_error_cap(ParserContext *ctx)
{
    if (ctx->max_errors > 0
        && ctx->diagnostics.errors >= (size_t)ctx->max_errors) {
        ctx->stopped_early = 1;
        return 0;
    }
    return 1;
}

int dispatch_rule_accepted(ParserContext *ctx, Rule *rule)
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
    return under_error_cap(ctx);
}

int dispatch_rule_rejected(ParserContext *ctx, int rule_number, int line)
{
    ctx->invalid_count++;
    if (ctx->report_out != NULL) {
        report_rule_verdict(ctx->report_out, rule_number, line,
                            VERDICT_INVALID_SYNTAX);
    }
    return under_error_cap(ctx);
}

static double elapsed_ms(const struct timespec *from, const struct timespec *to)
{
    return (double)(to->tv_sec - from->tv_sec) * 1000.0
         + (double)(to->tv_nsec - from->tv_nsec) / 1.0e6;
}

int validate_run(FILE *input, FILE *out, const ValidateOptions *opts)
{
    struct timespec t_start;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    ParserContext *ctx = context_create();
    ctx->max_errors = opts->max_errors;
    if (!opts->syntax_only) {
        ctx->semantics = semantic_context_create();
    }
    /* Verdicts stream only in the default human mode. */
    if (!opts->json && !opts->quiet && !opts->summary_only) {
        ctx->report_out = out;
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

    struct timespec t_parsed;
    clock_gettime(CLOCK_MONOTONIC, &t_parsed);

    int exit_code;
    if (parse_rc != 0) {
        fprintf(stderr, "error: parser aborted (internal failure)\n");
        exit_code = 2;
    } else if (ctx->valid_count + ctx->invalid_count
                   + ctx->semantic_invalid_count != ctx->rule_count) {
        /* Engine self-check: every rule the lexer numbered must have
         * received exactly one verdict. Unreachable through malformed
         * input; if it fires, the engine itself is wrong. */
        fprintf(stderr,
                "error: internal invariant violation "
                "(verdicts %d+%d+%d != rules %d)\n",
                ctx->valid_count, ctx->invalid_count,
                ctx->semantic_invalid_count, ctx->rule_count);
        exit_code = 2;
    } else {
        if (opts->json) {
            report_json_run(out, ctx,
                            opts->syntax_only ? "syntax-only" : "validate");
        } else if (opts->summary_only) {
            report_summary(out, ctx);
        } else {
            if (ctx->diagnostics.count > 0) {
                fputc('\n', out);
                report_diagnostics(out, &ctx->diagnostics);
            }
            fputc('\n', out);
            report_summary(out, ctx);
        }
        exit_code = (ctx->invalid_count > 0 || ctx->semantic_invalid_count > 0
                     || ctx->diagnostics.count > 0)
                        ? 1
                        : 0;
    }

    if (opts->timing) {
        /* stderr, deliberately: timing is non-deterministic and must never
         * contaminate golden-diffable stdout. */
        double ms = elapsed_ms(&t_start, &t_parsed);
        double per_sec = ms > 0.0 ? (double)ctx->rule_count / (ms / 1000.0)
                                  : 0.0;
        fprintf(stderr,
                "-- timing: parse+validate %.2f ms | %d rules | %d lines"
                " | %.0f rules/s --\n",
                ms, ctx->rule_count, ctx->lines_seen, per_sec);
    }

    semantic_context_destroy(ctx->semantics);
    context_destroy(ctx);
    return exit_code;
}
