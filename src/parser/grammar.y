/* Parser stub — Phase 2.
 *
 * This file exists to OWN the token inventory: Bison generates the canonical
 * token enumeration (build/gen/parser.tab.h) that the lexer, token dump, and
 * all future modules include. No hand-written token enum exists anywhere —
 * one definition, one owner.
 *
 * The grammar body is a deliberate placeholder. Real productions, the
 * `error EOL` recovery backbone, and `%define parse.error custom` arrive in
 * Phase 3/4 per docs/ROADMAP.md. yyparse() is not called in Phase 2.
 */

%code requires {
    #include "models/source_location.h"
    typedef struct ParserContext ParserContext;
}

%code {
    #include <stdio.h>
    #include "models/context.h"

    int yylex(void);
    static void yyerror(ParserContext *ctx, const char *msg);
}

/* Token semantic value: the lexeme for value-bearing tokens (heap-owned,
 * allocated by the lexer), NULL for fixed-spelling tokens. */
%define api.value.type {char *}

%locations
%define api.location.type {SrcSpan}
%parse-param {ParserContext *ctx}

/* ---- Token inventory (Phase 2 scope) ---- */

/* Actions */
%token TOK_ALERT   "alert"
%token TOK_DROP    "drop"
%token TOK_PASS    "pass"

/* Protocols */
%token TOK_TCP     "tcp"
%token TOK_UDP     "udp"
%token TOK_ICMP    "icmp"

/* Keywords */
%token TOK_ANY     "any"

/* Operators */
%token TOK_ARROW   "->"
%token TOK_BIDIR   "<>"

/* Punctuation */
%token TOK_LPAREN    "("
%token TOK_RPAREN    ")"
%token TOK_COLON     ":"
%token TOK_SEMICOLON ";"
%token TOK_COMMA     ","

/* Value-bearing tokens */
%token TOK_IP          "IP address"
%token TOK_CIDR        "CIDR block"
%token TOK_PORT        "port"
%token TOK_NUMBER      "number"
%token TOK_VARIABLE    "variable"
%token TOK_STRING      "string"
%token TOK_OPTION_KEY  "option keyword"
%token TOK_IDENT       "identifier"

/* Structural */
%token TOK_EOL "end of line"

/* Lexical-error carrier: unclassifiable input reaches the parser as a real
 * token so Phase 4 diagnostics can report `Found: INVALID_TOKEN "@"` from
 * genuine parser state instead of losing the information in the lexer. */
%token TOK_INVALID "invalid token"

%%

/* Placeholder start symbol — replaced by the real rule grammar in Phase 3. */
file:
    %empty
    ;

%%

static void yyerror(ParserContext *ctx, const char *msg)
{
    /* Phase 3+: route into the diagnostics engine. Never printed from here. */
    (void)ctx;
    (void)msg;
}
