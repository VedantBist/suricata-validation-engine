#include <stdio.h>
#include <string.h>

#include "core/token_dump.h"
#include "core/validate.h"

/* Exit codes are part of the tool contract (asserted by the test runner):
 *   0 — clean run, no violations
 *   1 — input violations found
 *   2 — engine/usage failure */

static int usage(FILE *out)
{
    fprintf(out,
            "usage: suricata-validate [--validate|--dump-tokens] <rules-file>\n"
            "\n"
            "modes:\n"
            "  --validate      parse and validate rules with error recovery\n"
            "                  (default when no mode is given)\n"
            "  --dump-tokens   print the token stream with positions\n"
            "                  (lexer inspection)\n"
            "\n"
            "use '-' as <rules-file> to read from stdin\n");
    return 2;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    int dump_tokens = 0;

    if (argc == 2) {
        path = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--validate") == 0) {
        path = argv[2];
    } else if (argc == 3 && strcmp(argv[1], "--dump-tokens") == 0) {
        dump_tokens = 1;
        path = argv[2];
    } else {
        return usage(stderr);
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
                         : validate_run(input, stdout);

    if (input != stdin) {
        fclose(input);
    }
    return rc;
}
