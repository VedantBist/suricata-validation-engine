#ifndef LEXER_LEXER_H
#define LEXER_LEXER_H

#include <stdio.h>

#include "models/context.h"
#include "models/token.h"

/* Public lexer interface. Consumers (token dump, tests, later the parser
 * driver in core) never touch flex internals (yyin/yylval/yylloc) directly —
 * this boundary is the only supported way in.
 *
 * The lexer streams: flex reads the input in fixed-size buffered blocks; the
 * whole file is never loaded into memory. One token is live at a time. */

/* Binds the lexer to an input stream and a ParserContext. The context is the
 * single access seam between lexer and engine state (see ARCHITECTURE.md §3);
 * the lexer is the only writer of its position fields. */
void lexer_begin(FILE *input, ParserContext *ctx);

/* Releases flex buffers and unbinds the context. */
void lexer_end(void);

/* Scans the next token. Fills `out` (kind, owned lexeme or NULL, source
 * span) and returns the token kind; returns 0 (YYEOF) at end of input.
 * Ownership of out->lexeme transfers to the caller. */
int lexer_next(Token *out);

#endif /* LEXER_LEXER_H */
