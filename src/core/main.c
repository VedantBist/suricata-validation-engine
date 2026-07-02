#include <stdio.h>
#include <string.h>

#include "core/token_dump.h"

/* Exit codes are part of the tool contract (asserted by the test runner):
 *   0 — clean run, no violations
 *   1 — input violations found
 *   2 — engine/usage failure */

static int usage(FILE *out)
{
    fprintf(out,
            "usage: suricata-validate --dump-tokens <rules-file>\n"
            "\n"
            "modes:\n"
            "  --dump-tokens   print the token stream with positions (lexer\n"
            "                  inspection; validation mode arrives in Phase 3)\n"
            "\n"
            "use '-' as <rules-file> to read from stdin\n");
    return 2;
}

int main(int argc, char **argv)
{
    if (argc != 3 || strcmp(argv[1], "--dump-tokens") != 0) {
        return usage(stderr);
    }

    const char *path = argv[2];
    FILE *input = stdin;
    if (strcmp(path, "-") != 0) {
        input = fopen(path, "r");
        if (input == NULL) {
            fprintf(stderr, "error: cannot open '%s'\n", path);
            return 2;
        }
    }

    int rc = token_dump_run(input, stdout);

    if (input != stdin) {
        fclose(input);
    }
    return rc;
}
