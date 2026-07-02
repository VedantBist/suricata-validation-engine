#ifndef LEXER_TOKEN_NAMES_H
#define LEXER_TOKEN_NAMES_H

/* Display names for token kinds, used by the token dump and (Phase 4) the
 * Found side of diagnostics. Kinds are the Bison enumerators from
 * build/gen/parser.tab.h — this table renders them; it never defines them. */

/* Stable display name: "ALERT", "ARROW", "OPTION_KEY", "EOL", "EOF", ... */
const char *token_kind_name(int kind);

/* Canonical spelling for fixed-spelling tokens ("alert", "->", "(" ...);
 * NULL for value-bearing kinds, whose text lives in the token's lexeme. */
const char *token_static_text(int kind);

#endif /* LEXER_TOKEN_NAMES_H */
