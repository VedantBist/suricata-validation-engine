/* Suricata rule parser — Phase 3 grammar (see docs/GRAMMAR.md).
 *
 * Scope: header-only rules — Action Protocol SrcIP SrcPort Direction DstIP
 * DstPort, terminated by EOL. Options join in Phase 5; semantic validation
 * of values is Phase 6 and never happens here.
 *
 * Contracts this file owns:
 *   - RECOVERY: the `error TOK_EOL` production is the panic-mode backbone.
 *     A malformed line discards tokens up to its EOL (destructors free their
 *     values), records exactly one syntax diagnostic, resets error state
 *     (yyerrok) and the progress cursor, and parsing resumes at the next
 *     line. Parsing never terminates on malformed input.
 *   - DIAGNOSTICS: %define parse.error custom routes syntax errors through
 *     yyreport_syntax_error, which reads the *parser tables* (expected token
 *     set via yypcontext, exact thanks to parse.lac full) — Expected/Found
 *     is never guessed from the input. This file classifies the expected
 *     set structurally; all prose lives in diagnostics/.
 *   - OWNERSHIP: grammar actions build the Rule; dispatch transfers it to
 *     core, which frees it (streaming — nothing accumulates here).
 *   - ZERO CONFLICTS: bison runs with -Werror; any conflict fails the build.
 *
 * The parser never prints and never judges values.
 */

%code requires {
    #include "models/option.h"
    #include "models/rule.h"
    #include "models/source_location.h"
    typedef struct ParserContext ParserContext;
}

%code {
    #include <stdio.h>
    #include <stdlib.h>

    #include "core/dispatch.h"
    #include "diagnostics/diagnostics.h"
    #include "models/context.h"

    int yylex(void);
    static void yyerror(ParserContext *ctx, const char *msg);
    /* yyreport_syntax_error is forward-declared by bison itself (after the
     * yypcontext_t typedef); we only provide the definition in the epilogue. */

    /* Default location propagation plus this project's extra channel:
     * rule_number must flow to reduced nonterminals (bison's built-in
     * default only knows the four standard fields). */
    #define YYLLOC_DEFAULT(Cur, Rhs, N)                                  \
        do {                                                             \
            if (N) {                                                     \
                (Cur).first_line   = YYRHSLOC(Rhs, 1).first_line;        \
                (Cur).first_column = YYRHSLOC(Rhs, 1).first_column;      \
                (Cur).last_line    = YYRHSLOC(Rhs, N).last_line;         \
                (Cur).last_column  = YYRHSLOC(Rhs, N).last_column;       \
                (Cur).rule_number  = YYRHSLOC(Rhs, 1).rule_number;       \
            } else {                                                     \
                (Cur) = YYRHSLOC(Rhs, 0);                                \
            }                                                            \
        } while (0)
}

%union {
    char *str;          /* lexeme of value-bearing tokens (heap-owned) */
    int discrim;        /* RuleAction / RuleProtocol / RuleDirection */
    Endpoint endpoint;
    PortSpec port;
    OptionValue optval; /* option value under reduction */
    Option *opt;
    OptionList *optlist;
    Rule *rule;
}

%define parse.error custom
%define parse.lac full
%locations
%define api.location.type {SrcSpan}
%parse-param {ParserContext *ctx}

/* ---- Token inventory (owned here; lexer and tooling include the
 *      generated enum — no hand-written token ids exist anywhere) ---- */

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
%token <str> TOK_IP          "IP address"
%token <str> TOK_CIDR        "CIDR block"
%token <str> TOK_PORT        "port"
%token <str> TOK_NUMBER      "number"
%token <str> TOK_VARIABLE    "variable"
%token <str> TOK_STRING      "string"
%token <str> TOK_OPTION_KEY  "option keyword"
%token <str> TOK_IDENT       "identifier"

/* Structural */
%token TOK_EOL "end of line"

/* Lexical-error carrier: unclassifiable input reaches the parser as a real
 * token so diagnostics report `Found: invalid token "@"` from genuine
 * parser state instead of losing the information in the lexer. */
%token <str> TOK_INVALID "invalid token"

%type <discrim>  action protocol direction
%type <endpoint> address src_address dst_address
%type <port>     port_spec src_port dst_port
%type <optval>   option_value
%type <opt>      option
%type <optlist>  options option_list
%type <rule>     rule header

/* Memory discipline for panic-mode recovery: every semantic value bison
 * discards while skipping to EOL must be released here, or the leak checker
 * flags every malformed rule. Each ownership level frees exactly what it
 * owns (lexeme -> Option -> OptionList -> Rule), so whichever level sits on
 * the stack when recovery pops it is released exactly once — no leaks, no
 * double frees, regardless of how deep the recursive reduction got. */
%destructor { free($$); }              <str>
%destructor { free($$.text); }         <endpoint> <port> <optval>
%destructor { option_free($$); }       <opt>
%destructor { option_list_free($$); }  <optlist>
%destructor { rule_free($$); }         <rule>

%%

rule_file:
      %empty
    | rule_file line
    ;

