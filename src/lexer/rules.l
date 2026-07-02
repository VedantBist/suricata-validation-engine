/* Suricata rule lexer — Phase 2 token scope (see docs/GRAMMAR.md).
 *
 * Responsibilities (and nothing more): character -> token classification,
 * line/column/rule-number tracking, lexical diagnostics via the factories in
 * diagnostics/. The lexer classifies, it never judges: 999.999.999.999 is a
 * well-formed IP *token*; rejecting its value is semantic analysis's job.
 *
 * Position model: the bound ParserContext is the single source of truth for
 * line/column/rule counters; this file is their only writer. Every match
 * stamps yylloc via YY_USER_ACTION before its action runs.
 *
 * EOL contract (the parser's recovery anchor):
 *   - an explicit TOK_EOL is returned at the end of every line that produced
 *     at least one token; blank and comment-only lines emit nothing, so the
 *     token stream is exactly a sequence of `<rule tokens> EOL` groups
 *   - a synthetic TOK_EOL is returned at EOF if the last line lacked a
 *     newline, so the recovery anchor always exists
 *   - paren depth resets at every EOL: an unbalanced ')' cannot leak the
 *     PORT/NUMBER context decision into the next rule
 */

%{
#include <string.h>

#include "parser.tab.h"

#include "diagnostics/diagnostics.h"
#include "lexer/lexer.h"
#include "models/context.h"
#include "utils/xalloc.h"

/* The one documented lexer <-> engine seam: set by lexer_begin(). */
static ParserContext *L = NULL;

static void stamp(void);
#define YY_USER_ACTION stamp();

static int  tok_fixed(int kind);
static int  tok_value(int kind);
static void mark_rule(void);
static int  end_of_line(void);
%}

%option noyywrap nounput noinput
%option never-interactive
%option nodefault

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*
IP4     {DIGIT}+\.{DIGIT}+\.{DIGIT}+\.{DIGIT}+

%%

[ \t]+                  { /* whitespace: column already advanced by stamp() */ }
\r?\n                   { int emit = end_of_line(); if (emit) return TOK_EOL; }
"#"[^\r\n]*             { /* comment runs to end of line */ }

 /* Keywords must precede the generic identifier rule: flex breaks
  * equal-length ties by rule order ("alert" vs {ID}), while longest-match
  * keeps "alerts" an IDENT. */
"alert"                 { return tok_fixed(TOK_ALERT); }
"drop"                  { return tok_fixed(TOK_DROP); }
"pass"                  { return tok_fixed(TOK_PASS); }
"tcp"                   { return tok_fixed(TOK_TCP); }
"udp"                   { return tok_fixed(TOK_UDP); }
"icmp"                  { return tok_fixed(TOK_ICMP); }
"any"                   { return tok_fixed(TOK_ANY); }

"msg"                   { return tok_value(TOK_OPTION_KEY); }
"sid"                   { return tok_value(TOK_OPTION_KEY); }
"rev"                   { return tok_value(TOK_OPTION_KEY); }
"content"               { return tok_value(TOK_OPTION_KEY); }

"->"                    { return tok_fixed(TOK_ARROW); }
"<>"                    { return tok_fixed(TOK_BIDIR); }

"("                     { L->paren_depth++; return tok_fixed(TOK_LPAREN); }
")"                     { if (L->paren_depth > 0) { L->paren_depth--; }
                          return tok_fixed(TOK_RPAREN); }
":"                     { return tok_fixed(TOK_COLON); }
";"                     { return tok_fixed(TOK_SEMICOLON); }
","                     { return tok_fixed(TOK_COMMA); }

 /* CIDR before IP is documentation only — longest match already prefers it. */
{IP4}\/{DIGIT}+         { return tok_value(TOK_CIDR); }
{IP4}                   { return tok_value(TOK_IP); }

 /* Bare integers: PORT in header context (outside parens), NUMBER inside
  * the options parens. Lexically identical; the paren-depth counter is the
  * minimal context needed to keep the token vocabulary meaningful for
  * diagnostics ("Expected: Port" vs "Expected: Number"). */
{DIGIT}+                { return tok_value(L->paren_depth > 0 ? TOK_NUMBER
                                                              : TOK_PORT); }

