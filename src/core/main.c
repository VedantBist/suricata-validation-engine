#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/token_dump.h"
#include "core/validate.h"

#define SV_VERSION "1.0.0"

/* Exit codes are part of the tool contract (asserted by the test runner):
 *   0 — clean run, no violations
 *   1 — input violations found (syntax, semantic, or warnings)
 *   2 — engine/usage failure */

static void usage(FILE *out)
{
    fprintf(out,
        "suricata-validate %s — Suricata rule validation engine\n"
        "\n"
        "usage: suricata-validate [options] <rules-file>\n"
        "       use '-' as <rules-file> to read from stdin\n"
        "\n"
        "modes (mutually exclusive):\n"
        "  --validate          full validation: syntax + semantics (default)\n"
        "  --syntax-only       structural validation only, semantic layer off\n"
        "  --dump-tokens       print the token stream with positions\n"
        "\n"
        "output (mutually exclusive):\n"
        "  --json              machine-readable JSON report (schema v1)\n"
        "  --quiet             suppress per-rule verdicts; diagnostics + summary only\n"
        "  --summary           summary line only\n"
        "\n"
        "behavior:\n"
        "  --max-errors <n>    stop after n error diagnostics (rule-boundary\n"
        "                      granularity; cleanup guarantees preserved)\n"
        "  --timing            print timing/throughput to stderr\n"
        "\n"
        "misc:\n"
        "  -h, --help          this help\n"
        "  --version           print version\n"
        "\n"
        "environment:\n"
        "  SV_PARSER_TRACE=1   full parser shift/reduce/recovery trace\n"
        "                      (debug builds only)\n"
        "\n"
        "exit codes: 0 clean | 1 violations found | 2 engine/usage failure\n",
        SV_VERSION);
}

static int fail_usage(const char *msg, const char *arg)
{
    if (arg != NULL) {
        fprintf(stderr, "error: %s: '%s'\n\n", msg, arg);
    } else {
        fprintf(stderr, "error: %s\n\n", msg);
    }
    fprintf(stderr, "run 'suricata-validate --help' for usage\n");
    return 2;
}

int main(int argc, char **argv)
{
    ValidateOptions opts = {0, 0, 0, 0, 0, 0};
    const char *path = NULL;
    int dump_tokens = 0;
    int mode_flags = 0;
    int output_flags = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(arg, "--version") == 0) {
            printf("suricata-validate %s\n", SV_VERSION);
            return 0;
        } else if (strcmp(arg, "--validate") == 0) {
            mode_flags++;
        } else if (strcmp(arg, "--syntax-only") == 0) {
            opts.syntax_only = 1;
            mode_flags++;
        } else if (strcmp(arg, "--dump-tokens") == 0) {
            dump_tokens = 1;
            mode_flags++;
        } else if (strcmp(arg, "--json") == 0) {
            opts.json = 1;
            output_flags++;
        } else if (strcmp(arg, "--quiet") == 0) {
            opts.quiet = 1;
            output_flags++;
        } else if (strcmp(arg, "--summary") == 0) {
            opts.summary_only = 1;
            output_flags++;
        } else if (strcmp(arg, "--timing") == 0) {
            opts.timing = 1;
        } else if (strcmp(arg, "--max-errors") == 0) {
            if (i + 1 >= argc) {
                return fail_usage("--max-errors requires a value", NULL);
            }
            char *end = NULL;
            long cap = strtol(argv[++i], &end, 10);
            if (end == argv[i] || *end != '\0' || cap < 1) {
                return fail_usage("--max-errors wants a positive integer",
                                  argv[i]);
            }
            opts.max_errors = (int)cap;
        } else if (arg[0] == '-' && strcmp(arg, "-") != 0) {
            return fail_usage("unknown option", arg);
        } else {
            if (path != NULL) {
                return fail_usage("more than one input file given", arg);
            }
            path = arg;
        }
    }

    if (path == NULL) {
        return fail_usage("no input file given", NULL);
    }
    if (mode_flags > 1) {
        return fail_usage(
            "--validate, --syntax-only and --dump-tokens are mutually exclusive",
            NULL);
    }
    if (output_flags > 1) {
        return fail_usage(
            "--json, --quiet and --summary are mutually exclusive", NULL);
    }
    if (dump_tokens && (output_flags > 0 || opts.max_errors > 0)) {
        return fail_usage(
            "--dump-tokens does not combine with output/behavior options",
            NULL);
    }

    FILE *input = stdin;
    if (strcmp(path, "-") != 0) {
        input = fopen(path, "r");
        if (input == NULL) {
            fprintf(stderr, "error: cannot open '%s'\n", path);
            return 2;
        }
    }

    int rc = dump_tokens ? token_dump_run(input, stdout)
                         : validate_run(input, stdout, &opts);

    if (input != stdin) {
        fclose(input);
    }
    return rc;
}