/* One line, one verdict. Both productions end at TOK_EOL — the recovery
 * anchor the lexer guarantees for every token-bearing line (synthetic at
 * EOF). Recovery can therefore never cross a line boundary: lexer and
 * parser resynchronize at exactly the same point. */
line:
      rule TOK_EOL
        {
            ctx->progress = PROGRESS_LINE_START;
            dispatch_rule_accepted(ctx, $1);
        }
    | error TOK_EOL
        {
            /* The syntax diagnostic was already recorded by
             * yyreport_syntax_error at the moment of the error; this action
             * only accounts for the failed rule and re-arms the parser.
             * yyerrok immediately: without it bison would suppress error
             * reports until 3 tokens shift, swallowing an early error on
             * the very next line. */
            ctx->progress = PROGRESS_LINE_START;
            dispatch_rule_rejected(ctx, @2.rule_number, @2.first_line);
            yyerrok;
        }
    ;

/* Options are optional as two explicit productions (no empty nonterminal):
 * after `action header` the lookahead decides — EOL reduces the bare rule,
 * '(' shifts into the options block. LALR-disjoint, zero conflicts. */
rule:
      action header
        {
            $$ = $2;
            $$->action = (RuleAction)$1;
            $$->span = @$;
        }
    | action header options
        {
            $$ = $2;
            $$->action = (RuleAction)$1;
            $$->options = $3;
            $$->span = @$;
        }
    ;

/* header carries the Rule under construction (it owns six of the seven
 * fields); `rule` completes it with the action. Kept as its own nonterminal
 * so the grammar reads like the specification. */
header:
      protocol src_address src_port direction dst_address dst_port
        {
            $$ = rule_create();
            $$->protocol = (RuleProtocol)$1;
            $$->src_ip = $2;
            $$->src_port = $3;
            $$->direction = (RuleDirection)$4;
            $$->dst_ip = $5;
            $$->dst_port = $6;
        }
    ;

action:
      TOK_ALERT  { $$ = ACTION_ALERT; ctx->progress = PROGRESS_ACTION; }
    | TOK_DROP   { $$ = ACTION_DROP;  ctx->progress = PROGRESS_ACTION; }
    | TOK_PASS   { $$ = ACTION_PASS;  ctx->progress = PROGRESS_ACTION; }
    ;

protocol:
      TOK_TCP    { $$ = PROTO_TCP;  ctx->progress = PROGRESS_PROTOCOL; }
    | TOK_UDP    { $$ = PROTO_UDP;  ctx->progress = PROGRESS_PROTOCOL; }
    | TOK_ICMP   { $$ = PROTO_ICMP; ctx->progress = PROGRESS_PROTOCOL; }
    ;

direction:
      TOK_ARROW  { $$ = DIRECTION_TO;    ctx->progress = PROGRESS_DIRECTION; }
    | TOK_BIDIR  { $$ = DIRECTION_BIDIR; ctx->progress = PROGRESS_DIRECTION; }
    ;

/* src_/dst_ wrappers exist solely to advance the progress cursor with
 * positional knowledge; the token shapes are shared via address/port_spec.
 * Unit productions in fixed sequence — no conflicts. */
src_address:
      address    { $$ = $1; ctx->progress = PROGRESS_SRC_IP; }
    ;

dst_address:
      address    { $$ = $1; ctx->progress = PROGRESS_DST_IP; }
    ;

src_port:
      port_spec  { $$ = $1; ctx->progress = PROGRESS_SRC_PORT; }
    ;

dst_port:
      port_spec  { $$ = $1; ctx->progress = PROGRESS_DST_PORT; }
    ;

address:
      TOK_ANY       { $$.kind = ENDPOINT_ANY;      $$.text = NULL; }
    | TOK_IP        { $$.kind = ENDPOINT_IP;       $$.text = $1;   }
    | TOK_CIDR      { $$.kind = ENDPOINT_CIDR;     $$.text = $1;   }
    | TOK_VARIABLE  { $$.kind = ENDPOINT_VARIABLE; $$.text = $1;   }
    ;

port_spec:
      TOK_ANY       { $$.kind = PORT_ANY;    $$.text = NULL; }
    | TOK_PORT      { $$.kind = PORT_NUMBER; $$.text = $1;   }
    ;

/* ---- Options block (Phase 4) --------------------------------------------
 * Left-recursive list: each option reduces and appends as soon as its ';'
 * arrives, so the parser stack stays flat no matter how many options a rule
 * has — recursion in the grammar, not in memory. Recovery stays line-level
 * (`error EOL`): a malformed option invalidates its rule as a unit, which
 * is what keeps the one-diagnostic-per-line policy intact; the recovery
 * anchor is unchanged from Phase 3. */

options:
      TOK_LPAREN option_list TOK_RPAREN   { $$ = $2; }
    ;

option_list:
      option
        {
            $$ = option_list_create();
            option_list_append($$, $1);
        }
    | option_list option
        {
            option_list_append($1, $2);
            $$ = $1;
        }
    ;

option:
      TOK_OPTION_KEY TOK_COLON option_value TOK_SEMICOLON
        {
            /* takes ownership of the key lexeme and the value text */
            $$ = option_create($1, $3.kind, $3.text, @$);
        }
    ;

