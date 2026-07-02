#ifndef MODELS_TOKEN_H
#define MODELS_TOKEN_H

#include "models/source_location.h"

/* One lexed token as seen by tooling (token dump, tests).
 *
 * `kind` is the Bison-generated token enumerator (build/gen/parser.tab.h) —
 * Bison owns the canonical token inventory; nothing else defines token ids.
 *
 * `lexeme` ownership: for value-bearing kinds (IP, CIDR, PORT, NUMBER,
 * VARIABLE, STRING, OPTION_KEY, IDENT, INVALID_TOKEN) it is heap-allocated
 * by the lexer and owned by the receiver of the token. For fixed-spelling
 * kinds (keywords, operators, punctuation, EOL) it is NULL — their canonical
 * text is available via token_static_text(). */
typedef struct Token {
    int kind;
    char *lexeme;
    SrcSpan span;
} Token;

#endif /* MODELS_TOKEN_H */