\${ID}                  { return tok_value(TOK_VARIABLE); }

 /* Terminated string: lexeme stored without the surrounding quotes.
  * Escapes (\" etc.) pass through uninterpreted — value semantics are not
  * the lexer's business. */
\"([^"\\\r\n]|\\[^\r\n])*\"    { mark_rule();
                                 yylval = xstrndup(yytext + 1, (size_t)yyleng - 2);
                                 return TOK_STRING; }

 /* Unterminated string: closed automatically at end of line so one missing
  * quote cannot swallow the following rules. Still yields a STRING token so
  * downstream structure checking can continue; the LEXICAL diagnostic marks
  * the line as broken. */
\"([^"\\\r\n]|\\[^\r\n])*      { mark_rule();
                                 diag_lexical_unterminated_string(&L->diagnostics, yylloc);
                                 yylval = xstrndup(yytext + 1, (size_t)yyleng - 1);
                                 return TOK_STRING; }

{ID}                    { return tok_value(TOK_IDENT); }

 /* Anything unclassifiable becomes a real INVALID token (with diagnostic)
  * rather than being silently dropped: the future parser reports
  * `Found: INVALID_TOKEN "@"` from genuine state. One char at a time —
  * scanning always makes progress. */
.                       { mark_rule();
                          yylval = xstrndup(yytext, (size_t)yyleng);
                          diag_lexical_invalid_char(&L->diagnostics, yylloc, yylval);
                          return TOK_INVALID; }

<<EOF>>                 { yylloc.first_line = yylloc.last_line = L->line;
                          yylloc.first_column = yylloc.last_column = L->column;
                          yylloc.rule_number = L->rule_count;
                          if (L->line_has_tokens) {
                              /* Last line has no trailing newline: emit the
                               * synthetic EOL so the recovery anchor exists. */
                              L->lines_seen++;
                              L->line_has_tokens = 0;
                              yylval = NULL;
                              return TOK_EOL;
                          }
                          yyterminate(); }

%%

/* Stamps yylloc from the context and advances the column. Runs before every
 * action (YY_USER_ACTION), including whitespace/comments, which is exactly
 * what keeps the column honest without per-rule bookkeeping. */
static void stamp(void)
{
    yylloc.first_line = L->line;
    yylloc.first_column = L->column;
    yylloc.last_line = L->line;
    yylloc.last_column = L->column + (int)yyleng - 1;
    yylloc.rule_number = L->rule_count;
    L->column += (int)yyleng;
}

/* First token on a physical line claims the next rule number. Blank and
 * comment-only lines never get here, so they consume no rule numbers. */
static void mark_rule(void)
{
    if (!L->line_has_tokens) {
        L->line_has_tokens = 1;
        L->rule_count++;
    }
    yylloc.rule_number = L->rule_count;
}

static int tok_fixed(int kind)
{
    mark_rule();
    yylval = NULL;
    return kind;
}

static int tok_value(int kind)
{
    mark_rule();
    yylval = xstrndup(yytext, (size_t)yyleng);
    return kind;
}

/* Newline bookkeeping. Returns whether an EOL token must be emitted (only
 * for lines that produced tokens). */
static int end_of_line(void)
{
    int had_tokens = L->line_has_tokens;
    L->lines_seen++;
    L->line++;
    L->column = 1;
    L->line_has_tokens = 0;
    L->paren_depth = 0;
    yylval = NULL;
    return had_tokens;
}

void lexer_begin(FILE *input, ParserContext *ctx)
{
    L = ctx;
    yyrestart(input);
}

void lexer_end(void)
{
    yylex_destroy();
    L = NULL;
}