option_value:
      TOK_STRING    { $$.kind = OPTVAL_STRING; $$.text = $1; }
    | TOK_NUMBER    { $$.kind = OPTVAL_NUMBER; $$.text = $1; }
    | TOK_IDENT     { $$.kind = OPTVAL_IDENT;  $$.text = $1; }
    ;

%%

/* Custom syntax-error report: the single place parser state becomes a
 * diagnostic. Reads the exact expected-token set from the parser tables
 * (LAC makes it precise), classifies it structurally, and hands everything
 * to diagnostics/ — which owns all wording. Returns 0: the error was
 * consumed; recovery proceeds via `error TOK_EOL`. */
static int yyreport_syntax_error(const yypcontext_t *yyctx, ParserContext *ctx)
{
    SrcSpan span = *yypcontext_location(yyctx);

    enum { MAX_EXPECTED = 12 };
    yysymbol_kind_t expected[MAX_EXPECTED];
    int n = yypcontext_expected_tokens(yyctx, expected, MAX_EXPECTED);
    if (n < 0) {
        n = 0;
    }

    /* Structural classification of the expected set: collect which token
     * families the tables allow, then decide. Header families are mutually
     * exclusive per state; the options region introduces the two genuinely
     * mixed sets ({OPTION_KEY, ')'} after a completed option and
     * {'(', EOL} after the destination port), which get their own classes.
     * TOK_ANY is deliberately ignored — it appears in both the address and
     * port sets; the specific members decide. */
    struct {
        unsigned action : 1, protocol : 1, address : 1, port : 1, dir : 1;
        unsigned lparen : 1, rparen : 1, colon : 1, semi : 1;
        unsigned optkey : 1, value : 1, eol : 1;
    } saw = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    for (int i = 0; i < n; i++) {
        switch (expected[i]) {
        case YYSYMBOL_TOK_ALERT:
        case YYSYMBOL_TOK_DROP:
        case YYSYMBOL_TOK_PASS:       saw.action = 1;   break;
        case YYSYMBOL_TOK_TCP:
        case YYSYMBOL_TOK_UDP:
        case YYSYMBOL_TOK_ICMP:       saw.protocol = 1; break;
        case YYSYMBOL_TOK_IP:
        case YYSYMBOL_TOK_CIDR:
        case YYSYMBOL_TOK_VARIABLE:   saw.address = 1;  break;
        case YYSYMBOL_TOK_PORT:       saw.port = 1;     break;
        case YYSYMBOL_TOK_ARROW:
        case YYSYMBOL_TOK_BIDIR:      saw.dir = 1;      break;
        case YYSYMBOL_TOK_LPAREN:     saw.lparen = 1;   break;
        case YYSYMBOL_TOK_RPAREN:     saw.rparen = 1;   break;
        case YYSYMBOL_TOK_COLON:      saw.colon = 1;    break;
        case YYSYMBOL_TOK_SEMICOLON:  saw.semi = 1;     break;
        case YYSYMBOL_TOK_OPTION_KEY: saw.optkey = 1;   break;
        case YYSYMBOL_TOK_STRING:
        case YYSYMBOL_TOK_NUMBER:
        case YYSYMBOL_TOK_IDENT:      saw.value = 1;    break;
        case YYSYMBOL_TOK_EOL:        saw.eol = 1;      break;
        default:                                        break;
        }
    }

    ExpectedClass klass =
        saw.action                ? EXPECT_ACTION
        : saw.protocol            ? EXPECT_PROTOCOL
        : saw.address             ? EXPECT_ADDRESS
        : saw.port                ? EXPECT_PORT
        : saw.dir                 ? EXPECT_DIRECTION
        : saw.colon               ? EXPECT_COLON
        : saw.value               ? EXPECT_OPTION_VALUE
        : saw.semi                ? EXPECT_SEMICOLON
        : saw.optkey && saw.rparen ? EXPECT_OPTION_OR_CLOSE
        : saw.optkey              ? EXPECT_OPTION_KEY
        : saw.lparen && saw.eol   ? EXPECT_OPTIONS_OR_END
        : saw.eol                 ? EXPECT_END_OF_RULE
                                  : EXPECT_OTHER;

    const char *expected_names[MAX_EXPECTED];
    for (int i = 0; i < n; i++) {
        expected_names[i] = yysymbol_name(expected[i]);
    }

    yysymbol_kind_t lookahead = yypcontext_token(yyctx);
    const char *found_symbol =
        lookahead == YYSYMBOL_YYEMPTY ? NULL : yysymbol_name(lookahead);

    diag_syntax_error(&ctx->diagnostics, span, ctx->progress, klass,
                      expected_names, n, found_symbol, ctx->found_text);
    return 0;
}

/* With parse.error custom, bison only calls yyerror for non-syntax
 * failures (e.g. "memory exhausted"). Engine-level, not input-level. */
static void yyerror(ParserContext *ctx, const char *msg)
{
    (void)ctx;
    fprintf(stderr, "parser: %s\n", msg);
}
